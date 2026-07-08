# Pact MP-1 — Asset Pipeline (fonts, icons, images) for LVGL

**The point:** LVGL is a blank, fast canvas. The generic "printer UI" look comes from the *stock
theme + default font*, not from LVGL itself. This doc is the recipe for bringing the Pact's own
identity in — **ABC Diatype** type, **Phosphor** icons, the monochrome brand system — so nothing
generic ever ships.

Split of duties: **Micah** preps + licenses the source assets (fonts, icon SVGs, logo) and picks
sizes. **Fable** runs the converters, bakes the C arrays, wires the styles, and sets up the desktop
simulator so the UI can be seen on the Mac before hardware exists.

---

## 0. Tools (one-time install)
```bash
# LVGL font + image converters (Node)
npm i -g lv_font_conv          # TTF/OTF/WOFF -> LVGL C font arrays
# image conversion: use the online tool https://lvgl.io/tools/imageconverter
#   or the python/JS lv_img_conv for batch/CLI
# icon-font build (SVG set -> .ttf): fantasticon OR icomoon.io (web)
npm i -g fantasticon
```
All free/open. `lv_font_conv` is the workhorse.

---

## 1. Fonts — ABC Diatype, rendered natively

**Face:** **ABC Diatype Variable (Edu)** — a clean neo-grotesque (Dinamo foundry).

### ⚠️ License — read this
- The **"Edu"** cut is an **educational/student license — NOT valid for a product you sell.** It's
  fine for the portfolio build and all development. **If the Pact is ever sold, swap in a licensed
  commercial cut of Diatype** (same font, proper Dinamo embedding/app license). Nothing else in the
  pipeline changes — only the source file + the license behind it.

### Variable font → static cuts
LVGL does **not** do runtime axis interpolation. A variable font is a *design convenience*: pick the
instances you want (e.g. Regular ~400 for body, Medium ~500 for titles) and **bake each as a static
cut**. Export the instances from the variable file (Glyphs/FontTools) or point `lv_font_conv` at a
specific instance, then convert.

### Convert (per size + cut)
```bash
lv_font_conv --font ABCDiatype-Regular.ttf --size 20 --bpp 4 \
  --range 0x20-0x7F --range 0x2018-0x2019 --range 0x201C-0x201D \
  --format lvgl -o src/assets/diatype_regular_20.c
```
- **`--bpp 4`** = smooth anti-aliasing (16 alpha levels). Good balance of quality vs flash.
- **`--range`** = only the glyphs you use. ASCII `0x20-0x7F` + smart quotes/dashes. Add Latin-1
  (`0x00A0-0x00FF`) if track metadata needs accents. Keep it tight — every glyph costs flash.
- One `.c` per (cut × size). Suggested set below.

### Suggested sizes (600×450 landscape, ~51 mm wide)
| Role | Cut | px |
|---|---|---|
| Small data / mono labels | (system mono, see §1b) | 14–16 |
| Menu rows / body | Diatype Regular | 20–22 |
| Now-Playing artist/album | Diatype Regular | 24 |
| Now-Playing **title** | Diatype Medium | 30–34 |
| Boot / large | Diatype Medium | 40 |

### 1b. Mono for data
Times / format badges / small numeric labels want a mono face (per brand doc). Either bake a mono
Diatype cut if the license includes one, or use a clean open mono (e.g. **IBM Plex Mono**, OFL) at
14–16 px. Convert the same way.

### Use in code
```c
LV_FONT_DECLARE(diatype_regular_20);
lv_obj_set_style_text_font(label, &diatype_regular_20, 0);
```

---

## 2. Icons — Phosphor as an icon-font (monochrome, recolorable, tiny)

Your UI is single-color, so **build an icon font**, don't ship PNGs. Icons then render *like text*:
scalable, recolored in code, a few KB total.

### Pick the set
- **Phosphor "Thin" or "Light"** to match Diatype's stroke. (MIT — free in a product you sell.)
- Only the glyphs you actually use: `play, pause, play-next, play-prev, shuffle, repeat, repeat-once,
  folder, gear, caret-left/right/up/down, music-note, playlist, battery-charging, magnifying-glass,
  arrow-left`. (The battery *fill* is drawn, not an icon — see §4.)

### Build the font, then convert
```bash
# 1. SVGs -> one .ttf (assigns each glyph a codepoint in the Private Use Area, 0xE000+)
fantasticon ./phosphor-light-svgs -o ./build --name pact-icons \
  --font-types ttf --normalize

# 2. .ttf -> LVGL C array (range = the PUA codepoints fantasticon assigned; see its codepoints.json)
lv_font_conv --font ./build/pact-icons.ttf --size 24 --bpp 4 \
  --range 0xE000-0xE0FF --format lvgl -o src/assets/pact_icons_24.c
```
Bake **1–2 sizes** (e.g. 20 + 28) so icons read consistently. Define readable names for the
codepoints:
```c
#define ICON_PLAY   "\xEE\x80\x80"   /* U+E000, UTF-8 — match fantasticon's codepoints.json */
lv_obj_set_style_text_font(icn, &pact_icons_24, 0);
lv_obj_set_style_text_color(icn, lv_color_hex(0xF4F3EF), 0);  /* recolor freely */
lv_label_set_text(icn, ICON_PLAY);
```

### When to use `lv_img` instead
Only for something detailed/multi-tone — e.g. a rendered logo mark. Convert PNG/SVG → LVGL image C
array (online image converter, RGB565 or with alpha). Overkill for flat glyphs.

---

## 3. Album art & logo (runtime images)
- **Album art:** decoded at runtime from the file's embedded JPEG via the **H7 hardware JPEG unit**
  → DMA2D scale → `lv_img`/canvas. Not baked into firmware. (firmware-spec §8 / Phase 6–7.)
- **Boot logo:** a single baked `lv_img` (C array in flash) *or* a bin on flash. Deferred —
  never a boot blocker (brand doc). White/grey only, on black.

---

## 4. Brand tokens as LVGL styles
Set these once (a `theme.c`) and apply everywhere — this *is* the brand system in code:
```c
#define COL_GROUND    lv_color_hex(0x000000)  /* true black — AMOLED off */
#define COL_TEXT      lv_color_hex(0xF4F3EF)  /* warm white */
#define COL_TEXT_DIM  lv_color_hex(0x8A887F)  /* secondary grey */
#define COL_SELECT    lv_color_hex(0x1A1A1A)  /* subtle selection fill */
#define COL_WHITE     lv_color_hex(0xFFFFFF)  /* progress bar / LED mirror */
#define COL_BATT_LOW  lv_color_hex(0xE04030)  /* battery < ~15% — the ONLY red */
/* NO accent hue anywhere else. Color comes only from album art + the battery fill. */
```
- **Selection:** grey fill `COL_SELECT`, or full invert (white row / black text). Never a hue.
- **Battery:** draw a shell (outline + nub) + a proportional `lv_bar`/rect fill tracking real % —
  white, → `COL_BATT_LOW` under 15%, charging bolt (`ICON_BATTERY_CHARGING`) or breathing fill when
  plugged. The one thing allowed to shout.
- **Motion:** snap/ease, short. No bounce. `lv_anim` with `lv_anim_path_ease_out`, ~150–200 ms.

---

## 5. Where assets live (flash vs eMMC)
| Asset | Storage | Why |
|---|---|---|
| Fonts (Diatype cuts, mono) | **internal flash** (baked C) | needed at boot, small, always present |
| Icon font | **internal flash** (baked C) | tiny, used everywhere |
| Boot logo | internal flash (baked) | boot-time, single small image |
| Album art | decoded from track files (eMMC/µSD) at runtime | large, per-track, must not bloat firmware |
| Any large image set | eMMC, loaded on demand | keeps the .elf lean |

Keep baked assets lean — the whole point of `--range` and 1–2 sizes. Rough budget: full font+icon set
should sit comfortably in a few hundred KB of the 2 MB flash.

---

## 6. LVGL desktop simulator (see the UI on your Mac, no board)
LVGL runs on the desktop via SDL2 — renders the *real* screens in a window so the carousel/menus can
be built and previewed before hardware exists. Same UI code ports to the device.
```bash
brew install sdl2          # macOS graphics backend for the sim (installs Homebrew if needed)
# Fable sets up a separate CMake target (host build) that compiles the UI layer + LVGL against
# the SDL driver, at 600x450, using the SAME fonts/icons/styles baked above.
```
- **Fable:** add a `sim/` host target (SDL driver, 600×450, dummy input mapped to the named button
  events) so the UI layer is portable between sim and device. Build the UI here first, then flash.
- **Micah:** run the sim target to click through the interface as it's built.

---

## Checklist
**Micah (prep + license):**
- [ ] Confirm Diatype license path — Edu for the build; **commercial cut before any sale**
- [ ] Export Diatype static instances (Regular + Medium) from the variable file
- [ ] Download Phosphor **Light/Thin** SVGs for the glyph list in §2
- [ ] Decide mono face (Diatype mono if licensed, else IBM Plex Mono OFL)
- [ ] (later) supply a logo mark, white/grey on black

**Fable (convert + wire):**
- [ ] `npm i -g lv_font_conv fantasticon`; `brew install sdl2`
- [ ] Bake Diatype cuts (§1 sizes) + mono → `src/assets/*.c`
- [ ] Build Phosphor icon font → `pact_icons_*.c`; map codepoints to `ICON_*` defines
- [ ] `theme.c` with the §4 tokens; apply across all screens
- [ ] Host `sim/` target (SDL, 600×450) sharing the UI layer + baked assets
