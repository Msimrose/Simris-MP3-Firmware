/*
 * Pact MP-1 - TinyUSB configuration (device-only MSC over OTG_HS + ULPI).
 *
 * RHPort 1 = OTG_HS with the external USB3343 ULPI PHY at 480 Mbit/s.
 * Slave (FIFO) mode for bring-up - the CPU copies packets, good for
 * ~10-15 MB/s; the DWC2 internal-DMA + D2 buffers upgrade is the path to
 * the ~24 MB/s eMMC-limited target (firmware-spec section 9).
 */
#pragma once

#define CFG_TUSB_MCU            OPT_MCU_STM32H7
#define CFG_TUSB_OS             OPT_OS_FREERTOS

/* CubeMX's FreeRTOS kernel predates pdTICKS_TO_MS (osal_freertos.h uses
 * it); expands lazily where FreeRTOS.h is already included. */
#ifndef pdTICKS_TO_MS
#define pdTICKS_TO_MS(xTicks) \
    ((uint32_t)(((uint64_t)(xTicks) * 1000u) / configTICK_RATE_HZ))
#endif

#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       OPT_MODE_HIGH_SPEED

#define CFG_TUSB_MEM_SECTION                       /* .bss (DTCM) is fine:
                                                      slave mode = CPU copies */
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))

#define CFG_TUD_ENDPOINT0_SIZE  64

/* ---- class: MSC only ----------------------------------------------------- */
#define CFG_TUD_MSC             1
#define CFG_TUD_MSC_EP_BUFSIZE  8192    /* sector-multiple; bigger = faster */
