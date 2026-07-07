# Pact MP-1 — Software & UI Spec (handoff for Fable)

**Purpose:** the spec for *writing the firmware application* — architecture, audio behavior, and UI/UX.
Hand this to the coding agent (Fable) to scaffold and build the app logic.

> ## ⚠️ Scope: pins are OUT of this doc, on purpose
> **Do NOT hardcode pin numbers or peripheral init here.** Write the application against a thin **HAL/BSP
> layer** with clean interfaces (`audio_out_write()`, `display_flush()`, `input_poll()`, `storage_read()`,
> etc.). Leave the actual pin/GPIO/peripheral wiring as **TODO stubs**. The real pin map lives in
> `docs/pinout/mcu-pinout.md` and gets dropped into the HAL **later** — the app logic must not care which
> pin a button is on, only that "Next was pressed."
>
> For locked *technical* decisions (RTOS, decoders, H7 memory/DMA rules, toolchain), see `firmware-spec.md`.

---

## 1. What it is
A pocket audiophile music player: FLAC / MP3 from internal eMMC + microSD, 2.4" AMOLED (~450×600),
wheel + buttons, hi-fi DAC/amp. **The UI is a first-class feature** — fast, gorgeous, album-art-forward,
music-first. Think "the nicest little music player you've held," not a utilitarian embedded menu.

## 2. Stack (locked — details in `firmware-spec.md`)
C11 · FreeRTOS (CMSIS-RTOS2) · **LVGL v9** (RGB565, partial buffers) · dr_flac + minimp3 · FatFs (exFAT) ·
hardware JPEG for album art. Build: CMake + arm-none-eabi-gcc (CLI, no IDE).

## 3. Architecture & conventions
**Go by convention** — idiomatic C, clear module boundaries, decoupled layers. Modules:
- `audio/` — decode → PCM ring → I²S out (the only real-time path)
- `library/` — scan/index eMMC+SD, tags, album art, persisted DB/cache
- `ui/` — LVGL screens, navigation, animations (this doc's focus)
- `input/` — wheel + button events → an event queue (by *function*, not pin)
- `player/` — playback state machine (play/pause/seek/queue/shuffle/repeat)
- `hal/` — **thin hardware shim; pins/peripherals stubbed for now**
- `power/`, `storage/` — per `firmware-spec.md`

UI talks to `player/` via commands + observes state; never blocks the audio task. LVGL called only from `ui_task`.

---

## 4. UI / UX design

### 4.1 Look & feel
- **Font:** **ABC Favorit** (Dinamo), the **Variable** cut, **Edu** license — **Light** weight as the
  primary across the UI. Heavier weights only sparingly for emphasis (e.g. the now-playing title). It's a
  clean neo-grotesque — keep the type quiet and confident, lots of breathing room. *(LVGL needs the TTF
  converted via `lv_font_conv`: generate **Light** at title / body / small sizes.)*
- **Theme:** dark, near-black background (AMOLED blacks ≈ free power + deep contrast). Album art and
  accent color carry the visual weight. Minimal chrome.
- **Motion:** smooth, DMA2D-accelerated transitions and easing. Nothing janky — 60 fps target for UI.
- **Density:** big, legible, touch-of-premium spacing. This is a boutique object, not a spec sheet.

### 4.2 Navigation model (by function — pins added later)
- **Wheel:** scroll / move through lists and the carousel.
- **Center (dome):** select / play-pause.
- **Buttons (functional):** Next, Prev, Menu, Back/Function, Vol +, Vol −, Power. *(Physical mapping
  comes from the pinout later; the app only handles named events.)*

### 4.3 Screens
1. **Now Playing** — large album cover, title / artist / album, progress bar + time, play state, small
   format badge (e.g. "FLAC 24/96"). LED bar mirrors volume. The home base.
2. **Library** — browse by **Albums**, **Artists**, **Songs**, **Folders**, **Playlists**. Album/Artist
   views show cover thumbnails.
3. **Album Carousel** — the horizontal cover browser (see §4.4). The signature view.
4. **Queue / Up Next** — current queue, reorder, clear.
5. **Settings** — output, gapless, theme accent, brightness, about.

### 4.4 ⭐ Album Carousel (the signature feature)
The one Micah described: *click into albums, then flip through them horizontally.*
- Enter from Library → Albums (or a dedicated shortcut).
- Albums render as a **horizontal row of cover art** — a coverflow-style carousel. The **centered cover is
  enlarged/highlighted**; neighbors sit smaller/dimmed to the sides.
- **Wheel scrolls horizontally** through covers (with momentum/snap-to-center); animation is smooth and
  DMA2D-accelerated.
- **Center/select on a cover** → opens that album's track list → select a track to play (or "Play album").
- Optional: title/artist label under the centered cover; subtle reflection/shadow if cheap to render.
- Covers come from the **library thumbnail cache** (hardware-JPEG decoded, pre-scaled) so scrolling never
  re-decodes full images. Cache misses show a placeholder, fill in async.

### 4.5 Now-playing & art everywhere
Album art shows on: now-playing (large), album/artist browse (thumbnails), the carousel, lock/sleep.
Source: embedded tag art (ID3 `APIC` / FLAC `PICTURE`), fallback `folder.jpg`. Dark UI throughout.

### 4.6 Boot & wake behavior
- **Cold boot (real power-on):** brand **logo splash** → then the **main menu**.
- **Wake from sleep (screen was off, device still on):** return to **exactly where it left off** —
  now-playing / last screen + playback state. **No logo on wake**, no menu detour.
- **⚠️ The logo is a DEFERRED placeholder — it must NEVER block boot or the build.** Micah supplies the
  logo asset later (made in Illustrator). For now, stub it: a text/vector placeholder behind an
  easily-swapped asset slot and a `#define SHOW_BOOT_LOGO` flag. Ship the boot flow working *without* the
  final logo; drop the asset in when it's ready.

---

## 5. Core behaviors
- **Playback:** play/pause/next/prev, seek, **gapless**, shuffle, repeat (off/one/all), resume-where-left-off.
- **Volume:** digital, 32 steps, log taper; LED bar mirrors it.
- **Library:** background scan/index of both volumes on boot + on card insert; extract title/artist/album/
  art; persist an index + thumbnail cache so browsing is instant.
- **Now-playing state** persists across sleep/power.
- **Sleep:** screen-off playback is the common battery mode; wake on button.

## 6. Controls → functions (mapping, no pins)
| Event | Action |
|---|---|
| Wheel scroll | move in list / carousel |
| Center press | select / play-pause |
| Next / Prev | track skip (or carousel jump in browse) |
| Menu | open menu / context |
| Back / Function | up a level / configurable |
| Vol ± | volume |
| Power | short = screen sleep/wake; long = shutdown |

## 7. Explicitly deferred (add later, not now)
- **Pin/peripheral wiring** → from `mcu-pinout.md` into the `hal/` layer.
- **MJPEG video playback** → later phase (`firmware-spec.md` §8.5).
- **AAC decode** → Phase 8 decision.
- **Clock-tree specifics** (audio PLLs, USB ULPI) → firmware clock init, from CubeMX export.

## 8. Build convention
CMake project (from CubeMX CMake output) + arm-none-eabi-gcc, terminal-buildable so the agent can
compile-test-iterate. App modules in `firmware/app/` (untouched by CubeMX regen). Each module gets a
minimal test/harness where practical.

---

## Open items for Micah to fill
- [x] **Font** = ABC Favorit Variable (Edu), **Light** primary. Exact sizes firm up with layout.
- [x] **Power-up default** = logo splash → menu (cold boot); resume-to-last on wake.
- [x] **Logo** = deferred placeholder; Micah supplies the Illustrator asset later (must not block build).
- [ ] **Accent color** — the draft brand UI system proposes one; confirm or swap.
- [ ] Any other **signature UI touches** beyond the carousel (visualizer style, transitions)?

## Brand UI system
Visual design system (typography, color, screen mockups incl. the album carousel) lives as a rendered
Artifact — the design target Fable builds the LVGL UI against.
