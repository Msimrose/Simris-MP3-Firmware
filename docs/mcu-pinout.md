# Pact MP-1 — STM32H743VIT6 Pinout (from CubeMX)

> ⚠️ **PENDING CHANGE (2026-07-05): USB High-Speed locked in.** Adding an external ULPI PHY forces an
> **8-signal pin reshuffle** (ULPI claims PA3/PA5/PB0/PB1/PB5/PB10–13/PC0/PC2/PC3; PWR5V_EN, JACK_DET,
> CHG_STAT, PG_STAT, PWR_SW_SENSE, EMMC_RST, VOL_UP, and QSPI_NCS relocate). **This table is not yet
> updated — apply the reshuffle in CubeMX and re-export.** Full plan: [`../usb-hs-phy.md`](../usb-hs-phy.md).


**Source of truth for the MCU schematic sheet** (satisfies hard rule #3 — pins assigned by CubeMX, not by hand).
Exported from `micahsimrose.ioc` (CubeMX 6.17), 2026-06-14. Core @ 480 MHz, all peripherals fit the LQFP-100
with ~43 GPIO to spare. Connect each net to the MCU symbol pin of the given **name** (KiCad's `STM32H743VITx`
symbol is labelled by pin name).

## Clocks
| Pin | Function |
|---|---|
| PH0 | RCC_OSC_IN (25 MHz HSE) |
| PH1 | RCC_OSC_OUT |
| PC14 | RCC_OSC32_IN (32.768 kHz LSE) |
| PC15 | RCC_OSC32_OUT |

## Debug
| PA13 | SWDIO · | PA14 | SWCLK |
|---|---|---|---|

## Audio — SAI1 (I²S, 3-wire, no MCLK) → PCM5102A
| Pin | Function | DAC pin |
|---|---|---|
| PE4 | SAI1_FS_A | LRCK |
| PE5 | SAI1_SCK_A | BCK |
| PE6 | SAI1_SD_A | DIN |

## Display — QUADSPI → RM690B0 FPC
| Pin | Function | FPC pin |
|---|---|---|
| PB2 | QUADSPI_CLK | 20 (SCL) |
| PB10 | QUADSPI_BK1_NCS | 21 (CSX) |
| PD11 | QUADSPI_BK1_IO0 | 18 (SDI_SDA) |
| PD12 | QUADSPI_BK1_IO1 | 19 (DCX) |
| PE2 | QUADSPI_BK1_IO2 | 22 (D0) |
| PD13 | QUADSPI_BK1_IO3 | 23 (D1) |

## microSD — SDMMC1 (SD, 4-bit) → TF-015
| PC12 CK · PD2 CMD · PC8 D0 · PC9 D1 · PC10 D2 · PC11 D3 |
|---|

## eMMC — SDMMC2 (MMC, 4-bit) → Samsung KLMBG2JETD-B041 (part changed from MKEMF032GT1E 2026-06-22; pins unchanged)
| PC1 CK · PA0 CMD · PB14 D0 · PB15 D1 · PB3 D2 · PB4 D3 |
|---|

## USB — USB_OTG_FS (Device) → USB-C
| PA11 USB_OTG_FS_DM (D−) · PA12 USB_OTG_FS_DP (D+) |
|---|

## I²C1 → AS5600 navigation dial (scroll/select only — volume is now 2 buttons)
| PB6 I2C1_SCL · PB7 I2C1_SDA |
|---|

## ADC1 → battery sense
| PA6 ADC1_IN3 |
|---|

## GPIO — outputs (default Low)
| Pin | Net |
|---|---|
| PE3 | DAC_XSMT |
| PA2 | AMP_EN |
| PA3 | PWR5V_EN |
| PD8 | PMIC_EN |
| PD9 | PMIC_CTRL |
| PD10 | DISP_RST |
| PB13 | EMMC_RST |

## GPIO — interrupt inputs (EXTI, pull-up)
| Pin | Net | EXTI |
|---|---|---|
| PE7 | BTN_PLAY | 7 |
| PE8 | BTN_NEXT | 8 |
| PE9 | BTN_PREV | 9 |
| PE10 | BTN_MENU | 10 |
| PE11 | VOL_UP | 11 |
| PE12 | VOL_DOWN | 12 |
| PD14 | DISP_TE | 14 |
| PC6 | SD_CD | 6 |
| PA5 | JACK_DET | 5 |

## GPIO — inputs (pull-up)
| PB0 CHG_STAT · PB1 PG_STAT · PB11 PWR_SW_SENSE |
|---|

## Power / system pins (LQFP-100 fixed — not in CubeMX list)
VDD ×5, VSS ×5, **VCAP ×2 (2.2 µF each)**, VDDA, VREF+, VSSA, VBAT, NRST, BOOT0. (Decoupling per `schematic-design-reference.md` §2.)

## Clock tree (480 MHz)
- HSE 25 MHz → PLL1 (DIVM1=5 → 5 MHz, DIVN1=192 → VCO 960 MHz, /2) → **SYSCLK 480 MHz**.
- AHB/AXI 240 MHz · APB 120 MHz · RTC from LSE · HAL timebase = TIM6.

### ⚠ Two clock items to finish at firmware time (do NOT affect the schematic)
1. **USB clock = 64 MHz now — must be 48 MHz.** Fix via **HSI48 + CRS** (auto-trims to USB SOF) when generating firmware.
2. **SAI1 clock = 64 MHz (30 % error)** — set PLL2/PLL3 for the 44.1 kHz + 48 kHz audio families when generating firmware.

Both are clock-tree (firmware) settings; the pinout above is final and feeds the schematic as-is.
