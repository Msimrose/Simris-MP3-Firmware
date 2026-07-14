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

### Storage stack (host-VERIFIED on a disk image; SDMMC silicon at bring-up)
- `App/pact_io.h` seam: host backend = stdio, device = FatFs. All decoders,
  tag parsers, index IO go through it (regression suite re-verified after
  the swap).
- FatFs upgraded **R0.15b -> R0.16 + official patches p1+p2** (2026-07-14).
  Why: R0.15b shipped a real regression - f_readdir repeats the LAST entry
  forever at end-of-directory (upstream R0.16 changelog: "Fixed f_readdir
  cannot detect end of directory... appeared at R0.15b"). Found by
  fatfs_test the first time f_readdir ever actually ran; on device it
  would have hung the boot scan in an infinite loop. p2 also fixes the
  2026 FatFs CVEs (malicious/corrupt volume robustness - a device that
  mounts user SD cards wants this). Code page 932 -> 437 (LFN names are
  Unicode regardless; drops ~60K of DBCS tables from flash).
- **FF_FS_REENTRANT = 1** with FreeRTOS mutexes in `ffsystem_pact.c`
  (pthread when PACT_FATFS_HOST): audio (decoder reads), ui (track open)
  and storage (scan) all call FatFs concurrently - the file's old "single
  filesystem owner" comment was stale and wrong.
- Device library scan DONE: `library.c` walks f_opendir/f_readdir on
  device builds (walk helpers shared with the POSIX branch; per-level
  frames heap-allocated because the storage task stack is 6K).
  storage_task mounts, scans (eMMC "1:" first, else "0:"), saves
  1:/pact.idx, publishes to ui_task. Volume-merge + rescan-on-SD-insert
  are TODO.
- **VERIFIED on host** (`sim/fatfs_test/`, compiled WITHOUT PACT_SIM +
  with PACT_FATFS_HOST): 1 GiB file-backed exFAT image -> f_mkfs -> copy
  the demo gallery in (43 real tracks: MP3 embedded-art UTF-8 titles +
  24/48 FLAC + folder art) -> device scan -> field-by-field index
  round-trip through pact_io -> art extraction. Passes in 0.5 s. Run:
  `fatfs_test <music_dir> [image]`.
- `App/storage/diskio_sdmmc.c`: SDMMC1 = microSD = "0:", SDMMC2 = eMMC =
  "1:", polling first (DMA+MPU is a Phase-2 perf step). This diskio layer
  against real silicon is the only storage piece left unverified.

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

### Boot wiring + MPU (compiles for device; runs at hardware bring-up)
- Link probe RETIRED - `App/hal/pact_boot.c` is the real startup, called
  from main() USER CODE RTOS_THREADS. Tasks (CMSIS-RTOS2, stacks from the
  64K RTOS heap in DTCM): audio (High; pump loop woken by the SAI wakeup
  hook via vTaskNotifyGiveFromISR + 10ms poll fallback), ui (Normal;
  lv_init + tick -> pact_display_init() -> wait for the library ->
  ui_init + lv_timer_handler loop), storage (Low; mounts, scans, saves
  the index, publishes the library). input/power/led tasks come with the
  input/power HAL.
  `device_play()` = the UI on_play seam: engine play ->
  audio_out_start(track rate).
- main() USER CODE 1: PWR_HOLD (PE15) latched high first thing via raw
  registers (before HAL_Init), and MPU region 1 = D2 SRAM1+SRAM2 256 KB
  non-cacheable (TEX=1 C=0 B=0), configured before the generated
  MPU_Config() enables the MPU and caches come up. D2 SRAM3 (0x30040000)
  stays cacheable: keep .d2_bss under 256 KB.
- newlib heap -> AXI: `App/hal/pact_heap.c` + `-Wl,--wrap=_sbrk`
  (CMakeLists). 192 KB arena at 0x24010000; DTCM heap was ~40 KB and
  dr_flac opens alone need ~35 KB. __malloc_lock/__malloc_unlock =
  scheduler suspension (nano-malloc is not thread-safe alone). malloc
  from tasks only, never ISRs; DMA buffers stay explicit PACT_D2 statics.
- PCM ring on device: 16384 frames (128 KB, .d2_bss) - matches the sim.

### Display driver (compiles for device; lights up at hardware bring-up)
- `App/hal/disp_rm690b0.c` + `App/hal/pact_display.h`. Init = the OFFICIAL
  Startek spec 6.3 MCU code verbatim (0xFE 0x20 / 0x26 0x0A / 0x24 0x80 /
  page 0 / CASET 16-col offset / RASET / TE on / 0x51 0xFF / 0x30+0x12
  partial / sleep-out +120ms / 0x29) with exactly two additions: COLMOD
  0x3A=0x55 (RGB565; official code leaves 24-bit default) and MADCTL for
  the landscape mount. Power-off / idle / HBM codes from the same section
  (off implemented; idle/HBM TODO trivial).
- QSPI transport: cmd = 0x02 + (cmd<<8) 24-bit addr, 1 lane; pixels =
  0x32 + 0x002C00, 4 lanes, one CS burst per flush. MX_QUADSPI_Init is a
  CubeMX placeholder (prescaler 255!) - the driver re-inits at runtime:
  40 MHz to start (prescaler 5; try 3 = 60 MHz later), FlashSize 23.
- LVGL: 600x450 landscape display, 2x 600x48 RGB565 partial buffers
  (57.6 KB each, AXI), RENDER_MODE_PARTIAL, blocking polled flush
  (~2.9 ms/band @40 MHz) with lv_draw_sw_rgb565_swap (panel is
  big-endian). MDMA + TE-synced flush = the planned perf step.
- Rails: PMIC_EN -> 10ms -> PMIC_CTRL -> 10ms -> DISP_RST high -> 50ms ->
  init. ⚠ BRING-UP UNKNOWNS (one-constant fixes, flagged in the source):
  MADCTL 0x60 vs 0xA0 (which way is up) and which axis carries the +16
  offset (LCD_X_OFF/LCD_Y_OFF). Bench note: TPS65632 ELVDD is 4.6 V
  fixed vs the panel's 3.6 V typical (in the 2.0-6.0 V spec range, but
  watch first power-up).

### Input/power HAL (compiles for device; feels real at hardware bring-up)
- `App/hal/input_hal.c` + `pact_input.h`: input task polls at 100 Hz -
  7 buttons (active-low, two-sample debounce = 20 ms worst case; volume
  keys auto-repeat 400 ms/150 ms), power switch (release <2 s =
  POWER_SHORT, hold 2 s = POWER_LONG once), AS5600 wheel (RAW_ANGLE reads
  over I2C1 @0x36, wrap-aware deltas, 24 detents/rev -> WHEEL_CW/CCW;
  I2C errors counted not fatal). Events land in a FreeRTOS queue as
  `pact_event_t`; ui_task drains it into ui_handle_event. The CubeMX EXTI
  lines stay armed for STOP-mode wake later; the event path is polled.
  ⚠ Bring-up constants: PACT_WHEEL_INVERT (direction), PACT_WHEEL_DETENTS
  (feel), PACT_PWR_SW_ACTIVE_HIGH (verify latch polarity on the board).
- `App/hal/power_hal.c` + `pact_power.h`: power task at 1 Hz reads
  BAT_SENSE (ADC1 ch3) - **the 1.5-cycle sampling-time preflight finding
  is fixed here** (810.5 cycles + one-shot calibration at task start) -
  median-of-5, resting LiPo LUT -> percent; CHG_STAT/PG_STAT (open-drain
  active-low) -> charging/vbus getters; ui_task pushes percent+charging
  into ui_set_battery every 2 s. POWER_SHORT toggles panel off/on
  (playback keeps running dark); POWER_LONG -> pact_power_shutdown():
  audio_out_stop -> unmount both volumes -> panel off -> PWR_HOLD low.
  ⚠ PACT_BAT_DIV assumes a 2:1 divider - the divider is NOT yet wired on
  the schematic (open preflight item); set the real ratio at reconcile.

### Device build
- Whole app (LVGL + fonts + UI + decoders + library + FatFs + SAI driver
  + boot + display) compiles and links with the CubeMX core: **~1046 KB
  flash (51%), DTCM 83K/128K (64K RTOS heap + app bss), AXI 368K/512K
  (64K LVGL pool + 192K malloc arena + 115K display buffers), D2
  144K/288K (16K SAI DMA + 128K PCM ring)**. Flash grew ~136K at display
  registration: the RGB565 render paths only link once a real display
  exists, so the budget is honest now.
- Linker: `.axi_bss` / `.d2_bss` sections added to STM32H743XX_FLASH.ld
  (a CubeMX regen may rewrite the .ld: re-add if so). `App/pact_mem.h`
  has the placement macros. A regen also rewrites main.c USER CODE
  sections only if markers are damaged - the PWR_HOLD/MPU/boot hooks all
  live inside USER CODE blocks, so they survive.

---

## 2. What REMAINS

### Backend (writable now, testable on hardware)
1. **On-device thumb cache** - HW JPEG decode -> pre-scaled raw thumbs
   persisted on eMMC (must transcode progressive sources, see baseline
   note above); then the UI art provider hook in pact_boot.
2. **USB MSC** (Phase 7): TinyUSB or ST stack; unmount FatFs while host
   owns volumes; DMA double-buffered bridge for ~24 MB/s.
3. **Audio polish**: gapless (engine APIs already expose exact lengths),
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
