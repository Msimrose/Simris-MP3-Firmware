# Pact MP-1 — Firmware Handoff (for Fable)

You're building the firmware for the **Pact MP-1**: a pocket audiophile FLAC/MP3 player
(STM32H743VIT6, 2.4" AMOLED, wheel + buttons, hi-fi DAC/amp). The hardware config and a buildable
CMake project are **done** — your job is the **application**.

---

## 0. Read these first (in `docs/`)
1. **`firmware-spec.md`** — the technical spec: RTOS, decoders, H7 memory/DMA rules, the **phased build
   plan (§12)**, and per-phase pass/fail tests. **START HERE.**
2. **`software-ui-spec.md`** — app architecture + UI behavior. Pins are stubbed behind a HAL; the app
   handles *named events* ("Next pressed"), never pin numbers. Includes the **album carousel**.
3. **`brand-ui-system.md`** — the visual language: **monochrome, landscape 600×450**, ABC Favorit Light,
   Phosphor icons, dynamic battery. Only album art + battery get color.
4. **`mcu-pinout.md`** — pin reference (the *authoritative* pins are in `MP3_Firmware.ioc`).

## 1. What's already done — DO NOT redo
- **CubeMX config** (in `MP3_Firmware.ioc`): all peripherals, pins, EXTI, NVIC.
- **Clocks:** 480 MHz core; SAI1 audio = 44.1 kHz @ **0% error** (PLL2P 11.2896 MHz / PLL3P 12.288 MHz).
- **SAI1 circular DMA** (DMA1_Stream1, word, high priority) — the audio ring.
- **FreeRTOS** (CMSIS-RTOS2), **I-Cache + D-Cache** enabled, HAL timebase on **TIM6**.
- **Generated:** `Core/` (peripheral inits + main.c), `Drivers/` (HAL), `Middlewares/` (FreeRTOS),
  `CMakeLists.txt`, `CMakePresets.json`, linker + startup.

## 2. Your job — the app (libraries CubeMX does NOT provide)
Add these yourself, per the phased plan:
- **LVGL v9** — the UI (RGB565, partial render buffers — see firmware-spec §6)
- **FatFs** (exFAT) + diskio glue to SDMMC1 (µSD) & SDMMC2 (eMMC)
- **USB Device + MSC class** — dual LUNs (eMMC + SD), unmount FatFs while host owns the volume
- **dr_flac + minimp3** — decoders (AAC/Helix deferred to Phase 8)
- **RM690B0 display driver** — QSPI init + LVGL flush cb, TE-synced (port from LilyGO)
- **MPU regions** for DMA-buffer cache coherency (firmware-spec §6)

Later phases: MJPEG **video** (§8.5), album-art via the hardware JPEG decoder.

## 3. Build & flash (once STM32CubeCLT is installed + on PATH)
```bash
cd ~/Documents/MP3_Firmware
# configure + build (use the presets CubeMX generated — check CMakePresets.json for names):
cmake --preset Debug
cmake --build --preset Debug
# flash over SWD (ST-LINK/V2 + Olimex adapter on J5):
STM32_Programmer_CLI -c port=SWD -w build/Debug/MP3_Firmware.elf -rst
```
*(Toolchain = arm-none-eabi-gcc + Ninja, all from STM32CubeCLT. Build-test-iterate from the terminal.)*

## 4. Critical constraints (firmware-spec §6 has the detail)
- **DMA buffers → AXI or D2 SRAM, NEVER DTCM.** DMA1/2 physically can't reach DTCM on the H7.
- **D-Cache is ON** → handle coherency: MPU non-cacheable regions for DMA buffers, or clean/invalidate
  around transfers (32-byte aligned). Pick MPU-non-cacheable for the audio ring + SDMMC — removes a bug class.
- **Never hardcode pins.** They live in the `.ioc`/generated code. Work through the HAL; if a pin must
  change, it's a CubeMX regen, not a magic number in app code.
- **Framebuffer:** a full 600×450 RGB565 frame is 540 KB > 512 KB AXI → **partial LVGL buffers only.**
- **Audio:** SAI1 defaults to 44.1 kHz. For other rates, switch the SAI1 kernel mux (PLL2P ↔ PLL3P) and
  MCKDIV at runtime. No MCLK (PCM5102A runs its PLL off BCLK).
- **UI is landscape, monochrome** — art + battery are the only color.

## 5. Phased plan (firmware-spec §12 — each phase has a pass/fail test)
1. Bring-up (clocks, FreeRTOS, **UART/RTT console**, buttons) — SWO is blocked (PB3=eMMC), so use RTT or a UART.
2. Storage (SDMMC1+2, FatFs, exFAT)
3. Audio out (SAI DMA ring, 1 kHz sine, pop-free XSMT/AMP sequencing)
4. Decoders (WAV → FLAC → MP3, per-track rate switch, volume)
5. Display (RM690B0 init, LVGL partial flush, TE sync)
6. UI (browser, now-playing, **album carousel**, dial + buttons)
7. System (LED bar, battery, USB-MSC, settings persistence)
8. Polish (gapless, sleep/wake, brightness, AAC if approved)
9. Video (MJPEG transcode-on-load)

## 6. Ground rules
- **Keep app code out of CubeMX's way:** put your modules under `Core/Src` app files or a new `App/` dir,
  inside `/* USER CODE */` blocks where CubeMX regen preserves them. A regen must not clobber your work.
- **Commit per phase** on branch `v1.1` (or feature branches off it). Repo remote:
  `git@github.com:Msimrose/Simris-MP3-Firmware.git`.
- **Verify by running**, not just compiling — each phase's pass/fail test is the bar.

---
**TL;DR:** the board's config and a clean CMake project are done. Read `docs/firmware-spec.md`, install
STM32CubeCLT, build to confirm it compiles, then work the phased plan starting at Phase 1. Ask the owner
(Micah) on any hardware ambiguity — the schematic is the ground truth.
