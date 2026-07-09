/*
 * Pact MP-1 - memory placement attributes.
 *
 * On the H7, DMA1/2 and SDMMC IDMA cannot reach DTCM (where .bss lives by
 * default), so anything a DMA engine touches must be placed explicitly:
 *   PACT_AXI  - big CPU-side pools (LVGL heap, decode scratch), 512K AXI
 *   PACT_D2   - DMA buffers (PCM ring, SDMMC, LVGL draw buffers), D2 SRAM,
 *               32-byte aligned for cache maintenance; the MPU marks this
 *               region non-cacheable at bring-up
 * On the host build both are no-ops.
 */
#pragma once

#ifdef PACT_SIM
    #define PACT_AXI
    #define PACT_D2
#else
    #define PACT_AXI __attribute__((section(".axi_bss")))
    #define PACT_D2  __attribute__((section(".d2_bss"), aligned(32)))
#endif
