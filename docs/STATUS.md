# Pact MP-1 Firmware - Status & Roadmap

Updated 2026-07-14. The working handoff document: what exists, what is verified,
what remains. Written at the end of the first major build push (branches
`v1.1` = mainline, `ui/a-recommended` = UI variant A, both pushed to
`github.com/Msimrose/Simris-MP3-Firmware`).

---

## 1. What is DONE and verified

### Audio engine (`App/audio/`) - the bit-perfect core
- Decoders: dr_flac, minimp3 (float path), dr_wav, unified behind
  `audio_source.h`; everything decodes to full-scale interleaved stereo s32
  (the SAI wire format). No resampling anywhere: the output clock follows the
  file (device: PLL2/PLL3 mux switch; sim: SDL device reopen).
- Lock-free SPSC `pcm_ring` (C11 atomics), 32-step log volume (2 dB/step,
  Q31 round-to-nearest, true bypass at step 31), engine with
  play/pause/seek/stop + pump, pthread/FreeRTOS shim (`audio_port.h`).
- **Verified vs ffmpeg** (decode_test tool): FLAC 16/44.1 and 24/96 and WAV
  BIT-PERFECT; seek sample-exact; MP3 320/V0 = 128 dB SNR with exact gapless
  length match; volume -20 dB measures 0.100000.

### Library (`App/library/`) - fully automatic from file tags
- Custom parsers (no tag-lib bloat): FLAC STREAMINFO/Vorbis/PICTURE block
  walk; ID3v2.3+2.4 (UTF-16/latin1/UTF-8 to UTF-8, APIC art offsets, v1
  fallback, Xing/VBRI/CBR duration probe); WAV header probe.
- Art recorded as location (embedded offset+size or folder cover.jpg),
  extracted on demand; extraction verified byte-identical.
- Scanner walks a folder tree (POSIX now; FatFs port pending), sorts into
  albums, and **derives views automatically**: artist aggregation (albums +
  tracks per artist, alphabetized) and a global title-ordered songs index.
- Compact binary index (save/load round-trip verified field-by-field).

### UI - variant A, exact to Figma (file AN66SqmZfUbMRvjufRK1YR)
- Screens: split Menu (01), album Carousel (08 flat rail, coverflow motion:
  420ms pos ease-out / 220ms scale-opa, retargeting, stepped tops via pivot
  119), Track list (13:16), Now Playing NP-A (10:2, + format badge),
  Artists list, Artist page, Songs list. All values pulled via Figma MCP
  design-context (12 exact Diatype cuts baked; see `App/ui/assets/`).
- Brand: simris audio wordmark baked from the Figma asset (PACT wordmark
  scrapped); native-drawn iOS-style battery 23x11 (hairline shell +
  continuous fill, red <15%); volume toast; iPod step-scrolling everywhere
  (selection walks visible rows, list steps instantly at the edge).
- Playback UX: select-to-play lands on Now Playing; center pause/resume;
  left = restart >3s in, else previous; right = next; wheel on NP = volume;
  auto-advance through album with ring-drain grace; track switches
  rate-limited 350ms (key-repeat storm fix).

### Simulator (`sim/`)
- Custom HiDPI SDL driver (`sdl_driver.c`): framebuffer maps 1:1 to Retina
  pixels (text renders exactly as the panel will); `--big` = 2x window.
  LVGL's stock SDL window is NOT HiDPI-aware; do not go back to it.
- Flags: `--library <dir>` (remembered in ~/.pact_sim_lib), `--play <file>`,
  `--shot out.bmp` (+ `--screen albums|tracks|nowplaying|artists|songs`),
  `--datafont`, `--screen stress2` (real-key navigation fuzz; ALWAYS run
  before handing a build to Micah).
- Art pipeline (sim-side stand-in for the device thumb cache): extract ->
  sips via PNG intermediate (forces BASELINE jpeg; progressive breaks both
  TJPGD and the H7 hardware JPEG codec) -> per-size jpg + BMP twin; cache
  keyed by first-track path hash. Carousel loads BMP twins to RAM
  (`pact_thumb_load_bmp`) because LVGL 9.4 cannot scale-transform
  file-sourced JPEGs.
- Demo gallery: /tmp/pact-gallery (regenerate if /tmp purged: real covers +
  symlinked real albums).

### Storage stack (compiles for device; runs at hardware bring-up)
- `App/pact_io.h` seam: host backend = stdio, device = FatFs. All decoders,
  tag parsers, index IO go through it (regression suite re-verified after
  the swap).
- `lib/fatfs/` R0.15b: exFAT + UTF-8 LFN (heap) + 2 volumes + mkfs.
  `App/storage/diskio_sdmmc.c`: SDMMC1 = microSD = "0:", SDMMC2 = eMMC =
  "1:", polling first (DMA+MPU is a Phase-2 perf step).

### SAI output driver (compiles for device; sounds at hardware bring-up)
- `App/hal/sai_out.c` + `App/audio/audio_out.h`: ring consumer on
  SAI1/DMA1_Stream1 circular DMA (2x 1024-frame halves, 16 KB in `.d2_bss`
  at 0x30000000). Underrun = zero-fill (never stale samples) + counter;
  ISR wakeup hook ready for the audio_task notification (DMA IRQ prio 5 =
  max-syscall, so FromISR calls are legal in the hook).
- Pop-free sequencing per spec section 7: PWR5V_EN -> settle -> BCK runs
  zeros -> DAC lock -> XSMT high -> ramp -> AMP_EN; exact reverse on stop.
  Rate switches keep the +/-5V rails up and go through a full mute.
- Clocking: NODIV=1 so SCK = kernel/MCKDIV, fs = SCK/64 - exact division
  for every rate 8k..192k in both families. 44.1k family = PLL2P
  11.2896 MHz (mux-only; PLL2 also feeds the ADC, NEVER reconfigure it).
  48k family = PLL3P 12.288 MHz (M=25 N=393 FRACN=1769 P=32) - CubeMX
  solved these but generates no PLL3 code; brought up at runtime on first
  use via HAL_RCCEx_PeriphCLKConfig (this HAL has no HAL_RCCEx_EnablePLL3),
  then left running so later family switches are mux-only.
- Each refilled half is explicitly cache-cleaned, so the driver is correct
  both before and after the MPU non-cacheable D2 region lands (boot wiring).

### Device build
- Whole app (LVGL + fonts + UI + decoders + library + FatFs + SAI driver)
  compiles and links into the firmware image with the CubeMX core:
  **~910 KB flash (44%), DTCM 83K/128K, LVGL 64K pool in AXI**
  (`.axi_bss`), 16K of D2 in use (SAI DMA buffer; rest reserved for PCM
  ring + SDMMC buffers). Link probe in `main.c` USER CODE 2 keeps the
  call graph honest (replace with real boot at bring-up).
- Linker: `.axi_bss` / `.d2_bss` sections added to STM32H743XX_FLASH.ld
  (a CubeMX regen may rewrite the .ld: re-add if so). `App/pact_mem.h`
  has the placement macros.

---

## 2. What REMAINS

### Backend (writable now, testable on hardware)
1. **Real boot wiring + MPU** - replace link probe: FreeRTOS tasks
   (audio/ui/input/storage/power per firmware-spec section 10), MPU
   non-cacheable D2 region, PWR_HOLD first thing in main. Wire the
   audio_task pump to audio_out_set_wakeup (vTaskNotifyGiveFromISR) and
   allocate the PCM ring in D2 (PACT_D2).
2. **Display driver** - RM690B0 over QUADSPI. ⚠ Use the OFFICIAL Startek
   init from `~/Downloads/KD024EGOIN152-01 SPEC V0.pdf` section 6.3
   ("Power on Initial Code For MCU"), NOT the LilyGO port (LilyGO was only
   ever a reference for the same RM690B0 controller IC; the panel spec
   supersedes it). Key facts from the spec: driver IC = RM690B0; init =
   0xFE 0x20 / 0x26 0x0A / 0x24 0x80 / 0xFE 0x00 / CASET 0x2A
   0x0010..0x01D1 (**note the 16-column offset**) / RASET 0x2B
   0x0000..0x0257 / TE on 0x35 / brightness 0x51 0xFF / 0x30+0x12 partial /
   sleep-out 0x11 + 120ms / display-on 0x29. Power-off, Idle (0x39/0x38)
   and HBM (0x66) sequences also in section 6.3. Panel native 450x600
   portrait; UI renders 600x450 landscape (rotate via MADCTL or in blit).
3. **Library scan over FatFs** - port scan_dir to f_opendir/f_readdir
   (parsers already portable); on-device thumb cache generation
   (HW JPEG decode -> pre-scaled raw thumbs; must transcode progressive
   sources, see baseline note above).
4. **USB MSC** (Phase 7): TinyUSB or ST stack; unmount FatFs while host
   owns volumes; DMA double-buffered bridge for ~24 MB/s.
5. **Input/power HAL**: buttons EXTI debounce, AS5600 wheel poll -> named
   events (the UI already consumes `pact_event_t` only), battery ADC
   (sampling time fix needed: 1.5 cyc too short) + LiPo LUT, sleep/wake.
6. **Audio polish**: gapless (engine APIs already expose exact lengths),
   UI sounds mixer (Micah's sound design, later).

### UI
- Variant B (different composition: 01c big-type menu, NP-C poster etc.)
  on its own branch for comparison; variant C if wanted.
- Queue (06), Settings (07), Search with A-Z scrubber (10, non-touch!),
  Artist page enrichment (09's POPULAR section), boot splash (logo baked
  already), art-missing placeholder boxes everywhere (failures currently
  invisible), Shuffle Songs behavior, Playlists.
- Songs-context auto-advance (currently album-based even from Songs view).

### Hardware (blocks all on-device testing)
- Schematic fixes from `Simris PACT mini p1/docs/preflight-review.md`:
  J1 USB-C VBUS + GND unconnected, J5 SWD header missing, QSPI IO0/IO1
  swapped, pin re-sync to the .ioc (14+ moved pins), LP5012 + power-latch
  wiring, then 6-layer layout -> JLCPCB. Buy ST-LINK (STLINK-V3MINIE).

---

## 3. Working agreements & gotchas
- Verify by running: every change gets `--screen stress2` + a `--shot`
  screenshot check (compare against Figma via stacked composites when
  styling). Bit-perfect suite: `decode_test` vs ffmpeg refs.
- Build: `source ~/.zshrc` first (CubeCLT/brew PATH). Sim:
  `cmake -S sim -B sim/build && cmake --build sim/build -j`. Device:
  `cmake --preset Debug && cmake --build --preset Debug -j`. New files
  need a RECONFIGURE (globs are configure-time) - a "successful" build
  that skipped configure is how we shipped a stale binary twice.
- LVGL 9.4: no scale transforms on file-sourced images; decoder_open
  returns OK with decoded=NULL (banded); use RAM draw_bufs. Screen swaps:
  `lv_obj_delete_async` (sync delete during key dispatch = use-after-free).
- Fonts: bake via lv_font_conv (`-r` ranges incl 0x2039-0x203A for
  chevrons), bpp 4, --no-compress. Sizes live in App/ui/assets.
- macOS: qlmanage can't render stroke-SVGs or currentColor and composites
  on white; Figma node screenshots are the reliable asset renderer.
  Progressive JPEG kills TJPGD + H7 HW JPEG: normalize to baseline.
- Micah's Figma is the design authority; pull exact values with
  get_design_context (Starter-tier call quota; batch what you need).
- Keep ~/Documents repos out of iCloud trouble: if git hangs, check
  `ls -lO` for dataless files (eviction) and free disk space first.
