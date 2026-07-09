/*
 * Device link probe (temporary, until real bring-up wiring).
 *
 * Forces the full application call graph (LVGL + UI + fonts + decoders +
 * library) into the firmware image so flash/RAM budgets are real instead
 * of gc-sections fiction. pact_probe_enable stays 0, so none of this runs;
 * the volatile read stops the compiler from proving it dead.
 *
 * Replaced by the real app startup at Phase 1 bring-up.
 */
#ifndef PACT_SIM
#include "lvgl.h"
#include "ui/ui_internal.h"
#include "audio/audio_engine.h"
#include "library/library.h"

volatile int pact_probe_enable = 0;

void pact_link_probe(void)
{
    static pcm_ring_t ring;
    static library_t lib;

    lv_init();
    library_load(&lib, "0:/pact.idx");
    ui_set_library(&lib, NULL);
    ui_init();
    audio_engine_init(&ring);
    audio_engine_play("0:/test.flac");
    while (audio_engine_pump()) {}
    ui_handle_event(PACT_EVT_CENTER);
    ui_nowplaying_refresh();
}
#endif
