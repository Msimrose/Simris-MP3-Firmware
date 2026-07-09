# Album art thumbnails: making dark covers survive (note for Fable)

## The problem
Dark, low-contrast covers (test case: Mk.gee, "Two Star & The Dream Police") almost disappear as
small list thumbnails. Two failures stack up:
1. **Detail loss at thumbnail size** (a ~110 px thumb has too few pixels to keep a low-contrast subject).
2. **RGB565 shadow crush.** 16-bit color has very few code levels in the near-black range. A cover that
   lives almost entirely in the shadows loses exactly the subtle dark gradations that define the subject,
   so they quantize to flat black.

These only bite **small thumbnails of dark art**. Full-size Now-Playing art is fine and must be left
untouched for fidelity. Three fixes, all at thumbnail-generation time.

---

## Fix 1: quality downscale (never nearest-neighbor)
When shrinking decoded cover art to a thumbnail, average source pixels, do not point-sample.

- **DMA2D does NOT resize** on the H743 (Chrom-ART does pixel-format conversion, blending, and fill, but
  no interpolation/scaling). So the downscale is software or done during JPEG decode.
- **Preferred: scaled JPEG decode.** Decoding only the low-frequency DCT coefficients yields a 1/2, 1/4,
  or 1/8 image nearly for free, and it is effectively an area-average (high quality) downscale. Use this
  to get close to the target thumb size, e.g. decode at 1/8 for a tiny list thumb.
- **Otherwise: software box-filter.** Decode full res once, then average each source block into one
  destination pixel. This runs fine on the 480 MHz M7 for occasional thumbnail generation (it is not a
  per-frame operation).
- **Avoid** grabbing every Nth pixel (nearest-neighbor). That is what deletes low-contrast detail.

## Fix 2: dither on the RGB888 to RGB565 conversion
Truncating 8-bit channels to 5/6/5 is what crushes the shadows. Add a small dither before truncation so
gradations survive as a fine pixel pattern instead of collapsing to one flat black.

- **Ordered (Bayer) dither** is cheap, stable, and parallelizable. Recommended.
- Match the dither amplitude to the bits lost: R and B lose 3 bits (add up to ~7), G loses 2 bits
  (add up to ~3), from an 8x8 Bayer matrix.

```c
// per pixel, bayer8[y&7][x&7] is 0..63
static inline uint16_t to565_dithered(uint8_t r, uint8_t g, uint8_t b, uint8_t d /*0..63*/) {
    // scale the 0..63 Bayer value into each channel's quantization step
    int rr = r + ((d >> 3));        // ~0..7 for the 3 lost R bits
    int gg = g + ((d >> 4));        // ~0..3 for the 2 lost G bits
    int bb = b + ((d >> 3));        // ~0..7 for the 3 lost B bits
    if (rr > 255) rr = 255; if (gg > 255) gg = 255; if (bb > 255) bb = 255;
    return ((rr & 0xF8) << 8) | ((gg & 0xFC) << 3) | (bb >> 3);
}
```
Error-diffusion (Floyd-Steinberg) looks slightly better but is serial and slower; ordered dither is the
right default. Apply dithering to full-size art too if you see banding there; it is cheap.

## Fix 3: shadow-lift LUT, thumbnails only
Give tiny thumbnails a mild shadow lift so a dark subject reads at size. This is cosmetic and applies
**only** below a size threshold; full-size art is never touched.

- Precompute a 256-entry lookup table with a gentle gamma < 1 (lifts shadows), e.g. `out = 255 *
  (in/255)^0.85`, or a soft S-curve. Keep it subtle.
- Apply the LUT per channel during thumbnail generation, before the dither/565 step.
- Gate it: `if (thumbW <= 160) apply shadow_lift`. Leave Now-Playing art linear/full fidelity.

---

## Performance: cache thumbnails
The library grid shows many covers. Do not regenerate on every scroll frame.
- Generate each thumbnail once (decode, downscale, lift, dither, 565), then **cache the finished RGB565
  thumb** keyed by album. Cache in RAM (a small LRU) or on eMMC if the library is large.
- Full-size art: decode on demand when Now-Playing opens; no need to cache aggressively.

## Where each fix lives in the pipeline
```
embedded JPEG (in FLAC)
  → HW JPEG decode  (full res, or scaled-DCT decode for Fix 1)
  → downscale to thumb size (Fix 1: area-average / box filter)
  → shadow-lift LUT (Fix 3, thumbnails only)
  → RGB888 → RGB565 with dither (Fix 2)
  → cache the RGB565 thumbnail
```

## Test set
- **Torture case:** Mk.gee, "Two Star & The Dream Police" (near-black, low-contrast subject in shadow).
  If he is legible at list-thumb size, the pipeline is good.
- Also test a bright/high-contrast cover (should be unaffected) and a smooth-gradient cover (checks
  dithering).
- Judge on the real AMOLED, not a backlit monitor: the panel's true blacks and contrast recover a lot of
  dark-cover detail that a Mac preview muddies.
