/*
 * Pact MP-1 - input HAL: buttons + AS5600 wheel -> pact_event_t queue.
 *
 * Device implementation: App/hal/input_hal.c. The input task samples the
 * seven buttons, the power switch and the wheel at 100 Hz, debounces, and
 * pushes semantic events; the UI task drains with pact_input_get(). The
 * simulator feeds ui_handle_event from SDL keys instead.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "input/input_events.h"

void pact_input_init(void);                 /* create the queue (pre-scheduler ok) */
void pact_input_task(void *arg);            /* the input task body (100 Hz poll) */

/* Pop one event; wait_ms 0 = non-blocking. False when queue empty. */
bool pact_input_get(pact_event_t *evt, uint32_t wait_ms);
