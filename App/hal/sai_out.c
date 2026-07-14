/*
 * Pact MP-1 - SAI output driver: the device ring consumer.
 *
 * pcm_ring -> DMA1_Stream1 (circular, half/complete callbacks) -> SAI1
 * Block A (I2S master TX, 32-bit, no MCLK) -> PCM5102A -> OPA1622.
 *
 * Clocking: SAI1 kernel mux switches between PLL2P (11.2896 MHz, 44.1k
 * family) and PLL3P (12.288 MHz, 48k family) per track. PLL2 also clocks
 * the ADC and is NEVER reconfigured here - mux-only. PLL3 has no
 * CubeMX-generated code, so it is brought up on first 48k-family use and
 * left running; every later family switch is mux-only. With NODIV=1,
 * SCK = kernel/MCKDIV and fs = SCK/64 (frame = 2 x 32-bit slots), an exact
 * integer division for every rate in both families (8k..192k).
 *
 * Pop-free sequencing (firmware-spec section 7):
 *   start: PWR5V_EN -> settle -> BCK running (zeros) -> XSMT high -> ramp
 *          -> AMP_EN
 *   stop:  AMP_EN low -> XSMT low -> soft-mute ramp -> DMA stop ->
 *          PWR5V_EN low
 *
 * The DMA buffer lives in .d2_bss (DMA1 cannot reach DTCM). The MPU marks
 * D2 non-cacheable at boot wiring; until that lands each refilled half is
 * also explicitly cache-cleaned, so the driver is correct either way.
 * On ring underrun a half is zero-padded - silence, never stale samples.
 */
#ifndef PACT_SIM

#include "audio/audio_out.h"
#include "pact_mem.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define OUT_HALF_FRAMES  1024u                    /* 8 KB half; 23 ms @44.1k */
#define OUT_TOTAL_FRAMES (2u * OUT_HALF_FRAMES)

#define OUT_5V_SETTLE_MS  20u  /* TPS65133 soft-start to a solid +/-5V */
#define OUT_CLK_SETTLE_MS 15u  /* PCM5102A BCK detect + internal PLL lock */
#define OUT_RAMP_MS       15u  /* XSMT soft un/mute = 104/fs, 13 ms worst @8k */
#define OUT_AMP_OFF_MS     2u  /* OPA1622 disable before touching the DAC */

extern SAI_HandleTypeDef hsai_BlockA1;

PACT_D2 static int32_t out_buf[OUT_TOTAL_FRAMES * 2];

static struct {
    pcm_ring_t       *ring;
    void            (*wakeup)(void);
    uint32_t          rate;      /* 0 = output path closed */
    bool              rails_on;
    bool              pll3_on;
    volatile uint32_t underruns;
    volatile uint32_t errors;
} out;

static void out_delay(uint32_t ms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskDelay(pdMS_TO_TICKS(ms));
    else
        HAL_Delay(ms);
}

static void cache_clean(const void *addr, size_t bytes)
{
    SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)addr, (int32_t)bytes);
}

/* ---- ISR side: keep the circular buffer fed from the ring --------------- */

static void refill(unsigned half)
{
    int32_t *dst = &out_buf[half * OUT_HALF_FRAMES * 2];
    size_t   got = pcm_ring_read(out.ring, dst, OUT_HALF_FRAMES);
    if (got < OUT_HALF_FRAMES) {
        memset(dst + got * 2, 0, (OUT_HALF_FRAMES - got) * 2 * sizeof(int32_t));
        out.underruns++;
    }
    cache_clean(dst, OUT_HALF_FRAMES * 2 * sizeof(int32_t));
    if (out.wakeup) out.wakeup();
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI1_Block_A) refill(0);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI1_Block_A) refill(1);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI1_Block_A) out.errors++;
}

/* ---- clocking ------------------------------------------------------------ */

static bool sai_kernel_select(bool family48)
{
    if (!family48) {
        __HAL_RCC_SAI1_CONFIG(RCC_SAI1CLKSOURCE_PLL2);
        return true;
    }
    if (out.pll3_on) {
        __HAL_RCC_SAI1_CONFIG(RCC_SAI1CLKSOURCE_PLL3);
        return true;
    }
    /* The values CubeMX solved for the clock tree but does not generate:
     * 25 MHz HSE /25 x (393 + 1769/8192) /32 = 12.288 MHz, -0.15 ppm. */
    RCC_PeriphCLKInitTypeDef k = {0};
    k.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
    k.PLL3.PLL3M = 25;
    k.PLL3.PLL3N = 393;
    k.PLL3.PLL3FRACN = 1769;
    k.PLL3.PLL3P = 32;
    k.PLL3.PLL3Q = 2;
    k.PLL3.PLL3R = 2;
    k.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
    k.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    k.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
    if (HAL_RCCEx_PeriphCLKConfig(&k) != HAL_OK)
        return false;
    out.pll3_on = true;
    return true;
}

/* ---- output path (SAI + DMA); muted the whole time ----------------------- */

static bool output_open(uint32_t rate)
{
    uint32_t sck = rate * 64u;
    uint32_t kernel;
    bool     family48;

    if (sck != 0 && 12288000u % sck == 0u) { kernel = 12288000u; family48 = true;  }
    else if (sck != 0 && 11289600u % sck == 0u) { kernel = 11289600u; family48 = false; }
    else return false;

    uint32_t div = kernel / sck;
    if (div > 63u)
        return false;

    HAL_SAI_DeInit(&hsai_BlockA1);          /* also releases the DMA stream */
    if (!sai_kernel_select(family48))
        return false;

    /* CubeMX Init fields carry over; only the rate-dependent ones change */
    hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_MCKDIV;
    hsai_BlockA1.Init.Mckdiv         = div;
    hsai_BlockA1.Init.NoDivider      = SAI_MASTERDIVIDER_DISABLE; /* NODIV=1 */
    hsai_BlockA1.Init.FIFOThreshold  = SAI_FIFOTHRESHOLD_1QF;
    if (HAL_SAI_InitProtocol(&hsai_BlockA1, SAI_I2S_STANDARD,
                             SAI_PROTOCOL_DATASIZE_32BIT, 2) != HAL_OK)
        return false;

    memset(out_buf, 0, sizeof out_buf);
    cache_clean(out_buf, sizeof out_buf);
    if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)out_buf,
                             OUT_TOTAL_FRAMES * 2u) != HAL_OK)
        return false;

    out.rate = rate;
    return true;
}

static void output_close(void)
{
    HAL_GPIO_WritePin(AMP_EN_GPIO_Port, AMP_EN_Pin, GPIO_PIN_RESET);
    out_delay(OUT_AMP_OFF_MS);
    HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_RESET);
    out_delay(OUT_RAMP_MS);
    HAL_SAI_DMAStop(&hsai_BlockA1);
    out.rate = 0;
}

/* ---- public API ----------------------------------------------------------- */

void audio_out_init(pcm_ring_t *ring)
{
    out.ring = ring;
}

bool audio_out_start(uint32_t sample_rate)
{
    if (!out.ring || sample_rate == 0u)
        return false;
    if (out.rate == sample_rate)
        return true;

    if (out.rate)
        output_close();                /* rate switch: mute, rails stay up */

    if (!out.rails_on) {
        HAL_GPIO_WritePin(PWR5V_EN_GPIO_Port, PWR5V_EN_Pin, GPIO_PIN_SET);
        out_delay(OUT_5V_SETTLE_MS);
        out.rails_on = true;
    }

    if (!output_open(sample_rate))
        return false;                  /* everything still muted/off */

    out_delay(OUT_CLK_SETTLE_MS);      /* BCK carries zeros while DAC locks */
    HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_SET);
    out_delay(OUT_RAMP_MS);
    HAL_GPIO_WritePin(AMP_EN_GPIO_Port, AMP_EN_Pin, GPIO_PIN_SET);
    return true;
}

void audio_out_stop(void)
{
    if (out.rate)
        output_close();
    if (out.rails_on) {
        HAL_GPIO_WritePin(PWR5V_EN_GPIO_Port, PWR5V_EN_Pin, GPIO_PIN_RESET);
        out.rails_on = false;
    }
}

bool     audio_out_running(void)     { return out.rate != 0u; }
uint32_t audio_out_sample_rate(void) { return out.rate; }
uint32_t audio_out_underruns(void)   { return out.underruns; }
uint32_t audio_out_errors(void)      { return out.errors; }

void audio_out_set_wakeup(void (*hook)(void)) { out.wakeup = hook; }

#endif /* !PACT_SIM */
