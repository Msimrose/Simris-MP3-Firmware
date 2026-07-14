/*
 * Pact MP-1 - device boot: the FreeRTOS task wiring (firmware-spec
 * section 10). Replaces the old link probe with the real startup path.
 *
 * Tasks created here (CMSIS-RTOS2, stacks from the RTOS heap in DTCM):
 *   audio_task   High    decode pump -> PCM ring; woken by the SAI DMA
 *                        half/complete wakeup hook (the only real-time path)
 *   ui_task      Normal  LVGL init + render loop; idles until the display
 *                        driver (backend #2) sets pact_display_ready
 *   storage_task Low     mounts eMMC + microSD; library scan lands here
 *                        (backend #3)
 * input/power/led tasks arrive with the input/power HAL (backend #5).
 *
 * The PCM ring lives in .d2_bss: DMA-reachable, inside the MPU
 * non-cacheable D2 region configured in main() USER CODE 1. Keep total
 * .d2_bss under 256 KB (SRAM1+SRAM2) - the MPU region does not cover
 * D2 SRAM3 at 0x30040000.
 */
#ifndef PACT_SIM

#include "pact_boot.h"
#include "pact_display.h"
#include "pact_mem.h"
#include "audio/audio_engine.h"
#include "audio/audio_out.h"
#include "audio/pcm_ring.h"
#include "library/library.h"
#include "storage/storage.h"
#include "ui/ui.h"

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"

/* ~370 ms of decode-ahead at 44.1k (85 ms at 192k), matching the sim. */
#define PACT_RING_FRAMES 16384u

PACT_D2 static int32_t ring_storage[PACT_RING_FRAMES * 2];
static pcm_ring_t ring;

static TaskHandle_t audio_task_handle;

/* The library: scanned and owned by storage_task, consumed by ui_task
 * once lib_ready flips. */
static library_t lib;
static volatile bool lib_ready;

/* ---- audio task ---------------------------------------------------------- */

/* ISR-context hook: a DMA half was consumed, wake the pump. */
static void audio_wakeup_isr(void)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(audio_task_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

static void audio_task_fn(void *arg)
{
    (void)arg;
    audio_task_handle = xTaskGetCurrentTaskHandle();
    pcm_ring_init(&ring, ring_storage, PACT_RING_FRAMES);
    audio_engine_init(&ring);
    audio_out_init(&ring);
    audio_out_set_wakeup(audio_wakeup_isr);
    for (;;) {
        while (audio_engine_pump()) {}
        /* Woken by the SAI ISR; the timeout keeps the pump responsive to
         * engine state changes (play/seek) that don't notify. */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
    }
}

/* Device play path - the UI's on_play seam (sim/main.c wires sim_play
 * here). Engine first so the ring starts filling, then the output at the
 * track's native rate; first start carries ~55 ms of pop-free sequencing. */
static void device_play(const char *path)
{
    if (!audio_engine_play(path))
        return;
    const audio_fmt_t *fmt = audio_engine_fmt();
    if (fmt)
        audio_out_start(fmt->sample_rate);
    if (audio_task_handle)
        xTaskNotifyGive(audio_task_handle);
}

/* ---- ui task ------------------------------------------------------------- */

static void ui_task_fn(void *arg)
{
    (void)arg;
    lv_init();
    lv_tick_set_cb(HAL_GetTick);

    /* Panel rails + official Startek init + LVGL display registration.
     * QSPI writes are blind (no readback), so failure here means the QSPI
     * peripheral itself; park rather than render into nothing. */
    if (!pact_display_init()) {
        for (;;)
            vTaskDelay(portMAX_DELAY);
    }

    while (!lib_ready)                  /* storage_task is scanning */
        vTaskDelay(pdMS_TO_TICKS(50));

    ui_set_library(&lib, NULL);         /* TODO: art provider = thumb cache */
    ui_set_on_play(device_play);
    ui_init();
    for (;;) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ---- storage task -------------------------------------------------------- */

static void storage_task_fn(void *arg)
{
    (void)arg;
    storage_status_t st = storage_mount_all();

    /* Music home = eMMC ("1:"); fall back to the card. Merging both
     * volumes into one library is a later step. An empty or missing
     * volume publishes an empty library; no storage at all leaves the
     * UI waiting (bring-up bench state - debug over SWD/UART). */
    const char *root = st.emmc_mounted ? "1:" : (st.sd_mounted ? "0:" : NULL);
    if (root) {
        library_scan(&lib, root);
        if (st.emmc_mounted)
            library_save(&lib, "1:/pact.idx");  /* fast-boot cache, later */
        lib_ready = true;
    }

    /* TODO: rescan on SD insert (SD_CD EXTI) and after USB MSC detach. */
    for (;;)
        vTaskDelay(portMAX_DELAY);
}

/* ---- boot ----------------------------------------------------------------- */

void pact_boot_create_tasks(void)
{
    static const osThreadAttr_t audio_attr = {
        .name = "audio", .stack_size = 8192, .priority = osPriorityHigh,
    };
    static const osThreadAttr_t ui_attr = {
        .name = "ui", .stack_size = 8192, .priority = osPriorityNormal,
    };
    static const osThreadAttr_t storage_attr = {
        .name = "storage", .stack_size = 6144, .priority = osPriorityLow,
    };
    (void)osThreadNew(audio_task_fn, NULL, &audio_attr);
    (void)osThreadNew(ui_task_fn, NULL, &ui_attr);
    (void)osThreadNew(storage_task_fn, NULL, &storage_attr);
}

#endif /* !PACT_SIM */
