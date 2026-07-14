/*
 * Pact MP-1 - RM690B0 QSPI AMOLED driver (Startek KD024EGOIN152-01).
 *
 * Init sequence = the OFFICIAL Startek spec section 6.3 "Power on Initial
 * Code For MCU" (KD024EGOIN152-01 SPEC V0.pdf) - NOT the LilyGO port; the
 * panel spec supersedes it. Only two additions: COLMOD 0x3A = RGB565 (the
 * official code leaves the 24-bit default; LVGL renders 16-bit) and
 * MADCTL 0x36 for the landscape mount.
 *
 * QSPI protocol (RM690B0 MCU/QSPI mode, IM=10 on the schematic):
 *   command:  instruction 0x02 (1 line) + 24-bit address (cmd << 8,
 *             1 line) + parameter bytes (1 line)
 *   pixels:   instruction 0x32 (1 line) + address 0x002C00 (1 line) +
 *             pixel stream (4 lines), one CS-low burst per flush
 *
 * Geometry: panel is 450x600 native portrait with a 16-column offset
 * (CASET 0x0010..0x01D1). The UI renders 600x450 landscape; MADCTL MV
 * maps it in hardware (zero CPU). ⚠ BRING-UP: the MADCTL value (0x60 vs
 * 0xA0 picks which way is "up") and which axis carries the +16 offset
 * (LCD_X_OFF/LCD_Y_OFF below) must be verified on the panel - a shifted
 * or mirrored image means flip those two constants, nothing else.
 *
 * Flush: LVGL partial buffers in AXI (QSPI is fed by the CPU for now -
 * polling HAL, ~2.9 ms per 48-row band at 40 MHz quad). MDMA is the
 * planned perf step; TE-synced flushes (PE14 EXTI) come with it.
 * LVGL renders classic RGB565; the panel wants big-endian, so each band
 * is byte-swapped in place (lv_draw_sw_rgb565_swap) before transmit.
 *
 * Power path (TPS65632 via PMIC_EN/PMIC_CTRL, reset on DISP_RST):
 * rails before reset release, reset before init, per spec section 6.1.
 * Bench note: our TPS65632 gives ELVDD 4.6 V fixed - inside the panel's
 * 2.0-6.0 V range but above the 3.6 V typical; watch panel heat on
 * first power-up.
 */
#ifndef PACT_SIM

#include "hal/pact_display.h"
#include "pact_mem.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "src/draw/sw/lv_draw_sw_utils.h"

#define LCD_HOR       600           /* landscape, as the UI renders */
#define LCD_VER       450
#define LCD_X_OFF     0             /* +16 native-column offset lands on Y */
#define LCD_Y_OFF     16            /* in landscape; swap if image shifts  */
#define MADCTL_LAND   0x60          /* MV|MX; 0xA0 = the other way up      */

#define DISP_BUF_ROWS 48            /* 600x48 RGB565 = 57.6 KB per buffer  */

#define QSPI_TO_MS    250u
#define PRESCALER_40M 5u            /* 240 MHz kernel / (5+1) = 40 MHz;
                                       try 3 (60 MHz) once flexes are known */

extern QSPI_HandleTypeDef hqspi;

PACT_AXI static uint8_t disp_buf1[LCD_HOR * DISP_BUF_ROWS * 2];
PACT_AXI static uint8_t disp_buf2[LCD_HOR * DISP_BUF_ROWS * 2];

static lv_display_t *disp;

/* ---- QSPI transport ------------------------------------------------------ */

static bool qspi_cmd(uint8_t cmd, const uint8_t *data, size_t len)
{
    QSPI_CommandTypeDef c = {0};
    c.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    c.Instruction     = 0x02;
    c.AddressMode     = QSPI_ADDRESS_1_LINE;
    c.AddressSize     = QSPI_ADDRESS_24_BITS;
    c.Address         = (uint32_t)cmd << 8;
    c.DataMode        = len ? QSPI_DATA_1_LINE : QSPI_DATA_NONE;
    c.NbData          = len;
    c.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
    if (HAL_QSPI_Command(&hqspi, &c, QSPI_TO_MS) != HAL_OK)
        return false;
    if (len && HAL_QSPI_Transmit(&hqspi, (uint8_t *)data, QSPI_TO_MS) != HAL_OK)
        return false;
    return true;
}

static bool qspi_pixels(const uint8_t *px, size_t len)
{
    QSPI_CommandTypeDef c = {0};
    c.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    c.Instruction     = 0x32;
    c.AddressMode     = QSPI_ADDRESS_1_LINE;
    c.AddressSize     = QSPI_ADDRESS_24_BITS;
    c.Address         = 0x002C00;               /* RAMWR on the quad lane */
    c.DataMode        = QSPI_DATA_4_LINES;
    c.NbData          = len;
    c.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
    if (HAL_QSPI_Command(&hqspi, &c, QSPI_TO_MS) != HAL_OK)
        return false;
    return HAL_QSPI_Transmit(&hqspi, (uint8_t *)px, QSPI_TO_MS) == HAL_OK;
}

#define CMD(op, ...)                                                     \
    do {                                                                 \
        static const uint8_t p_[] = { __VA_ARGS__ };                     \
        if (!qspi_cmd((op), p_, sizeof p_)) return false;                \
    } while (0)
#define CMD0(op)                                                         \
    do { if (!qspi_cmd((op), NULL, 0)) return false; } while (0)

static void delay_ms(uint32_t ms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskDelay(pdMS_TO_TICKS(ms));
    else
        HAL_Delay(ms);
}

/* ---- panel window + flush ------------------------------------------------ */

static bool set_window(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    x0 += LCD_X_OFF; x1 += LCD_X_OFF;
    y0 += LCD_Y_OFF; y1 += LCD_Y_OFF;
    uint8_t ca[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t ra[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    return qspi_cmd(0x2A, ca, 4) && qspi_cmd(0x2B, ra, 4);
}

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px_map)
{
    size_t px = (size_t)lv_area_get_width(area) * lv_area_get_height(area);
    lv_draw_sw_rgb565_swap(px_map, px);         /* panel wants big-endian */
    if (set_window(area->x1, area->y1, area->x2, area->y2))
        qspi_pixels(px_map, px * 2);
    lv_display_flush_ready(d);
}

/* ---- bring-up ------------------------------------------------------------ */

static bool qspi_reinit(void)
{
    /* MX_QUADSPI_Init is a CubeMX placeholder (prescaler 255, 4-byte
     * FlashSize); the panel is not a flash chip, so own the config here -
     * same pattern as the SAI driver. */
    HAL_QSPI_DeInit(&hqspi);
    hqspi.Init.ClockPrescaler     = PRESCALER_40M;
    hqspi.Init.FifoThreshold      = 4;
    hqspi.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_NONE;
    hqspi.Init.FlashSize          = 23;         /* 24-bit address space */
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_4_CYCLE;
    hqspi.Init.ClockMode          = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID            = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash          = QSPI_DUALFLASH_DISABLE;
    return HAL_QSPI_Init(&hqspi) == HAL_OK;
}

static bool panel_power_on(void)
{
    /* Rails stable before reset release (spec section 6.1): logic rails
     * (VCI/VDDIO on +3V3) are always up; sequence the TPS65632 ELVDD then
     * ELVSS, then release RESX. */
    HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PMIC_EN_GPIO_Port, PMIC_EN_Pin, GPIO_PIN_SET);
    delay_ms(10);
    HAL_GPIO_WritePin(PMIC_CTRL_GPIO_Port, PMIC_CTRL_Pin, GPIO_PIN_SET);
    delay_ms(10);
    HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, GPIO_PIN_SET);
    delay_ms(50);
    return true;
}

/* Startek spec section 6.3, "Power on Initial Code For MCU", verbatim -
 * plus COLMOD/MADCTL (marked) which the official code leaves at default. */
static bool panel_init_seq(void)
{
    CMD(0xFE, 0x20);              /* manufacturer page 1 */
    CMD(0x26, 0x0A);
    CMD(0x24, 0x80);
    CMD(0xFE, 0x00);              /* user command page */
    CMD(0x3A, 0x55);              /* + COLMOD: RGB565 (LVGL is 16-bit) */
    CMD(0x36, MADCTL_LAND);       /* + MADCTL: landscape mount */
    CMD(0x2A, 0x00, 0x10, 0x01, 0xD1);   /* CASET 16..465: 16-col offset */
    CMD(0x2B, 0x00, 0x00, 0x02, 0x57);   /* RASET 0..599 */
    CMD(0x35, 0x00);              /* TE on, V-blank only */
    CMD(0x51, 0xFF);              /* brightness max */
    CMD(0x30, 0x00, 0x01, 0x02, 0x56);   /* partial area (official code) */
    CMD(0x12, 0x00);              /* partial mode on (official code) */
    CMD0(0x11);                   /* sleep out */
    delay_ms(120);
    CMD0(0x29);                   /* display on */
    return true;
}

bool pact_display_init(void)
{
    if (!qspi_reinit())
        return false;
    panel_power_on();
    if (!panel_init_seq())
        return false;

    disp = lv_display_create(LCD_HOR, LCD_VER);
    if (!disp)
        return false;
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, disp_buf1, disp_buf2, sizeof disp_buf1,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    return true;
}

void pact_display_set_brightness(uint8_t level)
{
    (void)qspi_cmd(0x51, &level, 1);
}

void pact_display_off(void)
{
    /* Spec section 6.3 power-off: display off, sleep in, 5 frames, then
     * rails down and reset low. */
    (void)qspi_cmd(0x28, NULL, 0);
    (void)qspi_cmd(0x10, NULL, 0);
    delay_ms(85);
    HAL_GPIO_WritePin(PMIC_CTRL_GPIO_Port, PMIC_CTRL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PMIC_EN_GPIO_Port, PMIC_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, GPIO_PIN_RESET);
}

#endif /* !PACT_SIM */
