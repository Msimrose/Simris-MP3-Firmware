/*
 * Pact MP-1 - FatFs disk IO over the H7 SDMMC peripherals.
 *   drive 0 ("0:") = microSD  on SDMMC1 (hsd1)
 *   drive 1 ("1:") = eMMC     on SDMMC2 (hmmc2)
 *
 * Polling transfers for bring-up simplicity and cache-safety; the move to
 * IDMA + MPU-non-cacheable buffers is a Phase 2 performance step once the
 * console is up (firmware-spec sections 6 and 12).
 */
#ifndef PACT_SIM

#include <stdbool.h>
#include "ff.h"
#include "diskio.h"
#include "stm32h7xx_hal.h"

extern SD_HandleTypeDef  hsd1;
extern MMC_HandleTypeDef hmmc2;

#define DRV_SD    0
#define DRV_EMMC  1
#define IO_TIMEOUT_MS 1000u

static DSTATUS drv_stat[2] = { STA_NOINIT, STA_NOINIT };

static bool sd_wait_ready(void)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
        if (HAL_GetTick() - t0 > IO_TIMEOUT_MS) return false;
    return true;
}

static bool mmc_wait_ready(void)
{
    uint32_t t0 = HAL_GetTick();
    while (HAL_MMC_GetCardState(&hmmc2) != HAL_MMC_CARD_TRANSFER)
        if (HAL_GetTick() - t0 > IO_TIMEOUT_MS) return false;
    return true;
}

DSTATUS disk_status(BYTE pdrv)
{
    return pdrv <= DRV_EMMC ? drv_stat[pdrv] : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    /* Peripheral init is CubeMX's MX_SDMMC*_Init at boot; here we only
     * confirm the medium answers. */
    switch (pdrv) {
    case DRV_SD:
        drv_stat[DRV_SD] = sd_wait_ready() ? 0 : STA_NOINIT;
        return drv_stat[DRV_SD];
    case DRV_EMMC:
        drv_stat[DRV_EMMC] = mmc_wait_ready() ? 0 : STA_NOINIT;
        return drv_stat[DRV_EMMC];
    default:
        return STA_NOINIT;
    }
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    switch (pdrv) {
    case DRV_SD:
        if (HAL_SD_ReadBlocks(&hsd1, buff, (uint32_t)sector, count,
                              IO_TIMEOUT_MS) != HAL_OK) return RES_ERROR;
        return sd_wait_ready() ? RES_OK : RES_ERROR;
    case DRV_EMMC:
        if (HAL_MMC_ReadBlocks(&hmmc2, buff, (uint32_t)sector, count,
                               IO_TIMEOUT_MS) != HAL_OK) return RES_ERROR;
        return mmc_wait_ready() ? RES_OK : RES_ERROR;
    default:
        return RES_PARERR;
    }
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    switch (pdrv) {
    case DRV_SD:
        if (HAL_SD_WriteBlocks(&hsd1, (uint8_t *)buff, (uint32_t)sector, count,
                               IO_TIMEOUT_MS) != HAL_OK) return RES_ERROR;
        return sd_wait_ready() ? RES_OK : RES_ERROR;
    case DRV_EMMC:
        if (HAL_MMC_WriteBlocks(&hmmc2, (uint8_t *)buff, (uint32_t)sector,
                                count, IO_TIMEOUT_MS) != HAL_OK)
            return RES_ERROR;
        return mmc_wait_ready() ? RES_OK : RES_ERROR;
    default:
        return RES_PARERR;
    }
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    HAL_SD_CardInfoTypeDef  si;
    HAL_MMC_CardInfoTypeDef mi;

    switch (cmd) {
    case CTRL_SYNC:
        return (pdrv == DRV_SD ? sd_wait_ready() : mmc_wait_ready())
                   ? RES_OK : RES_ERROR;

    case GET_SECTOR_COUNT:
        if (pdrv == DRV_SD) {
            if (HAL_SD_GetCardInfo(&hsd1, &si) != HAL_OK) return RES_ERROR;
            *(LBA_t *)buff = si.LogBlockNbr;
        } else {
            if (HAL_MMC_GetCardInfo(&hmmc2, &mi) != HAL_OK) return RES_ERROR;
            *(LBA_t *)buff = mi.LogBlockNbr;
        }
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 8;  /* 4 KB erase-aligned, conservative */
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

#endif /* !PACT_SIM */
