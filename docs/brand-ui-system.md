# Pact MP-1 — Brand & UI System

**One idea: radically simple.** Black screen, quiet white type, the album art is the only color.
Don't over-design it — the interface is a frame for the music, never the star.

*(This is the visual language. Functional behavior + architecture is in `software-ui-spec.md`.)*

---

## Orientation — LANDSCAPE
- **600 × 450, landscape (4:3, wider than tall).**
- The RM690B0 is mounted **landscape** (active area 51.56 × 38.72 mm — wide). Lay every screen out
  horizontally.
- ⚠️ Corrects the earlier "450×600 portrait" framing — same pixels, rotated. Update `firmware-spec.md`
  references to read **600×450 landscape**. (Memory math unchanged: 600×450 RGB565 ≈ 540 KB.)

## Color — there basically isn't any
- **Ground:** true black `#000000` (AMOLED — black pixels are off).
- **Text:** warm white `#F4F3EF`; secondary grey `#8A887F`.
- **No accent color. None.** No amber, no tint. Everything structural is black / white / grey.
- **Selection / active:** a subtle grey fill (`#1A1A1A`) or a full invert (white row, black text). Never a hue.
- **Progress bar & on-screen LED mirror:** **white** (the physical LED bar is white anyway).
- **Two colored exceptions only — both earned:**
  1. **Album art** — the single source of color on screen. Let it sing.
  2. **Battery** — a **dynamic** indicator, not a fixed icon: draw a battery **shell** (outline + nub) with
     a **proportional fill** (LVGL `lv_bar`/rect) whose width tracks the *real* charge % — smooth, continuous,
     Apple-style. **White** fill → **red below ~15%**; a **charging bolt** (Phosphor `battery-charging`) or a
     gentle breathing fill while plugged in. Optional numeric `84%` in mono. Source: ADC sense → LiPo curve.
     The one status allowed to shout. *(The physical white LED bar can mirror battery as segments too.)*

## Typography
- **ABC Diatype (Variable)** — a clean neo-grotesque. Bake a **Regular** cut for menus/body and a
  slightly heavier **Medium** cut for the now-playing title. (Variable font → on-device it's static
  cuts; the MCU doesn't interpolate axes.) Quiet, spacious, confident — give it room.
- **Mono** for data: times, format badges, small labels.
- ⚠️ **License:** the **"Edu"** cut is educational-only — fine for the build, **not for sale**. Swap
  a licensed commercial Diatype cut before shipping. See `asset-pipeline.md` §1 for the full pipeline.

## Screens (all landscape)
- **Boot:** logo placeholder → menu. *(Logo added later — never a build/boot blocker.)*
- **Menu:** plain list, generous rows. Now Playing · Albums · Artists · Songs · Folders · Playlists · Settings.
- **Now Playing:** **art on the left, info on the right** (landscape split) — cover square on the left half;
  track / artist / album, progress + times, format badge, LED mirror on the right. White only.
- **Library:** horizontal grid/row of cover thumbnails (the color).
- **Album Carousel (signature):** horizontal coverflow — wheel scrolls sideways, centered cover lifts &
  brightens, neighbors fall back; press to open the album → track list → play.

## Motion
- Minimal. Snap, ease, done. No bounce, no flourish. The music is the motion. Respect the calm.

---

## Icons — Apple-level quality, legally yours, no lawsuit
**Do NOT use SF Symbols** — Apple licenses them for use *only inside Apple-platform software*; you can't
put them in your own hardware product. Don't rip Apple's actual PNGs either. These open sets are just as
crisp and are fine for a commercial product:

| Set | License | Why |
|---|---|---|
| **Phosphor Icons** | **MIT** | Top pick. 9,000+ icons, 6 weights incl. **Thin / Light** — pairs perfectly with ABC Diatype. phosphoricons.com |
| **Lucide** | ISC | Clean, consistent, UI-first (Feather's successor). lucide.dev |
| **Tabler Icons** | MIT | 5,000+ outline icons, very even stroke. tabler.io/icons |
| **Material Symbols** | Apache 2.0 | Google; variable weight + fill, huge coverage. |

**For this device:** grab the **Phosphor "Light"** weight (battery, play/pause/next/prev, folder, shuffle,
repeat, gear) — it matches the type. All MIT/ISC/Apache = free to use and modify in a product you sell.

**Embedded path:** export SVG → convert to an **LVGL image or symbol-font** (`lv_img` / custom symbol
font). Keep a single-weight icon set at 1–2 sizes so they read consistently at small px.
