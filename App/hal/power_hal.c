/*
 * Pact MP-1 - power HAL: battery ADC + LiPo curve, charge pins, shutdown.
 *
 * ADC: BAT_SENSE = ADC1 channel 3 (PA6). CubeMX generated a 1.5-cycle
 * sampling time - far too short for a high-impedance divider (the flagged
 * preflight finding); the channel is reconfigured here with 810.5 cycles
 * (~72 us at the 11.29 MHz ADC kernel - nothing at a 1 Hz gauge). The ADC
 * is calibrated once at task start.
 *
 * ⚠ SCHEMATIC DEPENDENCY: the battery divider is NOT yet wired on the
 * schematic (preflight finding, still open). PACT_BAT_DIV below assumes a
 * 2:1 divider (e.g. 100k/100k) - set the real ratio when the divider is
 * drawn, and size it high-impedance + capacitor per the long sampling time.
 *
 * Charge pins: BQ24075 CHG_STAT/PG_STAT are open-drain, active LOW
 * (external pull-ups): low = charging / power-good.
 *
 * Percent: resting-voltage LiPo LUT with linear interpolation, median of
 * 5 samples. Good to ~5-10% honesty; a fuel gauge IC is decision D5 if
 * this proves annoying (charging voltage rise makes % optimistic on USB -
 * shown as "charging" anyway).
 */
#ifndef PACT_SIM

#include "hal/pact_power.h"
#include "hal/pact_display.h"
#include "audio/audio_out.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "storage/storage.h"

#define PACT_BAT_DIV_NUM 2u   /* vbat = vadc * NUM / DEN  (TODO: schematic) */
#define PACT_BAT_DIV_DEN 1u
#define VREF_MV          3300u

extern ADC_HandleTypeDef hadc1;

static volatile int  bat_percent = -1;
static volatile bool chg_active;
static volatile bool pg_active;

int  pact_battery_percent(void) { return bat_percent; }
bool pact_charging(void)        { return chg_active; }
bool pact_vbus_present(void)    { return pg_active; }

/* ---- ADC ------------------------------------------------------------------ */

static bool adc_setup(void)
{
    ADC_ChannelConfTypeDef c = {0};
    c.Channel      = ADC_CHANNEL_3;
    c.Rank         = ADC_REGULAR_RANK_1;
    c.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;   /* was 1.5: too short */
    c.SingleDiff   = ADC_SINGLE_ENDED;
    c.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(&hadc1, &c) != HAL_OK)
        return false;
    return HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET,
                                       ADC_SINGLE_ENDED) == HAL_OK;
}

static bool adc_read_mv(uint32_t *out_mv)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK) return false;
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return false;
    }
    uint32_t raw = HAL_ADC_GetValue(&hadc1);       /* 16-bit */
    HAL_ADC_Stop(&hadc1);
    uint32_t vadc = raw * VREF_MV / 65535u;
    *out_mv = vadc * PACT_BAT_DIV_NUM / PACT_BAT_DIV_DEN;
    return true;
}

/* ---- LiPo resting curve ----------------------------------------------------- */

static const struct { uint16_t mv; uint8_t pct; } lipo_lut[] = {
    { 4200, 100 }, { 4060, 90 }, { 3980, 80 }, { 3920, 70 }, { 3870, 60 },
    { 3820, 50 },  { 3790, 40 }, { 3770, 30 }, { 3740, 20 }, { 3680, 10 },
    { 3450, 5 },   { 3300, 0 },
};

static int mv_to_percent(uint32_t mv)
{
    size_t n = sizeof lipo_lut / sizeof lipo_lut[0];
    if (mv >= lipo_lut[0].mv)     return 100;
    if (mv <= lipo_lut[n - 1].mv) return 0;
    for (size_t i = 1; i < n; i++) {
        if (mv >= lipo_lut[i].mv) {
            uint32_t span_mv = lipo_lut[i - 1].mv - lipo_lut[i].mv;
            uint32_t span_pc = lipo_lut[i - 1].pct - lipo_lut[i].pct;
            return lipo_lut[i].pct
                   + (int)((mv - lipo_lut[i].mv) * span_pc / span_mv);
        }
    }
    return 0;
}

static uint32_t median5(uint32_t *v)
{
    /* insertion sort, 5 elements */
    for (int i = 1; i < 5; i++) {
        uint32_t x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    return v[2];
}

/* ---- shutdown ---------------------------------------------------------------- */

void pact_power_shutdown(void)
{
    audio_out_stop();                       /* amp off, mute, rails down */
    storage_unmount_all();
    pact_display_off();
    HAL_GPIO_WritePin(PWR_HOLD_GPIO_Port, PWR_HOLD_Pin, GPIO_PIN_RESET);
    /* Latch drops the rail here. On a bench supply (latch not wired yet)
     * we just park. */
    for (;;)
        vTaskDelay(portMAX_DELAY);
}

/* ---- task ---------------------------------------------------------------------- */

void pact_power_task(void *arg)
{
    (void)arg;
    bool adc_ok = adc_setup();

    for (;;) {
        /* open-drain status pins, active low */
        chg_active = HAL_GPIO_ReadPin(CHG_STAT_GPIO_Port, CHG_STAT_Pin)
                     == GPIO_PIN_RESET;
        pg_active  = HAL_GPIO_ReadPin(PG_STAT_GPIO_Port, PG_STAT_Pin)
                     == GPIO_PIN_RESET;

        if (adc_ok) {
            uint32_t v[5];
            int got = 0;
            for (int i = 0; i < 5; i++)
                if (adc_read_mv(&v[got])) got++;
            if (got == 5)
                bat_percent = mv_to_percent(median5(v));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#endif /* !PACT_SIM */
