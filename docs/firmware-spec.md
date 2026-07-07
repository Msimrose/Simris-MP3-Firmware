# Pact MP-1 — Firmware Specification (v2)

**Purpose:** the software spec for the Pact MP-1 pocket audiophile player, written to hand to an AI coding agent (Fable) to scaffold and build the firmware. v2 rewritten 2026-07-04 (by Fable): synced pins to `pinout/mcu-pinout.md`, resolved the panel resolution (450×600), added the H7 DMA/cache memory rules, made the toolchain CLI-buildable, and locked decoder choices.

> **Container note (audio):** MP4/M4A is a *container*; for audio it holds an **AAC** stream. "Plays MP4" for audio = decode AAC. Launch formats = **FLAC · MP3**; **AAC is deferred to Phase 8** (decision D3 below).
>
> **Video note (music videos):** The H743 has **no hardware H.264 decoder**, so raw `.mp4` (H.264) video cannot be played natively. Video is delivered **iPod-style: transcode-on-load to MJPEG** on the desktop/companion app, then the device plays MJPEG using the H743's **hardware JPEG decoder** + DMA2D + QSPI display, with the audio track (MP3/AAC) synced through the normal DAC path. See **§8.5**. Deferred to **Phase 9** — hardware fully supports it, no board/pin change.

---

## 1. Goals
A premium pocket music player: play **FLAC / MP3** (AAC later) from internal eMMC + microSD, AMOLED UI, nav dial + buttons, LED-bar visualizer, clean soft-power, bit-perfect audio path (PCM5102A → OPA1622), USB-C for transfer + DFU + charging. Target ≥ 5 hr screen-on battery (2000 mAh).

## 2. Document precedence
1. **`pinout/mcu-pinout.md`** — pin authority (CubeMX export). If this spec and the pinout disagree, the pinout wins.
2. `schematic-capture-status.md` — what is actually wired (parts + nets).
3. This spec — behavior, architecture, build plan.

⚠ **Pins in flux — .ioc regen pending (2026-07-05):** a **USB-HS pin reshuffle** is being applied (8 signals relocate so the ULPI bus fits — PWR5V_EN, JACK_DET, CHG_STAT, PG_STAT, PWR_SW_SENSE, EMMC_RST, PWR_HOLD, QSPI_NCS; VOL_UP→PE13; + adds RESETB, BTN_BOTTOM, USART1 console). **Until CubeMX is re-exported, treat ALL pin numbers in this spec as stale — `mcu-pinout.md` (once updated) wins.** Full plan: `usb-hs-phy.md`. There is no `BTN_FN`/PE13 — that was a v1 spec error.

## 3. Target hardware
| Block | Part | Interface / signals |
|---|---|---|
| MCU | STM32H743VIT6, LQFP-100 (M7 @ 480 MHz, 2 MB flash, 1 MB RAM) | HSE 25 MHz, LSE 32.768 kHz |
| DAC | PCM5102A (2.1 Vrms, ≤384 kHz/32-bit) | **SAI1 Block A, I²S master, 3-wire — no MCLK** (DAC SCK grounded → internal BCK-PLL mode). `DAC_XSMT` = PE3 (soft-mute, active-low) |
| Amp | OPA1622, fixed ×1.5, ±5 V (TPS65133) | `AMP_EN` PA2, `PWR5V_EN` PA3 |
| Display | 2.41" AMOLED **450×600 RGB**, RM690B0 on-flex (Startek KD024EGOIN152-01) | **QUADSPI** (CLK PB2, NCS PB10, IO0–3 PD11/PD12/PE2/PD13) + `DISP_TE` PD14 (EXTI), `DISP_RST` PD10; PMIC TPS65632 via `PMIC_EN` PD8, `PMIC_CTRL` PD9 |
| Storage A | eMMC 32 GB, Samsung KLMBG2JETD-B041, 3.3 V VCCQ | **SDMMC2**, MMC 4-bit (CK PC1, CMD PA0, D0–3 PB14/PB15/PB3/PB4), `EMMC_RST` PB13 |
| Storage B | microSD (Hirose DM3AT) | **SDMMC1**, SD 4-bit (CK PC12, CMD PD2, D0–3 PC8–PC11), `SD_CD` PC6 (EXTI) |
| Dial | AS5600 angle sensor, back of PCB, 6 mm diametric magnet through-board | **I²C1** (PB6/PB7) @ 0x36 — navigation/scroll only |
| LED bar | LP5012 (RUK) → 8× white 0603, anodes on +VSYS_REG | I²C1 @ 0x14 *(verify address + pull-ups when Controls sheet closes)* |
| Buttons | TL3305 tactile ×7 | active-LOW, pull-up, EXTI-falling: `BTN_PLAY` PE7 (= dial-center dome), `BTN_NEXT` PE8, `BTN_PREV` PE9, `BTN_MENU` PE10, `VOL_UP` PE11, `VOL_DOWN` PE12, `BTN_BOTTOM` PE15 *(pending CubeMX)* |
| Charger | BQ24075 (charges with MCU off) | `CHG_STAT` PB0, `PG_STAT` PB1 (inputs, pull-up) |
| Power | TPS63070 buck-boost + soft latch | `PWR_SW_SENSE` PB11 (in, active-HIGH, no internal pull), **`PWR_HOLD` (out — pin TBD, latch circuit not yet wired)** |
| Jack | 3.5 mm with detect | `JACK_DET` PA5 (EXTI) |
| USB | USB-C, **OTG_HS via external ULPI PHY (USB3343)** — 480 Mbit/s. `USB_OTG_HS_ULPI_*` 12-pin bus; PHY `RESETB` = PA11. *(Old OTG_FS on PA11/PA12 removed — see `usb-hs-phy.md`.)* | MSC (transfer, ~24 MB/s) + system-ROM DFU (BOOT0 button) |
| Battery | 2000 mAh LiPo | `BAT_SENSE` ADC1_IN3 PA6, resistor divider |
| Debug | SWD header J5 (2×5, 1.27 mm) | PA13 SWDIO, PA14 SWCLK |

## 4. Toolchain & repo layout (CLI-first, agent-friendly)
- **Language:** C11. **Config:** STM32CubeMX 6.17 (`.ioc` exists) — regenerate to add PWR_HOLD/BTN_BOTTOM, HSI48+CRS, SAI PLLs.
- **Build:** CubeMX **CMake** project output (not CubeIDE-managed make) → `arm-none-eabi-gcc` + CMake + Ninja, buildable/flashable from the terminal so the coding agent can compile-test-iterate without a GUI.
- **Install (macOS):** **STM32CubeCLT** (one bundle: arm-none-eabi toolchain, CMake integration, STM32CubeProgrammer CLI, ST-LINK GDB server) + **STM32CubeMX** + optional **VS Code + STM32 extension** for GUI debugging. STM32CubeIDE is *not* required.
- **Flash/debug:** SWD via ST-LINK (buy: **STLINK-V3MINIE**, ~$12) on J5 — primary during development. USB DFU (BOOT0 + STM32CubeProgrammer) = field/recovery path only; do not develop over DFU.
- **RTOS:** FreeRTOS (CMSIS-RTOS2 via CubeMX).
- **Layout:** `firmware/cubemx/` (.ioc + generated Core/Drivers) · `firmware/app/` (modules below, never touched by CubeMX regen) · `firmware/lib/` (dr_flac, minimp3, LVGL, FatFs config) · `docs/`.

## 5. Software stack (locked)
| Layer | Choice | Rationale |
|---|---|---|
| FLAC | **dr_flac** (single-file, public domain) | streaming + seek API, 16/24-bit, low RAM, proven on Cortex-M. Replaces the foxenflac-vs-libFLAC question. |
| MP3 | **minimp3** (CC0) | single-file, fixed-point-friendly, gapless-aware (delay/padding info). |
| AAC (Phase 8) | Helix AAC + minimal MP4/M4A demuxer | RealNetworks RPSL license is murky — fine for a personal device, revisit if it ever ships. |
| Tags | ID3v2 (MP3) + FLAC Vorbis comments/PICTURE — small custom parsers | full tag libs are bloat; we need title/artist/album/art only. |
| Filesystem | FatFs, `FF_FS_EXFAT=1` | exFAT is a compile switch; MS patent licensing is a commercial-product concern, not a hobby one. Long-file-name + UTF-8 on. |
| GUI | LVGL v9, RGB565 | partial render buffers (forced — see §6). |
| Display driver | custom RM690B0 QSPI init + LVGL flush cb, TE-synced, DMA | port the init sequence from LilyGO `LilyGo-AMOLED-Series` (same RM690B0). |

## 6. H7 memory & DMA rules ⚠ (read before writing any driver)
The #1 H743 time-sink. Non-negotiable rules:
1. **DMA1/DMA2 cannot access DTCM** (0x2000_0000). Any buffer touched by SAI-DMA, SPI-DMA, etc. lives in **D2 SRAM1/2 (0x3000_0000)** or **AXI SRAM (0x2400_0000)** — never DTCM.
2. **D-cache vs DMA:** either (a) MPU-configure the DMA-buffer regions **non-cacheable**, or (b) `SCB_CleanDCache_by_Addr` before TX / `InvalidateDCache_by_Addr` after RX, with all DMA buffers **32-byte aligned and 32-byte-multiple sized**. Pick (a) for the audio ring + SDMMC transfers; it removes a whole bug class.
3. SDMMC uses its own IDMA — same constraint: buffers in AXI or D2, not DTCM.
4. **Memory budget** (1 MB total is fragmented: 512 K AXI + 128+128+32 K D2 + 64 K D3 + 128 K DTCM + 64 K ITCM):
   - Full 450×600 RGB565 frame = **540 KB > 512 KB AXI → a full framebuffer is impossible.** LVGL uses 2× partial buffers of ~450×60 px (≈54 KB each) in AXI, DMA-flushed to the RM690B0's internal GRAM.
   - PCM ring: 2× 8–16 KB ping-pong in D2 SRAM1 (non-cacheable region).
   - Decode scratch, FatFs work areas: AXI. Stacks/heap hot data: DTCM (fast, no DMA needed there).
5. Put the RTOS heap and task stacks in DTCM; keep ISRs short — decode work happens in `audio_task`, never in the DMA callback.

## 7. Audio pipeline
`file (eMMC/SD) → FatFs → decode (dr_flac/minimp3) → PCM ring (D2, non-cacheable) → SAI1-DMA circular → PCM5102A → OPA1622 → jack`
- **Clocking (fixes the pinout doc's two ⚠ items):** USB kernel = **HSI48 + CRS** (SOF-trimmed). SAI1 kernel from two fractional PLLs — **PLL2P ≈ 45.1584 MHz** (44.1/88.2/176.4 k) and **PLL3P ≈ 49.152 MHz** (48/96/192 k); switch the SAI1 kernel mux per track. CubeMX solves the fractional-N values; verify BCLK = 64×fs exactly.
- **No MCLK by design** — PCM5102A runs its internal PLL from BCK; SAI master-clock output stays disabled.
- **Pop-free sequencing:** power-on: `PWR5V_EN`→ settle → SAI clocks valid → `DAC_XSMT` high (unmute) → `AMP_EN`. Stop/shutdown: exact reverse. Never enable the amp with the DAC muted-off or clocks stopped.
- **Volume:** digital only (DAC is fixed 2.1 Vrms): 32 steps, log taper, applied as 32-bit multiply in the decode stage before truncation to the SAI word. Vol± buttons; LED bar mirrors level (8 LEDs ← 32 steps).
- **Buffering:** decode ≥250 ms ahead; refill on SAI half/complete DMA callbacks via task notification; on underrun, mute (XSMT) rather than loop stale samples.
- **Seek:** dr_flac native seek; MP3 via ID3/Xing TOC when present, else byte-estimate.

## 8. Display pipeline
- **450×600 RGB565** over QUADSPI to RM690B0 GRAM (controller refreshes the panel itself; we only push dirty regions).
- QSPI at the max the panel/traces allow (start 40 MHz, try 60+): full-frame push ≈ 540 KB ≙ ~18 ms @ 60 MHz-quad — partial LVGL updates make typical frames far cheaper.
- Sync flushes to `DISP_TE` (EXTI) to avoid tearing on large updates; small updates can skip TE waiting.
- Brightness via RM690B0 command (0x51); panel PMIC on/off via `PMIC_EN`/`PMIC_CTRL` for true display-off sleep (AMOLED: black pixels ≈ free, favor dark UI).

## 8.5 Album-art UI & video (MJPEG)
Three features share the same hardware trio — **AMOLED + hardware JPEG decoder + DMA2D** — so they reuse one code path.

**Album art (Phase 6, core UI):**
- Art source: embedded in tags — ID3v2 `APIC` (MP3), FLAC `METADATA_BLOCK_PICTURE` (usually JPEG); fallback `folder.jpg`.
- Decode covers with the **hardware JPEG peripheral** (not software) → scale via **DMA2D** → LVGL image.
- **Thumbnail cache:** `storage_task` pre-decodes/scales covers to a small cached size and persists them (cache file on eMMC) so album-grid scrolling doesn't re-decode full JPEGs each frame. Cache keyed by file path + mtime.
- Screens that use art: now-playing (large cover), album/artist browser (thumbnail grid), lock/sleep screen. Favor a dark UI (AMOLED blacks ≈ free).
- ⚠ RAM: decode + scale one cover at a time into an AXI scratch buffer (see §6); never hold many full-size decoded covers.

**Video — MJPEG, transcode-on-load (Phase 9):**
- **No native H.264.** Desktop/companion app transcodes MP4 → an **MJPEG container** (e.g. `.avi`/`.mov`: MJPEG video + MP3/AAC audio track) sized to the panel (~450×600), ~24 fps. iPod-model: the device never decodes raw H.264.
- Playback pipeline: `file → demux → [video] HW-JPEG decode → DMA2D scale → QSPI blit  ‖  [audio] MP3/AAC decode → PCM ring → SAI1 → DAC`.
- **A/V sync** is the hard part: time video-frame presentation to the audio clock (audio is the master); drop/duplicate frames to stay locked. This is the trickiest bit of the feature — budget for it.
- **Storage cost:** MJPEG is ~4–5× larger than H.264. A 3-min clip ≈ **~150 MB** at 24 fps/native (tunable ~80 MB at 15 fps/lower quality). Audio track ≈ 3–6 MB (negligible). 32 GB eMMC ≈ ~200 clips — a "favorites" feature, not a whole video library.
- **No hardware or pin change** — uses the JPEG codec, DMA2D, QSPI display, and DAC path already present.

## 9. USB
- **MSC (song transfer):** on VBUS attach → finish writes, **unmount FatFs**, expose eMMC + SD as two raw MSC LUNs; remount + rescan library on detach. FatFs and the host must never own a volume simultaneously.
- **Speed (USB High-Speed via external ULPI PHY — USB3343):** OTG_HS at 480 Mbit/s. Real MSC throughput **~24 MB/s (eMMC-write-limited, not USB-limited); 32 GB ≈ ~22 min.** Requires an **efficient bridge**: DMA between USB and SDMMC, double-buffered, D-cache clean/invalidate per §6 — a naive byte-copy bridge falls to ~10–15 MB/s. microSD-in-a-card-reader remains the fastest bulk path. *(Reverses the earlier FS-only decision — see `usb-hs-phy.md`.)*
- **DFU:** system-ROM bootloader via BOOT0 button — zero firmware code needed; document the STM32CubeProgrammer command in the README.
- Charging is hardware (BQ24075) — works regardless of MCU state.

## 10. Task architecture (FreeRTOS / CMSIS-RTOS2)
| Task | Prio | Job |
|---|---|---|
| `audio_task` | highest app | decode → ring; woken by SAI DMA half/full notifications; the only real-time path |
| `ui_task` | med | LVGL tick/render, screens (now-playing, browser, settings) |
| `input_task` | med+ | button debounce (EXTI + timer), AS5600 poll ~100 Hz with hysteresis → event queue |
| `storage_task` | low | library scan/index on eMMC+SD (background), tag + art extraction, persisted index file |
| `power_task` | med | latch (`PWR_HOLD` high **first line of `main()`**, before HAL_Init), long-press shutdown, sleep/wake, ADC battery filter, charge status |
| `led_task` | low | LP5012 frames: volume / battery / charge animation |
| USB | event-driven | MSC attach/detach state machine |
Inter-task: one input-event queue; audio commands (play/seek/stop) as a small command queue to `audio_task`; LVGL calls from `ui_task` only.

## 11. Power behavior
- Press power → latch holds (`PWR_HOLD` high immediately in `main()`); short-press = display sleep/wake; long-press (≥2 s) = clean shutdown: save state → unmute-reverse sequence → flush FS → `PWR_HOLD` low.
- Sleep: display PMIC off, MCU STOP2-ish low-power w/ EXTI wake (buttons, `PWR_SW_SENSE`, `SD_CD`, VBUS); keep playing-with-screen-off as the common battery mode.
- Resume-where-you-left-off: persist track + position + volume to a settings file on eMMC on pause/shutdown.
- Battery %: ADC on `BAT_SENSE` through divider, median-filtered, LiPo curve LUT; charge state from `CHG_STAT`/`PG_STAT`.
- ⚠ Latch hardware (BQ24075 SYSOFF + button + hold GPIO) is **not yet wired** — firmware bring-up (Phase 1) can run on the bench supply/slide switch until the Controls sheet closes.

## 12. Phased build plan (each phase has a pass/fail test)
1. **Bring-up:** clocks (480 MHz, HSI48+CRS, PLL2/PLL3 audio), FreeRTOS boots, SWD + UART printf, buttons readable. ✔ LED-less blinky + button events over UART.
2. **Storage:** SDMMC1+2, FatFs mounts both, exFAT on. ✔ recursive file list of both volumes over UART.
3. **Audio out:** SAI+DMA ring plays a generated 1 kHz sine @ 44.1 k and 48 k (both PLLs), XSMT/AMP sequencing. ✔ clean tone, no pops on start/stop.
4. **Decoders:** WAV → **FLAC (dr_flac)** → **MP3 (minimp3)**; per-track sample-rate switch; volume. ✔ plays a 24/96 FLAC bit-perfect and a VBR MP3, seek works.
5. **Display:** RM690B0 init (port from LilyGO), LVGL partial-buffer flush, TE sync. ✔ 60 fps LVGL demo widget, no tearing.
6. **UI:** browser (folder + tag views from `storage_task` index), now-playing with art, dial+buttons nav. ✔ pick and play a song entirely on-device.
7. **System:** LED bar, battery/charge, USB MSC (mount-swap logic), settings persistence, jack-detect pause. ✔ drag a song in over USB, unplug, play it.
8. **Polish:** gapless (decoder delay/padding trim), sleep/wake, brightness, low-power tuning (try 240 MHz — D2), **AAC if D3 says yes**.
9. **Video (MJPEG):** demux MJPEG container → HW-JPEG decode → DMA2D → QSPI, audio-mastered A/V sync (§8.5). ✔ a transcoded 3-min music video plays with lips/beats in sync, audio clean. *(Also enrich album-art: HW-JPEG covers + thumbnail cache — the art path lands earlier in Phase 6, video reuses it.)*

## 13. Open decisions
| ID | Decision | Owner / when |
|---|---|---|
| D1 | `PWR_HOLD` pin + latch circuit (Controls sheet) → then regenerate .ioc + pinout doc, incl. `BTN_BOTTOM` PE15 | Micah, schematic time — **blocks Phase 1 power code only** |
| D2 | SYSCLK 480 vs 240 MHz for battery | measure in Phase 8; bring up at 480 |
| D3 | AAC at launch? | Micah — default **no** (FLAC+MP3 launch), revisit Phase 8 |
| D4 | LP5012 addr/pull-ups confirmation | falls out of Controls sheet completion |
| D5 | Battery curve: divider LUT now; fuel-gauge IC only if % proves annoying | Phase 7 |
| D6 | TPS63070 PS/SYNC tied to EN (forced PWM = clean audio, worse idle draw) — GPIO mod only if idle battery disappoints | post-P1 hardware rev, if ever |

## 14. Cross-references
- Pins (authority): `pinout/mcu-pinout.md` · CubeMX: `cubemx-setup.md`
- Schematic: `MP3 - Simris.kicad_sch` · status: `schematic-capture-status.md`
- Parts: `component-list.md`, `library-sourcing.md` · Enclosure: `enclosure-sizing.md`
- RM690B0 reference code: github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series (init + QSPI write path)
