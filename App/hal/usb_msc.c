/*
 * Pact MP-1 - USB MSC device: eMMC + microSD as two raw LUNs (TinyUSB).
 *
 * Data path: MSC callbacks bridge straight onto the FatFs diskio block
 * layer (App/storage/diskio_sdmmc.c) - the host reads/writes raw sectors,
 * FatFs is unmounted for the whole session, so the two never fight over
 * a volume (firmware-spec section 9).
 *
 * Session FSM (pact_usb_task): VBUS (BQ24075 PG_STAT) attach -> stop
 * playback pop-free, unmount volumes, soft-connect. Detach -> soft
 * disconnect + reboot: the cleanest way to get a fresh scan + thumb
 * build + consistent UI after the host rearranged the disks (iPods did
 * the same after sync). A live rescan without reboot is a later polish.
 *
 * The generated OTG_HS IRQ handler is spliced to tud_int_handler in
 * stm32h7xx_it.c USER CODE (HAL_PCD_IRQHandler never runs); the CubeMX
 * PCD init still does the useful part - ULPI pin mux + clock enables -
 * before TinyUSB's dcd_init soft-resets and reprograms the core.
 *
 * ⚠ VID/PID are the TinyUSB test values (0xCafe) - register real IDs
 * (e.g. pid.codes) before any unit is sold or distributed.
 */
#ifndef PACT_SIM

#include "hal/pact_usb.h"
#include "hal/pact_boot.h"
#include "audio/audio_engine.h"
#include "audio/audio_out.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "diskio.h"
#include "tusb.h"

#include <stdio.h>
#include <string.h>

#define USB_RHPORT 1

static volatile bool usb_owns_disks;

bool pact_usb_active(void) { return usb_owns_disks; }

/* Called from OTG_HS_IRQHandler (tud_int_handler is a macro here). */
void pact_usb_irq(void)
{
    tud_int_handler(USB_RHPORT);
}

/* LUN -> FatFs physical drive: LUN0 = eMMC (pdrv 1), LUN1 = microSD (0) */
static const BYTE lun_pdrv[2] = { 1, 0 };

/* ---- descriptors ----------------------------------------------------------- */

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,      /* TODO: real VID/PID before sale */
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

enum { ITF_NUM_MSC = 0, ITF_NUM_TOTAL };
#define EPNUM_MSC_OUT  0x01
#define EPNUM_MSC_IN   0x81
#define CONFIG_LEN     (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

static const uint8_t desc_cfg_hs[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_LEN, 0, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),
};
static const uint8_t desc_cfg_fs[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_LEN, 0, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return tud_speed_get() == TUSB_SPEED_HIGH ? desc_cfg_hs : desc_cfg_fs;
}

/* HS devices must answer device_qualifier / other_speed requests */
static const tusb_desc_device_qualifier_t desc_qualifier = {
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 1,
    .bReserved          = 0,
};

uint8_t const *tud_descriptor_device_qualifier_cb(void)
{
    return (uint8_t const *)&desc_qualifier;
}

uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
    (void)index;   /* mirror of the other speed's config */
    return tud_speed_get() == TUSB_SPEED_HIGH ? desc_cfg_fs : desc_cfg_hs;
}

static const char *desc_strings[] = {
    NULL,                    /* 0: language (special-cased below) */
    "Simris",                /* 1 */
    "Pact MP-1",             /* 2 */
    NULL,                    /* 3: serial from MCU UID */
};

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    static uint16_t str[34];
    size_t n;

    if (index == 0) {
        str[1] = 0x0409;     /* English (US) */
        n = 1;
    } else if (index == 3) {
        char s[27];
        snprintf(s, sizeof s, "%08lX%08lX%08lX",
                 (unsigned long)HAL_GetUIDw2(), (unsigned long)HAL_GetUIDw1(),
                 (unsigned long)HAL_GetUIDw0());
        n = strlen(s);
        for (size_t i = 0; i < n; i++) str[1 + i] = s[i];
    } else if (index < TU_ARRAY_SIZE(desc_strings) && desc_strings[index]) {
        const char *s = desc_strings[index];
        n = strlen(s);
        if (n > 32) n = 32;
        for (size_t i = 0; i < n; i++) str[1 + i] = s[i];
    } else {
        return NULL;
    }
    str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
    return str;
}

/* ---- MSC callbacks ---------------------------------------------------------- */

uint8_t tud_msc_get_maxlun_cb(void)
{
    return 2;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4])
{
    memcpy(vendor_id, "Simris  ", 8);
    memcpy(product_id, lun == 0 ? "Pact MP-1 eMMC  "
                                : "Pact MP-1 SD    ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    if (lun >= 2 || !usb_owns_disks) return false;
    if (disk_status(lun_pdrv[lun]) & STA_NOINIT) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return false;   /* medium not present (e.g. no card) */
    }
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size)
{
    LBA_t count = 0;
    if (lun < 2) disk_ioctl(lun_pdrv[lun], GET_SECTOR_COUNT, &count);
    *block_count = (uint32_t)count;
    *block_size  = 512;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject)
{
    (void)lun; (void)power_condition; (void)start; (void)load_eject;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize)
{
    if (lun >= 2 || !usb_owns_disks) return -1;
    if ((offset % 512) || (bufsize % 512)) return -1;
    uint32_t sect = lba + offset / 512;
    UINT     cnt  = bufsize / 512;
    if (disk_read(lun_pdrv[lun], buffer, sect, cnt) != RES_OK) return -1;
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize)
{
    if (lun >= 2 || !usb_owns_disks) return -1;
    if ((offset % 512) || (bufsize % 512)) return -1;
    uint32_t sect = lba + offset / 512;
    UINT     cnt  = bufsize / 512;
    if (disk_write(lun_pdrv[lun], buffer, sect, cnt) != RES_OK) return -1;
    return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
    return lun < 2;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer,
                        uint16_t bufsize)
{
    (void)lun; (void)scsi_cmd; (void)buffer; (void)bufsize;
    return -1;   /* unsupported command -> stall per spec */
}

/* ---- session task ------------------------------------------------------------ */

/* PG_STAT (BQ24075 power-good, open-drain active low) = VBUS present */
static bool vbus_present(void)
{
    return HAL_GPIO_ReadPin(PG_STAT_GPIO_Port, PG_STAT_Pin) == GPIO_PIN_RESET;
}

void pact_usb_task(void *arg)
{
    (void)arg;

    /* let the first-boot scan/thumb build finish before offering disks */
    while (!pact_boot_library_ready())
        vTaskDelay(pdMS_TO_TICKS(100));

    tusb_rhport_init_t dev_init = {
        .role  = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_HIGH,
    };
    if (!tusb_init(USB_RHPORT, &dev_init)) {
        for (;;) vTaskDelay(portMAX_DELAY);
    }
    tud_disconnect();               /* stay invisible until we own the disks */

    bool session = false;
    for (;;) {
        bool vbus = vbus_present();
        if (vbus && !session) {
            audio_engine_stop();
            audio_out_stop();       /* pop-free amp/DAC down */
            f_unmount("0:");
            f_unmount("1:");
            usb_owns_disks = true;
            tud_connect();
            session = true;
        } else if (!vbus && session) {
            tud_disconnect();
            usb_owns_disks = false;
            /* Reboot for a coherent rescan + thumb top-up + fresh UI
             * (iPod-style post-sync restart; live rescan = later polish) */
            vTaskDelay(pdMS_TO_TICKS(250));
            NVIC_SystemReset();
        }
        tud_task_ext(10, false);    /* service USB; 10 ms VBUS poll cadence */
    }
}

#endif /* !PACT_SIM */
