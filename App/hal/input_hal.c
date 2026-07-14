/*
 * Pact MP-1 - input HAL: buttons, power switch, AS5600 wheel.
 *
 * Poll-based at 100 Hz from the input task: two consistent samples arm an
 * edge, so worst-case debounce is 20 ms - well under perception. The EXTI
 * lines CubeMX configured stay armed for STOP-mode wake later; they are
 * not used for the event path (polling is immune to contact chatter).
 *
 * Wheel: AS5600 on I2C1 @0x36, RAW_ANGLE (12-bit) read each tick; deltas
 * accumulate into detents (PACT_WHEEL_DETENTS per rev) and emit
 * WHEEL_CW/CCW. ⚠ BRING-UP: wheel direction (PACT_WHEEL_INVERT) and detent
 * count are feel-tuning constants; I2C errors are counted, not fatal
 * (the magnetic stack itself is a known bench-validation item).
 *
 * Volume buttons auto-repeat (400 ms delay, 150 ms rate). Power switch:
 * release under 2 s = POWER_SHORT, held 2 s = POWER_LONG (emitted once).
 * ⚠ PWR_SW_SENSE polarity assumed active-HIGH per the power-latch design;
 * verify on the board (PACT_PWR_SW_ACTIVE_HIGH).
 */
#ifndef PACT_SIM

#include "hal/pact_input.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define PACT_WHEEL_ADDR        (0x36u << 1)
#define PACT_WHEEL_REG_ANGLE   0x0Cu          /* RAW_ANGLE msb, lsb follows */
#define PACT_WHEEL_DETENTS     24             /* detent-equivalents per rev */
#define PACT_WHEEL_INVERT      0              /* flip if CW scrolls up */

#define PACT_PWR_SW_ACTIVE_HIGH 1
#define POLL_MS                10u
#define REPEAT_DELAY_TICKS     40u            /* 400 ms */
#define REPEAT_RATE_TICKS      15u            /* 150 ms */
#define PWR_LONG_TICKS         200u           /* 2 s */

extern I2C_HandleTypeDef hi2c1;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    pact_event_t  evt;
    bool          repeats;
    uint8_t       cnt;       /* consecutive pressed samples */
    bool          down;
    uint32_t      held;      /* ticks held (for repeat) */
} btn_t;

static btn_t btns[] = {
    { GPIOA, BTN_PLAY_Pin, PACT_EVT_CENTER,   false },
    { GPIOE, BTN_MENU_Pin, PACT_EVT_UP,       false },
    { GPIOE, BTN_FN_Pin,   PACT_EVT_DOWN,     false },
    { GPIOE, BTN_PREV_Pin, PACT_EVT_LEFT,     false },
    { GPIOE, BTN_NEXT_Pin, PACT_EVT_RIGHT,    false },
    { GPIOE, VOL_UP_Pin,   PACT_EVT_VOL_UP,   true  },
    { GPIOE, VOL_DOWN_Pin, PACT_EVT_VOL_DOWN, true  },
};

static QueueHandle_t evt_q;
static uint32_t wheel_i2c_errors;

void pact_input_init(void)
{
    evt_q = xQueueCreate(16, sizeof(pact_event_t));
}

bool pact_input_get(pact_event_t *evt, uint32_t wait_ms)
{
    if (!evt_q) return false;
    return xQueueReceive(evt_q, evt, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}

static void emit(pact_event_t e)
{
    if (evt_q) (void)xQueueSend(evt_q, &e, 0);   /* full queue: drop, fine */
}

/* ---- buttons (active low, two-sample debounce) --------------------------- */

static void poll_buttons(void)
{
    for (size_t i = 0; i < sizeof btns / sizeof btns[0]; i++) {
        btn_t *b = &btns[i];
        bool pressed = HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_RESET;
        if (pressed) {
            if (b->cnt < 2) {
                if (++b->cnt == 2 && !b->down) {
                    b->down = true;
                    b->held = 0;
                    emit(b->evt);
                }
            } else if (b->down && b->repeats) {
                b->held++;
                if (b->held >= REPEAT_DELAY_TICKS &&
                    (b->held - REPEAT_DELAY_TICKS) % REPEAT_RATE_TICKS == 0)
                    emit(b->evt);
            }
        } else {
            b->cnt = 0;
            b->down = false;
        }
    }
}

/* ---- power switch --------------------------------------------------------- */

static void poll_power_switch(void)
{
    static uint32_t held;
    static bool long_sent;
    bool raw = HAL_GPIO_ReadPin(PWR_SW_SENSE_GPIO_Port, PWR_SW_SENSE_Pin)
               == GPIO_PIN_SET;
    bool pressed = PACT_PWR_SW_ACTIVE_HIGH ? raw : !raw;

    if (pressed) {
        held++;
        if (held == PWR_LONG_TICKS && !long_sent) {
            long_sent = true;
            emit(PACT_EVT_POWER_LONG);
        }
    } else {
        if (held >= 2 && held < PWR_LONG_TICKS && !long_sent)
            emit(PACT_EVT_POWER_SHORT);
        held = 0;
        long_sent = false;
    }
}

/* ---- AS5600 wheel ---------------------------------------------------------- */

static void poll_wheel(void)
{
    static bool     have_last;
    static uint16_t last;
    static int32_t  accum;

    uint8_t raw[2];
    if (HAL_I2C_Mem_Read(&hi2c1, PACT_WHEEL_ADDR, PACT_WHEEL_REG_ANGLE,
                         I2C_MEMADD_SIZE_8BIT, raw, 2, 5) != HAL_OK) {
        wheel_i2c_errors++;
        have_last = false;
        return;
    }
    uint16_t angle = (uint16_t)(((raw[0] << 8) | raw[1]) & 0x0FFF);
    if (!have_last) {
        have_last = true;
        last = angle;
        return;
    }
    int32_t d = (int32_t)angle - (int32_t)last;   /* wrap to [-2048, 2047] */
    if (d > 2048)  d -= 4096;
    if (d < -2048) d += 4096;
    last = angle;
#if PACT_WHEEL_INVERT
    d = -d;
#endif
    accum += d;

    const int32_t step = 4096 / PACT_WHEEL_DETENTS;
    while (accum >= step)  { accum -= step; emit(PACT_EVT_WHEEL_CW);  }
    while (accum <= -step) { accum += step; emit(PACT_EVT_WHEEL_CCW); }
}

/* ---- task ------------------------------------------------------------------ */

void pact_input_task(void *arg)
{
    (void)arg;
    TickType_t next = xTaskGetTickCount();
    for (;;) {
        poll_buttons();
        poll_power_switch();
        poll_wheel();
        vTaskDelayUntil(&next, pdMS_TO_TICKS(POLL_MS));
    }
}

#endif /* !PACT_SIM */
