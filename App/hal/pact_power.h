/*
 * Pact MP-1 - power HAL: battery gauge, charge status, clean shutdown.
 *
 * Device implementation: App/hal/power_hal.c. The power task samples the
 * battery ADC and charger status pins once a second; the UI task reads
 * the getters when refreshing the battery glyph.
 */
#pragma once
#include <stdbool.h>

void pact_power_task(void *arg);      /* the power task body (1 Hz) */

int  pact_battery_percent(void);      /* 0..100, median-filtered; -1 = unknown */
bool pact_charging(void);             /* BQ24075 CHG_STAT active */
bool pact_vbus_present(void);         /* BQ24075 PG_STAT active */

/* Clean shutdown: stop audio (pop-free), unmount volumes, panel off,
 * PWR_HOLD low. Does not return (bench supply: parks forever). */
void pact_power_shutdown(void);
