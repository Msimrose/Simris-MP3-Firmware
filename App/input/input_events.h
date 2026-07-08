/*
 * Pact MP-1 - semantic input events
 *
 * The app layer only ever sees these named events. Which pin, EXTI line, or
 * SDL key produced them is the business of hal/ (device) or sim/ (desktop).
 * See docs/software-ui-spec.md section 4.2.
 */
#pragma once

typedef enum {
    PACT_EVT_NONE = 0,

    PACT_EVT_WHEEL_CW,      /* dial rotated clockwise (one detent-equivalent) */
    PACT_EVT_WHEEL_CCW,     /* dial rotated counter-clockwise */

    PACT_EVT_CENTER,        /* dial center dome: select / play-pause */
    PACT_EVT_UP,            /* ring top: menu / back */
    PACT_EVT_DOWN,          /* ring bottom: function (deferred assignment) */
    PACT_EVT_LEFT,          /* ring left: previous track */
    PACT_EVT_RIGHT,         /* ring right: next track */

    PACT_EVT_VOL_UP,
    PACT_EVT_VOL_DOWN,

    PACT_EVT_POWER_SHORT,   /* screen sleep/wake */
    PACT_EVT_POWER_LONG,    /* clean shutdown */
} pact_event_t;
