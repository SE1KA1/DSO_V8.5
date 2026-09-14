/**
  ******************************************************************************
  * @file    main.c
  * @brief   Course experiment LCD panel on top of the minimum LCD bring-up chain
  ******************************************************************************
  */

#include "main.h"

const char *get_mode_name(uint8_t mode);
const char *get_mode_short_name(uint8_t mode);
const char *get_frequency_name(uint8_t clk_sel);

/* -- M3 Material Design 3 Surface elevation palette (dark theme) -- */
#define UI_SURFACE                          0x070B10U  /* elevation 0 - page background */
#define UI_SURFACE_CONTAINER_LOW            0x16212BU  /* elevation 1 - main card */
#define UI_SURFACE_CONTAINER_LOWEST         0x213140U  /* elevation 1 darker - panel inside card */
#define UI_SURFACE_CONTAINER_HIGH           0x101820U  /* elevation 3 - header bar */
#define UI_M3_PRIMARY                       0x00BCD4U  /* M3 Primary - accent / status highlight */
#define UI_M3_TERTIARY                      0xFFC107U  /* M3 Tertiary - warn accent */
#define UI_FREQ_FULL_SELECT                 0x42A5F5U  /* Blue - frequency full-select mode */

/* -- Backward-compatible aliases -- */
#define UI_BG_COLOR             UI_SURFACE
#define UI_HEADER_BG            UI_SURFACE_CONTAINER_HIGH
#define UI_CARD_BG              UI_SURFACE_CONTAINER_LOW
#define UI_CARD_ALT_BG          UI_SURFACE_CONTAINER_LOWEST
#define UI_ACCENT_COLOR         UI_M3_PRIMARY
#define UI_WARN_ACCENT_COLOR    UI_M3_TERTIARY

/* -- Unchanged detail colors -- */
#define UI_HEADER_SUB                       0xB8C7D1U
#define UI_CARD_CLOCK_BG                    0x121B24U
#define UI_BORDER_COLOR                     0x2E4050U
#define UI_LABEL_COLOR                      0xA8B7C3U
#define UI_VALUE_COLOR                      0xF5F8FAU
#define UI_MUTED_VALUE_COLOR                0xD5DEE6U
#define UI_STATUS_OK_BG                     0x2E7D32U
#define UI_STATUS_ERR_BG                    0xC62828U
#define UI_STATUS_WARN_BG                   0xF9A825U
#define UI_DIGIT_BG                         0x1A0B0BU
#define UI_DIGIT_BORDER                     0x7E2525U
#define UI_SEG_ON_COLOR                     0xFF2B24U
#define UI_SEG_OFF_COLOR                    0x3A1212U
#define UI_LED_ON_COLOR                     0x2E7D32U
#define UI_LED_OFF_COLOR                    0xCFD8DCU
#define UI_LED_BORDER                       0x90A4AEU
#define UI_STATUS_TEXT_Y                    18U
#define UI_CARD_TITLE_H                     24U
#define UI_HEADER_BOTTOM_Y                  56U
#define UI_REFRESH_PERIOD_MS                30U
#define SPI_SAMPLE_PERIOD_MS                1U

#define STATUS_X1                           0U
#define STATUS_Y1                           0U
#define STATUS_X2                           799U
#define STATUS_Y2                           55U

#define MODE_X1                             16U
#define MODE_Y1                             72U
#define MODE_X2                             784U
#define MODE_Y2                             350U

#define FREQ_X1                             16U
#define FREQ_Y1                             366U
#define FREQ_X2                             784U
#define FREQ_Y2                             464U

#define DATA_X1                             16U
#define DATA_Y1                             72U
#define DATA_X2                             784U
#define DATA_Y2                             350U

#define CLOCK_X1                            16U
#define CLOCK_Y1                            366U
#define CLOCK_X2                            784U
#define CLOCK_Y2                            464U

#define AUX_LABEL_Y                         396U
#define AUX_VALUE_Y                         418U
#define AUX_FREQ_X                          38U
#define AUX_FREQ_VALUE_X1                   38U
#define AUX_FREQ_VALUE_Y1                   418U
#define AUX_FREQ_VALUE_X2                   244U
#define AUX_FREQ_VALUE_Y2                   450U
#define AUX_MODE_X                          254U
#define AUX_MODE_VALUE_X1                   254U
#define AUX_MODE_VALUE_Y1                   420U
#define AUX_MODE_VALUE_X2                   534U
#define AUX_MODE_VALUE_Y2                   448U
#define AUX_KEY_LED_X                       560U
#define AUX_KEY_LED_VALUE_X1                560U
#define AUX_KEY_LED_VALUE_Y1                396U
#define AUX_KEY_LED_VALUE_X2                760U
#define AUX_KEY_LED_VALUE_Y2                450U
#define HEADER_TITLE_X                      20U
#define HEADER_TITLE_Y                      4U
#define HEADER_TITLE_TEXT_SIZE            32U
#define HEADER_RESOURCE_X                   320U
#define HEADER_RESOURCE_Y                   38U
#define STATUS_LINK_LABEL_X                 684U
#define STATUS_LINK_VALUE_X                 724U

/* -----------------------------------------------------------------------
 * LAYOUT GRID SYSTEM
 * All mode-view widgets are drawn inside the DATA card interior.
 * Changing the base offsets below repositions every mode view at once.
 * ----------------------------------------------------------------------- */

/* -- Standard 3-column grid (Modes 2, 4, 5 - identical layouts) -- */
#define L3_COL_X0                           42U
#define L3_COL_W                            204U
#define L3_COL_GAP                          52U
#define L3_COL_X1                           (L3_COL_X0 + L3_COL_W + L3_COL_GAP)  /* 298 */
#define L3_COL_X2                           (L3_COL_X1 + L3_COL_W + L3_COL_GAP)  /* 554 */
#define L3_ROW0_Y                           112U
#define L3_ROW0_H                           84U
#define L3_ROW0_BOT                         (L3_ROW0_Y + L3_ROW0_H)             /* 196 */

/* -- 5-panel 2-row grid (Generic experiment status: Modes A-fallback, B, C) -- */
#define L5_COL0_X0                          42U
#define L5_COL0_X1                          314U
#define L5_COL1_X0                          346U
#define L5_COL1_X1                          536U
#define L5_COL2_X0                          568U
#define L5_COL2_X1                          758U
#define L5_ROW0_Y                           112U
#define L5_ROW0_BOT                         184U
#define L5_BOT_COL0_X0                      42U
#define L5_BOT_COL0_X1                      384U
#define L5_BOT_COL1_X0                      416U
#define L5_BOT_COL1_X1                      758U
#define L5_ROW1_Y                           208U
#define L5_ROW1_BOT                         280U
#define L5_BITSTRIP_X                       189U
#define L5_BITSTRIP_Y                       318U

/* -- Single-column stacked 3-row grid (Mode 7) -- */
#define LS_COL_X0                           42U
#define LS_COL_X1                           324U
#define LS_ROW0_Y                           108U
#define LS_ROW0_BOT                         172U
#define LS_ROW1_Y                           192U
#define LS_ROW1_BOT                         256U
#define LS_ROW2_Y                           276U
#define LS_ROW2_BOT                         340U
#define LS_MATRIX_X                         452U
#define LS_MATRIX_Y                         144U
#define LS_ROW_LABEL_X                      414U
#define LS_COL_LABEL_Y                      134U

/* -- 7-segment digit display (Mode 0) -- */
#define SEG_LABEL_X                         32U
#define SEG_LABEL_Y                         140U
#define SEG_DIGIT_X0                        32U
#define SEG_DIGIT_Y                         158U
#define SEG_DIGIT_W                         72U
#define SEG_DIGIT_H                         132U
#define SEG_DIGIT_GAP                       16U

/* -- LED strip (Mode 0) -- */
#define LED_STRIP_LABEL_X                   32U
#define LED_STRIP_LABEL_Y                   292U
#define LED_STRIP_X0                        38U
#define LED_STRIP_Y                         310U
#define LED_STRIP_CELL_W                    42U
#define LED_STRIP_CELL_H                    34U
#define LED_STRIP_GAP                       18U
#define LED_STRIP_COUNT                     12U

/* -- Value panel internal layout helpers -- */
#define PANEL_LABEL_X_OFFSET                16U
#define PANEL_LABEL_Y_OFFSET                10U
#define PANEL_VALUE_Y_OFFSET                34U
#define PANEL_CORNER_RADIUS                 8U

/* -- Bit-strip drawing helpers -- */
#define BITSTRIP_CELL_W                     34U
#define BITSTRIP_CELL_H                     22U
#define BITSTRIP_GAP                        10U
#define BITSTRIP_LABEL_Y_OFFSET             24U
#define BITSTRIP_CORNER_RADIUS              5U

/* -- Nibble-row drawing helpers -- */
#define NIBBLE_CELL_W                       40U
#define NIBBLE_CELL_H                       46U
#define NIBBLE_GAP                          54U
#define NIBBLE_TEXT_X_OFFSET                11U
#define NIBBLE_TEXT_Y_OFFSET                8U
#define NIBBLE_CORNER_RADIUS                6U

#define LCD_DIAGNOSTIC_MODE                 0U
#define NUM_MODES                           16U   /* Modes 0x0..0xF */

/* -- SPI packet bit-field masks (Byte 0 & Byte 1) -- */
#define SPI_MASK_CLK_SEL                    0x1FU /* Byte 0, bits [4:0] - clock select */
#define SPI_MASK_FREQ_FULL_SELECT           0x20U /* Byte 0, bit  [5]  - frequency full-select flag */
#define SPI_MASK_MODE                       0x0FU /* Byte 1, bits [3:0] - current operating mode */
#define SPI_CRC6_MASK                       0x3FU
#define SPI_CRC6_POLY                       0x03U

static uint8_t spi_tx_buf[SPI1_DMA_BUFFER_SIZE] SPI1_DMA_ALIGNED;   /* Dummy MOSI pattern; XO2 slave only needs valid clocks/CS. */
static uint8_t spi_rx_buf[SPI1_DMA_BUFFER_SIZE] SPI1_DMA_ALIGNED;

static uint8_t current_mode = 0U;
static uint8_t current_clk_sel = 0U;
static uint8_t current_freq_full_select_mode = 0U;
static uint32_t frame_count = 0U;
static uint16_t rx_word0 = 0U;
static uint16_t rx_word1 = 0U;
static uint16_t rx_word2 = 0U;
static uint16_t current_fmcu_pi = 0x8000U;
static uint16_t last_written_pi = 0x8000U;
static uint8_t last_written_pi_valid = 0U;
static uint8_t fmcu_pi_initialized = 0U;
static uint32_t current_xc7a_po = 0x00000000UL;
static uint16_t current_xc7a_pio = 0x0000U;
static uint8_t current_key_led_mask = 0U;
static uint8_t last_written_key_led_mask = 0U;
static uint8_t last_written_key_led_valid = 0U;
static uint8_t key_led_initialized = 0U;
static uint8_t payload_bytes[4] = {0U};
static uint8_t payload_nibbles[8] = {0U};
static HAL_StatusTypeDef last_spi_status = HAL_OK;
static uint32_t last_spi_error_code = HAL_SPI_ERROR_NONE;
static uint32_t spi_error_count = 0U;
static uint32_t spi_timeout_count = 0U;
static uint32_t spi_invalid_frame_count = 0U;
static uint32_t pi_i2c_error_count = 0U;
static uint32_t key_led_i2c_error_count = 0U;
static const char *g_boot_stage = "reset";
/* -- GB2312-encoded Chinese UI label strings (null-terminated pairs) -- */
static const uint8_t CH_EXPERIMENT_VIEW[]   = {0xCA, 0xB5, 0xD1, 0xE9, 0xCA, 0xD3, 0xCD, 0xBC, 0x00, 0x00}; /* ???? */
static const uint8_t CH_RUN_STATUS[]        = {0xD4, 0xCB, 0xD0, 0xD0, 0xD7, 0xB4, 0xCC, 0xAC, 0x00, 0x00}; /* ???? */
static const uint8_t CH_CUR_MODE[]          = {0xB5, 0xB1, 0xC7, 0xB0, 0xC4, 0xA3, 0xCA, 0xBD, 0x00, 0x00}; /* ???? */
static const uint8_t CH_LINK[]              = {0xC1, 0xB4, 0xC2, 0xB7, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_READY[]             = {0xBE, 0xCD, 0xD0, 0xF7, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_HOLD[]              = {0xB5, 0xC8, 0xB4, 0xFD, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_PANEL_TITLE[]       = {0xBF, 0xCE, 0xB3, 0xCC, 0xCA, 0xB5, 0xD1, 0xE9, 0xC3, 0xE6, 0xB0, 0xE5, 0x00, 0x00}; /* ?????? */
static const uint8_t CH_FREQ[]              = {0xC6, 0xB5, 0xC2, 0xCA, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_DIGITS[]            = {0xCA, 0xFD, 0xC2, 0xEB, 0xB9, 0xDC, 0x00, 0x00};               /* ??? */
static const uint8_t CH_LED_GROUP[]         = {0x4C, 0x45, 0x44, 0xD7, 0xE9, 0x00, 0x00};                     /* LED? */
static const uint8_t CH_LINK_DISCONNECT[]   = {0xB6, 0xCF, 0xBF, 0xAA, 0x00, 0x00};                           /* ?? */

/* -- Mode-view label arrays (GB2312 encoded) -- */
static const uint8_t CH_RESULT[]           = {0xBD, 0xE1, 0xB9, 0xFB, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_KEY_CHAIN[]        = {0xB0, 0xB4, 0xBC, 0xFC, 0xC1, 0xB4, 0x00, 0x00};               /* ??? */
static const uint8_t CH_INPUT_BITS[]       = {0xCA, 0xE4, 0xC8, 0xEB, 0xCE, 0xBB, 0x00, 0x00};               /* ??? */
static const uint8_t CH_DECODED_VALUE[]    = {0xD2, 0xEB, 0xC2, 0xEB, 0xD6, 0xB5, 0x00, 0x00};               /* ??? */
static const uint8_t CH_SEG_PATTERN[]      = {0xB6, 0xCE, 0xC4, 0xA3, 0xCA, 0xBD, 0x00, 0x00};               /* ??? */
static const uint8_t CH_LAST_EVENT[]       = {0xD7, 0xEE, 0xBD, 0xFC, 0xCA, 0xC2, 0xBC, 0xFE, 0x00, 0x00};   /* ???? */
static const uint8_t CH_REGISTER[]         = {0xBC, 0xC4, 0xB4, 0xE6, 0xC6, 0xF7, 0x00, 0x00};               /* ??? */
static const uint8_t CH_DIRECTION[]        = {0xB7, 0xBD, 0xCF, 0xF2, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_SHIFT_PATH[]       = {0xD2, 0xC6, 0xCE, 0xBB, 0xC2, 0xB7, 0xBE, 0xB6, 0x00, 0x00};   /* ???? */
static const uint8_t CH_COUNT[]            = {0xBC, 0xC6, 0xCA, 0xFD, 0xD6, 0xB5, 0x00, 0x00};               /* ??? */
static const uint8_t CH_SCAN_LANES[]       = {0xC9, 0xA8, 0xC3, 0xE8, 0xCD, 0xA8, 0xB5, 0xC0, 0x00, 0x00};   /* ???? */
static const uint8_t CH_VALUE[]            = {0xCA, 0xFD, 0xD6, 0xB5, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_SINGLE_PULSE[]     = {0xB5, 0xA5, 0xC2, 0xF6, 0xB3, 0xE5, 0x00, 0x00};               /* ??? */
static const uint8_t CH_LCD_OUTPUT[]       = {0x4C, 0x43, 0x44, 0x20, 0xCA, 0xE4, 0xB3, 0xF6, 0x00, 0x00};   /* LCD ?? */
static const uint8_t CH_GROUP[]            = {0xB7, 0xD6, 0xD7, 0xE9, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_SERIAL[]           = {0xB4, 0xAE, 0xD0, 0xD0, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_GROUP_LINES[]      = {0xD7, 0xE9, 0xCF, 0xDF, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_CODE[]             = {0xBC, 0xFC, 0xC2, 0xEB, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_PARAM[]            = {0xB2, 0xCE, 0xCA, 0xFD, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_DISPLAY[]          = {0xCF, 0xD4, 0xCA, 0xBE, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_SCAN[]             = {0xC9, 0xA8, 0xC3, 0xE8, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_DIRECTION_KEYS[]   = {0xB7, 0xBD, 0xCF, 0xF2, 0xBC, 0xFC, 0x00, 0x00};               /* ??? */
static const uint8_t CH_TIME[]             = {0xCA, 0xB1, 0xBC, 0xE4, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_CONTROL[]          = {0xBF, 0xD8, 0xD6, 0xC6, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_CONTROL_KEYS[]     = {0xBF, 0xD8, 0xD6, 0xC6, 0xBC, 0xFC, 0x00, 0x00};               /* ??? */
static const uint8_t CH_OUTPUT[]           = {0xCA, 0xE4, 0xB3, 0xF6, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_STATUS[]           = {0xD7, 0xB4, 0xCC, 0xAC, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_LEFT[]             = {0xD7, 0xF3, 0xD2, 0xC6, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_RIGHT[]            = {0xD3, 0xD2, 0xD2, 0xC6, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_ACTIVE[]           = {0xBB, 0xEE, 0xB6, 0xAF, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_IDLE[]             = {0xBF, 0xD5, 0xCF, 0xD0, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_RUN[]              = {0xD4, 0xCB, 0xD0, 0xD0, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_SET[]              = {0xC9, 0xE8, 0xD6, 0xC3, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_MARK[]             = {0xB1, 0xEA, 0xBC, 0xC7, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_SPACE[]            = {0xBF, 0xD5, 0xBA, 0xC5, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_ALT[]              = {0xB1, 0xB8, 0xD3, 0xC3, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_BASE[]             = {0xBB, 0xF9, 0xB4, 0xA1, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_UP[]               = {0xC9, 0xCF, 0x00, 0x00};                                       /* ? */
static const uint8_t CH_DOWN[]             = {0xCF, 0xC2, 0x00, 0x00};                                       /* ? */
static const uint8_t CH_CENTER[]           = {0xD6, 0xD0, 0x00, 0x00};                                       /* ? */
static const uint8_t CH_ARROW_LEFT[]       = {0xD7, 0xF3, 0xCF, 0xF2, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_ARROW_RIGHT[]      = {0xD3, 0xD2, 0xCF, 0xF2, 0x00, 0x00};                           /* ?? */
static const uint8_t CH_PIO_STATUS[]       = {0x50, 0x49, 0x4F, 0xD7, 0xB4, 0xCC, 0xAC, 0x00, 0x00};         /* PIO?? */
static const uint8_t CH_PIO_BITMAP[]       = {0x50, 0x49, 0x4F, 0xCE, 0xBB, 0xCD, 0xBC, 0x00, 0x00};         /* PIO + wei2 tu2 */
static const uint8_t CH_KEY_BITMAP[]       = {0xBC, 0xFC, 0xCE, 0xBB, 0xCD, 0xBC, 0x00, 0x00};               /* jian4 wei2 tu2 */

static const char MODE0_RESOURCE_SUBTITLE[] = "8SEG + 12LED + LCD";
static const char MODE1_RESOURCE_SUBTITLE[] = "16BIT PI -> 4/8SEG";
static const char MODE2_RESOURCE_SUBTITLE[] = "AUTO/STUDENT DECODER";
static const char MODE3_RESOURCE_SUBTITLE[] = "8KEY EVENT + LATCH";
static const char MODE4_RESOURCE_SUBTITLE[] = "16SW BUS + LED";
static const char MODE5_RESOURCE_SUBTITLE[] = "RAW DIGIT + SEG CTRL";
static const char MODE6_RESOURCE_SUBTITLE[] = "8KEY SHAPE + LCD";
static const char MODE7_RESOURCE_SUBTITLE[] = "4x4 KEY MATRIX";
static const char MODE8_RESOURCE_SUBTITLE[] = "EC11 + PARAM VIEW";
static const char MODE9_RESOURCE_SUBTITLE[] = "5WAY NAV + LCD";
static const char MODEA_RESOURCE_SUBTITLE[] = "MOTOR HEALTH CHECK";
static const char MODEB_RESOURCE_SUBTITLE[] = "PO/PIO/KEY OBSERVE";
static const char MODEC_RESOURCE_SUBTITLE[] = "ETH/USB REMOTE";

typedef void (*ModeViewRenderer)(void);

typedef struct
{
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
} UiRect;

typedef enum
{
    UI_LABEL_ASCII = 0,
    UI_LABEL_CN
} UiLabelKind;

typedef struct
{
    UiLabelKind kind;
    const char *ascii;
    const uint8_t *cn;
} UiLabel;

typedef struct
{
    UiRect rect;
    UiLabel label;
    uint32_t value_color;
} UiValuePanelSpec;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t count;
    const uint8_t *label;
} UiBitStripSpec;

typedef struct
{
    uint16_t x;
    uint16_t y;
    const char *label;
} UiButtonSpec;

typedef struct
{
    uint8_t mode;
    const char *resource_subtitle;
    uint8_t compact_aux_mode_text;
    ModeViewRenderer draw_view;
    uint8_t has_lcd_output;
    uint8_t shows_key_led_strip;
    uint8_t uses_key_chain_display;
} ModeUiConfig;

typedef struct
{
    uint8_t mode;
    uint8_t exp_id;
    const char *name;
} ExperimentPageDescriptor;

typedef struct
{
    uint8_t previous_mode;
    uint8_t previous_visible_mask;
    uint8_t latched_mask;
    uint8_t pulse_ticks[KEY_LED_COUNT];
} KeyLedState;

typedef struct
{
    uint8_t visible_mask;
    uint8_t direct_mask;
    uint8_t latched_mask;
    uint8_t pulse_source;
    uint8_t pulse_mask;
} KeyLedSources;

typedef struct
{
    uint32_t digit_word;
    uint16_t led_status;
} LcdMode0Status;

typedef struct
{
    uint8_t key_value;
    uint8_t key_valid;
    uint8_t key_event;
    uint8_t multi_key;
    uint8_t key_pressed;
    uint8_t row_idx;
    uint8_t col_idx;
    uint16_t business_pio;
    uint32_t business_po;
} LcdMode7Status;

typedef struct
{
    uint16_t div_value;
    uint8_t tick_count;
    uint8_t step_select;
    uint8_t clk_div2;
    uint8_t cw_pulse;
    uint8_t ccw_pulse;
    uint8_t display_nibbles[8];
    uint16_t led_status;
} LcdMode8Status;

typedef struct
{
    uint8_t cursor_onehot;
    uint8_t cursor_index;
    uint8_t event_code;
    uint8_t boundary_hit;
    uint8_t key_mask;
} LcdMode9Status;

typedef struct
{
    uint8_t exp_id;
    uint8_t enable;
    uint8_t direction;
    uint8_t stop_request;
    uint8_t drive_active;
    uint8_t aux_code;
    uint16_t metric_code;
    uint8_t key_mask;
    uint16_t pio_status;
} LcdModeAStatus;

static void Data_Process(void);
static uint8_t SPI_FrameIsValid(const uint8_t *frame);
static uint8_t SPI_CRC6Compute(const uint8_t *frame);
static uint8_t TickDue(uint32_t now, uint32_t deadline);
static uint16_t BuildFmcuPiValue(void);
static uint8_t FMCU_PI_EnsureInitialized(void);
static void UpdateKeyLedState(void);
static void KeyLedState_Reset(KeyLedState *state);
static void KeyLedState_SelectSources(KeyLedState *state, KeyLedSources *sources);
static void KeyLedState_ApplyPulse(KeyLedState *state, KeyLedSources *sources);
static void KeyLedState_Commit(const KeyLedSources *sources);
static const ModeUiConfig *get_mode_ui_config(uint8_t mode);
static uint8_t mode_has_lcd_output(uint8_t mode);
static uint8_t mode_shows_key_led_strip(uint8_t mode);
static uint8_t mode_uses_key_chain_display(uint8_t mode);
static const char *get_mode_resource_subtitle(uint8_t mode);
static const ExperimentPageDescriptor *LCD_FindExperimentPage(uint8_t mode, uint8_t exp_id);
static void LCD_Display_Update(void);
static void LCD_DrawStaticLayout(void);
static void LCD_DrawCard(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                         const uint8_t *title, uint32_t fill_color, uint32_t border_color);
static void LCD_ShowFormatted(uint16_t x, uint16_t y, uint8_t size,
                              uint32_t color, uint32_t back_color, const char *fmt, ...);
static void LCD_ClearCardContent(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t fill_color);
static void LCD_ClearDynamicRegions(void);
static void LCD_DrawValuePanel(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                               const char *label, const char *value, uint32_t value_color);
static void LCD_DrawValuePanelCN(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                 const uint8_t *label, const char *value, uint32_t value_color);
static void LCD_DrawValuePanelSpec(const UiValuePanelSpec *spec, const char *value);
static void LCD_DrawBitStripSpec(const UiBitStripSpec *spec, uint16_t value);
static void LCD_DrawButtonSpec(const UiButtonSpec *spec, uint8_t pressed);
static void LCD_DrawValuePanelRow(const UiValuePanelSpec *specs, const char * const *values, uint8_t count);
static void LCD_DrawNibbleRow(uint16_t x, uint16_t y, uint8_t first, uint8_t count);
static void LCD_DrawBitStrip(uint16_t x, uint16_t y, uint8_t count, uint16_t value, const uint8_t *label);
static void LCD_DrawPianoKeyStrip(uint16_t x, uint16_t y, uint8_t key_mask);
static void LCD_DrawKeyLedStrip(uint8_t led_mask);
static void LCD_DrawMatrix4x4(uint16_t x, uint16_t y, uint16_t active_mask);
static void LCD_DrawSegmentH(uint16_t x, uint16_t y, uint16_t w, uint16_t t, uint32_t color);
static void LCD_DrawSegmentV(uint16_t x, uint16_t y, uint16_t h, uint16_t t, uint32_t color);
static void LCD_DrawSevenSegDigit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t value);
static LcdMode0Status LCD_ParseMode0Status(uint32_t po_status, uint16_t pio_status);
static uint8_t LCD_MapMode7LogicalKey(uint8_t row_idx, uint8_t col_idx);
static LcdMode7Status LCD_ParseMode7Status(uint32_t po_status, uint16_t pio_status);
static LcdMode8Status LCD_ParseMode8Status(uint32_t po_status, uint16_t pio_status);
static LcdMode9Status LCD_ParseMode9Status(uint32_t po_status, uint16_t pio_status);
static LcdModeAStatus LCD_ParseModeAStatus(uint32_t po_status, uint16_t pio_status);
static void LCD_DrawMode0View(void);
static void LCD_DrawMode0SevenSegView(uint32_t po_value);
static void LCD_DrawMode0LedView(uint16_t pio_value);
static void LCD_DrawMode1View(void);
static void LCD_DrawMode2View(void);
static void LCD_DrawMode3View(void);
static void LCD_DrawMode4View(void);
static void LCD_DrawMode5View(void);
static void LCD_DrawMode6View(void);
static void LCD_DrawMode7View(void);
static void LCD_DrawMode8View(void);
static uint8_t LCD_GetMode9CursorPos(uint8_t cursor_onehot, uint8_t fallback_index);
static void LCD_DrawMode9Button(uint16_t x, uint16_t y, const char *label, uint8_t pressed);
static void LCD_DrawMode9View(void);
static void LCD_DrawModeAView(void);
static void LCD_DrawGenericExperimentStatusView(const ExperimentPageDescriptor *descriptor);
static void LCD_DrawModeBView(void);
static void LCD_DrawModeCView(void);
static void LCD_UpdateStatusCard(uint32_t status_bg, uint32_t status_text);
static void LCD_UpdateDataCard(void);
static void LCD_UpdateAuxCard(void);
static void SDRAM_BasicTest(void);
static void Framebuffer_Init(void);
static void LCD_FillFramebufferColor(uint16_t rgb565);
static void LCD_DiagnosticTest(void);

/* -----------------------------------------------------------------------
 * MODE REGISTRATION CHECKLIST (for adding a new experiment mode 0xD):
 *
 * [ ] 1. Add subtitle string:   static const char MODED_RESOURCE_SUBTITLE[] = "...";
 * [ ] 2. Write view renderer:   static void LCD_DrawModeDView(void);
 * [ ] 3. Add entry to:          mode_ui_config_table[] below
 * [ ] 4. Add entry to:          experiment_page_table[] (if it has sub-experiments)
 * [ ] 5. Add name entry to:     g_mode_names[] and g_mode_short_names[] below
 * [ ] 6. Update (if PI mapping): BuildFmcuPiValue()  switch case
 * [ ] 7. Update (if LED ctrl):   KeyLedState_SelectSources() switch case
 * [ ] 8. Update (if new parser): Add LcdModeDStatus struct + LCD_ParseModeDStatus()
 * ----------------------------------------------------------------------- */
static const ModeUiConfig mode_ui_config_table[] = {
    {0x0U, MODE0_RESOURCE_SUBTITLE, 0U, LCD_DrawMode0View, 1U, 1U, 0U},
    {0x1U, MODE1_RESOURCE_SUBTITLE, 0U, LCD_DrawMode1View, 0U, 1U, 1U},
    {0x2U, MODE2_RESOURCE_SUBTITLE, 0U, LCD_DrawMode2View, 0U, 1U, 1U},
    {0x3U, MODE3_RESOURCE_SUBTITLE, 0U, LCD_DrawMode3View, 0U, 1U, 1U},
    {0x4U, MODE4_RESOURCE_SUBTITLE, 0U, LCD_DrawMode4View, 1U, 0U, 0U},
    {0x5U, MODE5_RESOURCE_SUBTITLE, 0U, LCD_DrawMode5View, 1U, 0U, 0U},
    {0x6U, MODE6_RESOURCE_SUBTITLE, 0U, LCD_DrawMode6View, 1U, 1U, 0U},
    {0x7U, MODE7_RESOURCE_SUBTITLE, 1U, LCD_DrawMode7View, 1U, 0U, 0U},
    {0x8U, MODE8_RESOURCE_SUBTITLE, 1U, LCD_DrawMode8View, 1U, 0U, 0U},
    {0x9U, MODE9_RESOURCE_SUBTITLE, 1U, LCD_DrawMode9View, 1U, 0U, 0U},
    {0xAU, MODEA_RESOURCE_SUBTITLE, 1U, LCD_DrawModeAView, 1U, 1U, 0U},
    {0xBU, MODEB_RESOURCE_SUBTITLE, 1U, LCD_DrawModeBView, 1U, 1U, 0U},
    {0xCU, MODEC_RESOURCE_SUBTITLE, 1U, LCD_DrawModeCView, 1U, 1U, 0U},
};

static const ExperimentPageDescriptor experiment_page_table[] = {
    {0xAU, 0xA1U, "CTRL GENERIC"},
    {0xAU, 0xD2U, "DC MOTOR"},
    {0xAU, 0xD3U, "STEP MOTOR"},
    {0xAU, 0xDEU, "EXT TRAFFIC"},
    {0xBU, 0xD4U, "MATRIX LED"},
    {0xBU, 0xB2U, "VGA"},
    {0xBU, 0xB3U, "HDMI"},
    {0xBU, 0xB4U, "RGB LCD"},
    {0xBU, 0xB5U, "OV5640"},
    {0xBU, 0xDFU, "ETH UDP"},
    {0xCU, 0xC1U, "ONEWIRE"},
    {0xCU, 0xC2U, "PS2"},
    {0xCU, 0xC3U, "I2C EEPROM"},
    {0xCU, 0xC4U, "SD SPI"},
    {0xCU, 0xC5U, "ETHERNET"},
};

#define BOOT_STAGE(_stage) do { g_boot_stage = (_stage); } while (0)

int main(void)
{
    uint32_t next_spi_tick;
    uint32_t next_ui_tick;
    uint32_t now;
    SPI1_TransferResult transfer_result;

    BOOT_STAGE("MPU_Config");
    MPU_Config();
    BOOT_STAGE("CPU_CACHE_Enable");
    CPU_CACHE_Enable();
    HAL_Init();
    BOOT_STAGE("SystemClock_Config");
    SystemClock_Config();

    BOOT_STAGE("MX_GPIO_Init");
    if (MX_GPIO_Init() != HAL_OK)
    {
        Error_Handler();
    }

    BOOT_STAGE("KEY_LED_Init");
    if (KEY_LED_Init() != HAL_OK)
    {
        key_led_i2c_error_count++;
    }
    else
    {
        key_led_initialized = 1U;
        last_written_key_led_mask = 0U;
        last_written_key_led_valid = 1U;
    }

    BOOT_STAGE("FMCU_PI_Init");
    if (FMCU_PI_Init() != HAL_OK)
    {
        pi_i2c_error_count++;
    }
    else
    {
        fmcu_pi_initialized = 1U;
        current_fmcu_pi = 0x8000U;
        last_written_pi = 0x8000U;
        last_written_pi_valid = 1U;
    }
    BOOT_STAGE("SPI1_Master_Init");
    SPI1_Master_Init();

    BOOT_STAGE("bsp_InitExtSDRAM");
    bsp_InitExtSDRAM();
    BOOT_STAGE("SDRAM_BasicTest");
    SDRAM_BasicTest();
    BOOT_STAGE("Framebuffer_Init");
    Framebuffer_Init();

    BOOT_STAGE("LCD_RGB_Init");
    LCD_RGB_Init();

    BOOT_STAGE("LTDC_CFBAR_Check");
    if (LTDC_Layer1->CFBAR != LCD_RGB_FB_ADDR)
    {
        LTDC_Layer1->CFBAR = LCD_RGB_FB_ADDR;
        LTDC->SRCR = LTDC_SRCR_IMR;
    }

    BOOT_STAGE("LCD_RGB_BacklightOn");
    LCD_RGB_BacklightOn();

#if (LCD_DIAGNOSTIC_MODE == 1U)
    BOOT_STAGE("LCD_DiagnosticTest");
    LCD_DiagnosticTest();
#endif

    LCD_SetBackColor(LCD_COLOR_BLACK);
    LCD_SetTextColor(LCD_COLOR_WHITE);
    LCD_DrawStaticLayout();

    /* Dummy TX pattern: FPGA slave only uses MOSI for CS-edge timing. */
    spi_tx_buf[0] = 0xA5U;
    spi_tx_buf[1] = 0x5AU;
    spi_tx_buf[2] = 0x00U;
    spi_tx_buf[3] = 0x00U;
    spi_tx_buf[4] = 0x00U;
    spi_tx_buf[5] = 0x00U;

    LCD_Display_Update();

    now = HAL_GetTick();
    next_spi_tick = now;
    next_ui_tick = now + UI_REFRESH_PERIOD_MS;

    while (1)
    {
        transfer_result = SPI1_TransferPoll();
        if (transfer_result == SPI1_TRANSFER_COMPLETE)
        {
            if (SPI_FrameIsValid(spi_rx_buf) != 0U)
            {
                last_spi_status = HAL_OK;
                last_spi_error_code = HAL_SPI_ERROR_NONE;
                Data_Process();
                frame_count++;
            }
            else
            {
                last_spi_status = HAL_ERROR;
                spi_invalid_frame_count++;
            }
        }
        else if (transfer_result == SPI1_TRANSFER_ERROR)
        {
            last_spi_status = HAL_ERROR;
            last_spi_error_code = SPI1_GetLastError();
            spi_error_count++;
        }
        else if (transfer_result == SPI1_TRANSFER_TIMEOUT)
        {
            last_spi_status = HAL_TIMEOUT;
            last_spi_error_code = SPI1_GetLastError();
            spi_timeout_count++;
        }

        now = HAL_GetTick();
        if ((TickDue(now, next_spi_tick) != 0U) &&
            (SPI1_TransferIsBusy() == 0U))
        {
            if (SPI1_TransferStart(spi_tx_buf, spi_rx_buf, SPI_FRAME_SIZE) != HAL_OK)
            {
                last_spi_status = HAL_ERROR;
                last_spi_error_code = SPI1_GetLastError();
                spi_error_count++;
            }

            next_spi_tick += SPI_SAMPLE_PERIOD_MS;
            if (TickDue(now, next_spi_tick) != 0U)
            {
                next_spi_tick = now + SPI_SAMPLE_PERIOD_MS;
            }
        }

        /* Never enter a potentially long LCD/DMA2D update while the 75 us
         * SPI transaction deadline is active. */
        if ((TickDue(now, next_ui_tick) != 0U) &&
            (SPI1_TransferIsBusy() == 0U))
        {
            UpdateKeyLedState();
            LCD_Display_Update();
            next_ui_tick += UI_REFRESH_PERIOD_MS;
            if (TickDue(now, next_ui_tick) != 0U)
            {
                next_ui_tick = now + UI_REFRESH_PERIOD_MS;
            }
        }
    }
}

static uint8_t TickDue(uint32_t now, uint32_t deadline)
{
    return ((int32_t)(now - deadline) >= 0) ? 1U : 0U;
}

static uint8_t SPI_FrameIsValid(const uint8_t *frame)
{
    uint8_t received_crc;

    if (frame == NULL)
    {
        return 0U;
    }

    received_crc = (uint8_t)((uint8_t)(frame[0] >> 6U) << 4U);
    received_crc |= (uint8_t)(frame[1] >> 4U);

    return (received_crc == SPI_CRC6Compute(frame)) ? 1U : 0U;
}

static uint8_t SPI_CRC6Compute(const uint8_t *frame)
{
    uint8_t crc = SPI_CRC6_MASK;
    uint8_t byte_index;
    uint8_t bit_index;
    uint8_t bit_value;
    uint8_t feedback;

    /* CRC payload order matches spi_packet_builder.v:
     *   freq_full_select_mode, clk_sel[4:0], mode[3:0],
     *   legacy_payload[31:0], PO[31:0], PIO[15:0].
     * Header CRC bits frame[0][7:6] and frame[1][7:4] are excluded. */
    for (bit_index = 0U; bit_index < 6U; bit_index++)
    {
        bit_value = (uint8_t)((frame[0] >> (5U - bit_index)) & 0x01U);
        feedback = (uint8_t)(bit_value ^ ((crc >> 5U) & 0x01U));
        crc = (uint8_t)((crc << 1U) & SPI_CRC6_MASK);
        if (feedback != 0U)
        {
            crc ^= SPI_CRC6_POLY;
        }
    }

    for (bit_index = 0U; bit_index < 4U; bit_index++)
    {
        bit_value = (uint8_t)((frame[1] >> (3U - bit_index)) & 0x01U);
        feedback = (uint8_t)(bit_value ^ ((crc >> 5U) & 0x01U));
        crc = (uint8_t)((crc << 1U) & SPI_CRC6_MASK);
        if (feedback != 0U)
        {
            crc ^= SPI_CRC6_POLY;
        }
    }

    for (byte_index = 2U; byte_index < SPI_FRAME_SIZE; byte_index++)
    {
        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            bit_value = (uint8_t)((frame[byte_index] >> (7U - bit_index)) & 0x01U);
            feedback = (uint8_t)(bit_value ^ ((crc >> 5U) & 0x01U));
            crc = (uint8_t)((crc << 1U) & SPI_CRC6_MASK);
            if (feedback != 0U)
            {
                crc ^= SPI_CRC6_POLY;
            }
        }
    }

    return crc;
}

static void Data_Process(void)
{
    uint16_t new_fmcu_pi;

    /* FPGA spi_packet_builder.v packs 96 bits MSB-first as
     *   {{crc6[5:4], freq_full_select_mode, clk_sel[4:0]},
     *    crc6[3:0], mode[3:0], payload[31:0], PO[31:0], PIO[15:0]}
     * After MSB-first 12-byte transfer:
     *   rx[0] = freq/select       rx[1] = {0000, mode}
     *   rx[2..5] = 4 legacy-aligned control bytes
     *   rx[6..9] = XC7A PO[31:0]  rx[10..11] = XC7A PIO[15:0]
     */
    current_clk_sel = spi_rx_buf[0] & SPI_MASK_CLK_SEL;
    current_freq_full_select_mode = ((spi_rx_buf[0] & SPI_MASK_FREQ_FULL_SELECT) != 0U) ? 1U : 0U;
    current_mode    = spi_rx_buf[1] & SPI_MASK_MODE;

    payload_bytes[0] = spi_rx_buf[2];
    payload_bytes[1] = spi_rx_buf[3];
    payload_bytes[2] = spi_rx_buf[4];
    payload_bytes[3] = spi_rx_buf[5];

    payload_nibbles[0] = (uint8_t)(spi_rx_buf[2] >> 4);
    payload_nibbles[1] = (uint8_t)(spi_rx_buf[2] & 0x0FU);
    payload_nibbles[2] = (uint8_t)(spi_rx_buf[3] >> 4);
    payload_nibbles[3] = (uint8_t)(spi_rx_buf[3] & 0x0FU);
    payload_nibbles[4] = (uint8_t)(spi_rx_buf[4] >> 4);
    payload_nibbles[5] = (uint8_t)(spi_rx_buf[4] & 0x0FU);
    payload_nibbles[6] = (uint8_t)(spi_rx_buf[5] >> 4);
    payload_nibbles[7] = (uint8_t)(spi_rx_buf[5] & 0x0FU);

    rx_word0 = ((uint16_t)spi_rx_buf[0] << 8) | spi_rx_buf[1];
    rx_word1 = ((uint16_t)spi_rx_buf[2] << 8) | spi_rx_buf[3];
    rx_word2 = ((uint16_t)spi_rx_buf[4] << 8) | spi_rx_buf[5];
    current_xc7a_po = ((uint32_t)spi_rx_buf[6] << 24) |
                      ((uint32_t)spi_rx_buf[7] << 16) |
                      ((uint32_t)spi_rx_buf[8] << 8)  |
                      ((uint32_t)spi_rx_buf[9]);
    current_xc7a_pio = ((uint16_t)spi_rx_buf[10] << 8) |
                       ((uint16_t)spi_rx_buf[11]);

    new_fmcu_pi = BuildFmcuPiValue();
    if (FMCU_PI_EnsureInitialized() == 0U)
    {
        return;
    }
    if ((last_written_pi_valid == 0U) || (new_fmcu_pi != last_written_pi))
    {
        if (FMCU_PI_Write(new_fmcu_pi) == HAL_OK)
        {
            last_written_pi = new_fmcu_pi;
            last_written_pi_valid = 1U;
            current_fmcu_pi = new_fmcu_pi;
        }
        else
        {
            pi_i2c_error_count++;
            /* Do not advance the last-known-good shadow.  A NACK may mean the
             * expander reset, so the next frame must restore IOCON/IODIR and
             * the safe PI preload before retrying the requested value. */
            fmcu_pi_initialized = 0U;
        }
    }
}

static uint8_t FMCU_PI_EnsureInitialized(void)
{
    if (fmcu_pi_initialized != 0U)
    {
        return 1U;
    }

    if (FMCU_PI_Init() != HAL_OK)
    {
        pi_i2c_error_count++;
        return 0U;
    }

    fmcu_pi_initialized = 1U;
    current_fmcu_pi = 0x8000U;
    last_written_pi = 0x8000U;
    last_written_pi_valid = 1U;
    return 1U;
}

/* Build the 16-bit FMCU PI register value from 4 SPI payload bytes.
 * Mapping is mode-dependent: Mode 0 uses 7 bits across all 4 bytes,
 * Modes 6/A/B/C pass byte-0 directly, Mode 1 swaps bytes, etc. */
static uint16_t BuildFmcuPiValue(void)
{
    const uint8_t control_byte_one   = payload_bytes[0];
    const uint8_t control_byte_two   = payload_bytes[1];
    const uint8_t control_byte_three = payload_bytes[2];
    const uint8_t control_byte_four  = payload_bytes[3];
    uint16_t value = 0x0000U;

    switch (current_mode)
    {
        case 0x0U:   /* 7-bit across all 4 control bytes */
            value |= (uint16_t)control_byte_one;
            value |= (uint16_t)((control_byte_two & 0x01U) << 8);
            value |= (uint16_t)(((control_byte_two >> 4) & 0x01U) << 9);
            value |= (uint16_t)((control_byte_three & 0x01U) << 10);
            value |= (uint16_t)(((control_byte_three >> 4) & 0x01U) << 11);
            value |= (uint16_t)((control_byte_four & 0x01U) << 12);
            value |= (uint16_t)(((control_byte_four >> 4) & 0x01U) << 13);
            break;

        case 0x1U:   /* byte swap: ctl2 | (ctl1 << 8) */
            value |= (uint16_t)control_byte_two;
            value |= (uint16_t)((uint16_t)control_byte_one << 8);
            break;

        case 0x2U:   /* 8 bits from nibble-pairs across 4 bytes */
        case 0x3U:
            value |= (uint16_t)((control_byte_one & 0x01U) << 0);
            value |= (uint16_t)(((control_byte_one >> 4) & 0x01U) << 1);
            value |= (uint16_t)((control_byte_two & 0x01U) << 2);
            value |= (uint16_t)(((control_byte_two >> 4) & 0x01U) << 3);
            value |= (uint16_t)((control_byte_three & 0x01U) << 4);
            value |= (uint16_t)(((control_byte_three >> 4) & 0x01U) << 5);
            value |= (uint16_t)((control_byte_four & 0x01U) << 6);
            value |= (uint16_t)(((control_byte_four >> 4) & 0x01U) << 7);
            break;

        case 0x6U:   /* pass-through control_byte_one */
        case 0xAU:
        case 0xBU:
        case 0xCU:
            value = (uint16_t)control_byte_one;
            break;

        default:
            break;
    }

    return value;
}

/* Key LED state machine: per 30 Hz frame, detect rising edges on key events
 * and drive a 2-frame LED pulse. Mode changes reset the state machine. */
static void UpdateKeyLedState(void)
{
    static KeyLedState state = {0xFFU, 0U, 0U, {0U}};
    KeyLedSources sources;

    if (last_spi_status != HAL_OK)
    {
        KeyLedState_Reset(&state);
        KeyLedState_Commit(NULL);
        return;
    }

    if (state.previous_mode != current_mode)
    {
        KeyLedState_Reset(&state);
        state.previous_mode = current_mode;
    }

    if (mode_shows_key_led_strip(current_mode) == 0U)
    {
        KeyLedState_Reset(&state);
        KeyLedState_Commit(NULL);
        return;
    }

    KeyLedState_SelectSources(&state, &sources);
    KeyLedState_ApplyPulse(&state, &sources);
    KeyLedState_Commit(&sources);
}

static void KeyLedState_Reset(KeyLedState *state)
{
    uint8_t i;

    state->previous_mode = 0xFFU;
    state->previous_visible_mask = 0U;
    state->latched_mask = 0U;
    for (i = 0U; i < KEY_LED_COUNT; i++)
    {
        state->pulse_ticks[i] = 0U;
    }
}

/* Select LED drive sources per operating mode: direct (immediate), latched
 * (sticky until mode change), or pulse (2-frame blip on rising edge). */
static void KeyLedState_SelectSources(KeyLedState *state, KeyLedSources *sources)
{
    uint8_t visible_mask = (uint8_t)(current_fmcu_pi & 0x00FFU);
    uint8_t direct_mask = 0U;
    uint8_t latched_mask = state->latched_mask;
    uint8_t pulse_source = 0U;

    switch (current_mode)
    {
        case 0x0U:   /* 7-bit expanded from FMCU_PI upper bits */
            visible_mask = (uint8_t)(
                0x03U
                | (((current_fmcu_pi >> 8)  & 0x0001U) << 2)
                | (((current_fmcu_pi >> 9)  & 0x0001U) << 3)
                | (((current_fmcu_pi >> 10) & 0x0001U) << 4)
                | (((current_fmcu_pi >> 11) & 0x0001U) << 5)
                | (((current_fmcu_pi >> 12) & 0x0001U) << 6)
                | (((current_fmcu_pi >> 13) & 0x0001U) << 7)
            );
            direct_mask = visible_mask;
            break;

        case 0x1U:   /* latched lower nibble + direct upper nibble from PIO */
            latched_mask = 0x0FU;
            direct_mask = (uint8_t)((current_xc7a_pio & 0x000FU) << 4);
            break;

        case 0x2U:   /* direct from FMCU_PI */
        case 0x3U:
            direct_mask = visible_mask;
            break;

        case 0x6U:   /* direct from PIO low byte */
            direct_mask = (uint8_t)(current_xc7a_pio & 0x00FFU);
            break;

        case 0xAU:   /* direct from FMCU_PI low byte */
        case 0xBU:
        case 0xCU:
            direct_mask = (uint8_t)(current_fmcu_pi & 0x00FFU);
            break;

        default:
            direct_mask = 0U;
            break;
    }

    state->latched_mask = latched_mask;
    sources->visible_mask = visible_mask;
    sources->direct_mask = direct_mask;
    sources->latched_mask = latched_mask;
    sources->pulse_source = pulse_source;
    sources->pulse_mask = 0U;
}

static void KeyLedState_ApplyPulse(KeyLedState *state, KeyLedSources *sources)
{
    uint8_t rising_mask = (uint8_t)(sources->pulse_source & (uint8_t)(~state->previous_visible_mask));
    uint8_t i;

    state->previous_visible_mask = sources->visible_mask;

    for (i = 0U; i < KEY_LED_COUNT; i++)
    {
        if ((rising_mask & (uint8_t)(1U << i)) != 0U)
        {
            state->pulse_ticks[i] = 2U;
        }

        if (state->pulse_ticks[i] != 0U)
        {
            sources->pulse_mask = (uint8_t)(sources->pulse_mask | (uint8_t)(1U << i));
            state->pulse_ticks[i]--;
        }
    }
}

static void KeyLedState_Commit(const KeyLedSources *sources)
{
    uint8_t new_mask;

    if (sources == NULL)
    {
        new_mask = 0U;
    }
    else
    {
        new_mask = (uint8_t)(sources->direct_mask |
                             sources->latched_mask |
                             sources->pulse_mask);
    }

    current_key_led_mask = new_mask;
    if (key_led_initialized == 0U)
    {
        if (KEY_LED_Init() != HAL_OK)
        {
            key_led_i2c_error_count++;
            return;
        }

        key_led_initialized = 1U;
        last_written_key_led_mask = 0U;
        last_written_key_led_valid = 1U;
    }

    if ((last_written_key_led_valid == 0U) ||
        (new_mask != last_written_key_led_mask))
    {
        if (KEY_LED_WriteMask(new_mask) == HAL_OK)
        {
            last_written_key_led_mask = new_mask;
            last_written_key_led_valid = 1U;
        }
        else
        {
            key_led_i2c_error_count++;
            /* If the expander reset while the bus was faulty, a GPIO-only
             * retry could ACK while its pins are still inputs. */
            key_led_initialized = 0U;
        }
    }
}

static const ModeUiConfig *get_mode_ui_config(uint8_t mode)
{
    uint8_t i;

    for (i = 0U; i < (sizeof(mode_ui_config_table) / sizeof(mode_ui_config_table[0])); i++)
    {
        if (mode_ui_config_table[i].mode == mode)
        {
            return &mode_ui_config_table[i];
        }
    }

    return &mode_ui_config_table[0xBU];
}

static uint8_t mode_has_lcd_output(uint8_t mode)
{
    return get_mode_ui_config(mode)->has_lcd_output;
}

static uint8_t mode_shows_key_led_strip(uint8_t mode)
{
    return get_mode_ui_config(mode)->shows_key_led_strip;
}

static uint8_t mode_uses_key_chain_display(uint8_t mode)
{
    return get_mode_ui_config(mode)->uses_key_chain_display;
}

static const char *get_mode_resource_subtitle(uint8_t mode)
{
    return get_mode_ui_config(mode)->resource_subtitle;
}

static const ExperimentPageDescriptor *LCD_FindExperimentPage(uint8_t mode, uint8_t exp_id)
{
    uint8_t i;

    for (i = 0U; i < (sizeof(experiment_page_table) / sizeof(experiment_page_table[0])); i++)
    {
        if ((experiment_page_table[i].mode == mode) &&
            (experiment_page_table[i].exp_id == exp_id))
        {
            return &experiment_page_table[i];
        }
    }

    return NULL;
}

static void LCD_DrawStaticLayout(void)
{
    LCD_Clear(UI_BG_COLOR);
    LCD_Fill(0U, 0U, LCD_WIDTH - 1U, UI_HEADER_BOTTOM_Y, UI_HEADER_BG);
    LCD_DrawLine(0U, UI_HEADER_BOTTOM_Y, LCD_WIDTH - 1U, UI_HEADER_BOTTOM_Y, UI_BORDER_COLOR);
    LCD_ShowChinese(20U, 12U, CH_PANEL_TITLE, 32U, LCD_COLOR_WHITE, UI_HEADER_BG);
    LCD_DrawCard(DATA_X1, DATA_Y1, DATA_X2, DATA_Y2, CH_EXPERIMENT_VIEW,
                 UI_CARD_BG, UI_BORDER_COLOR);
    LCD_DrawCard(CLOCK_X1, CLOCK_Y1, CLOCK_X2, CLOCK_Y2, CH_RUN_STATUS,
                 UI_CARD_CLOCK_BG, UI_BORDER_COLOR);
    LCD_Refresh();
}

static void LCD_DrawCard(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                         const uint8_t *title, uint32_t fill_color, uint32_t border_color)
{
    LCD_FillRounded(x1, y1, x2, y2, 10U, fill_color);
    LCD_FillRounded((uint16_t)(x1 + 1U), (uint16_t)(y1 + 1U),
                    (uint16_t)(x2 - 1U), (uint16_t)(y1 + UI_CARD_TITLE_H),
                    9U, border_color);
    LCD_DrawRoundedRect(x1, y1, x2, y2, 10U, border_color);
    LCD_ShowChinese((uint16_t)(x1 + 12U), (uint16_t)(y1 + 5U), title, 16U,
                    LCD_COLOR_WHITE, border_color);
}

static void LCD_ShowFormatted(uint16_t x, uint16_t y, uint8_t size,
                              uint32_t color, uint32_t back_color, const char *fmt, ...)
{
    char line_buf[96];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line_buf, sizeof(line_buf), fmt, args);
    va_end(args);

    LCD_ShowString(x, y, line_buf, size, color, back_color);
}

static void LCD_ClearCardContent(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t fill_color)
{
    uint16_t content_top = (uint16_t)(y1 + UI_CARD_TITLE_H + 1U);
    uint16_t rounded_top = (uint16_t)(y2 - 18U);

    LCD_Fill((uint16_t)(x1 + 1U), content_top,
             (uint16_t)(x2 - 1U), rounded_top, fill_color);
    LCD_FillRounded((uint16_t)(x1 + 1U), rounded_top,
                    (uint16_t)(x2 - 1U), (uint16_t)(y2 - 1U),
                    9U, fill_color);
}

static void LCD_ClearDynamicRegions(void)
{
    LCD_ClearCardContent(DATA_X1, DATA_Y1, DATA_X2, DATA_Y2, UI_CARD_BG);
    LCD_ClearCardContent(CLOCK_X1, CLOCK_Y1, CLOCK_X2, CLOCK_Y2, UI_CARD_CLOCK_BG);
}

/* LCD_DrawValuePanel and LCD_DrawValuePanelCN are intentionally kept as
 * two separate functions (not unified via a function pointer) to avoid a
 * per-frame branch on the label type.  Use LCD_DrawValuePanelSpec() for
 * dispatch when the label kind is not known at compile time. */

static void LCD_DrawValuePanel(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                               const char *label, const char *value, uint32_t value_color)
{
    uint8_t value_size = (strlen(value) > 6U) ? 24U : 32U;

    LCD_FillRounded(x1, y1, x2, y2, PANEL_CORNER_RADIUS, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(x1, y1, x2, y2, PANEL_CORNER_RADIUS, UI_BORDER_COLOR);
    LCD_ShowFormatted((uint16_t)(x1 + PANEL_LABEL_X_OFFSET), (uint16_t)(y1 + PANEL_LABEL_Y_OFFSET), 16U,
                      UI_LABEL_COLOR, UI_CARD_ALT_BG, "%s", label);
    LCD_ShowFormatted((uint16_t)(x1 + PANEL_LABEL_X_OFFSET), (uint16_t)(y1 + PANEL_VALUE_Y_OFFSET), value_size,
                      value_color, UI_CARD_ALT_BG, "%s", value);
}

static void LCD_DrawValuePanelCN(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                 const uint8_t *label, const char *value, uint32_t value_color)
{
    uint8_t value_size = (strlen(value) > 6U) ? 24U : 32U;

    LCD_FillRounded(x1, y1, x2, y2, PANEL_CORNER_RADIUS, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(x1, y1, x2, y2, PANEL_CORNER_RADIUS, UI_BORDER_COLOR);
    LCD_ShowChinese((uint16_t)(x1 + PANEL_LABEL_X_OFFSET), (uint16_t)(y1 + PANEL_LABEL_Y_OFFSET), label, 16U,
                    UI_LABEL_COLOR, UI_CARD_ALT_BG);
    LCD_ShowFormatted((uint16_t)(x1 + PANEL_LABEL_X_OFFSET), (uint16_t)(y1 + PANEL_VALUE_Y_OFFSET), value_size,
                      value_color, UI_CARD_ALT_BG, "%s", value);
}

static void LCD_DrawValuePanelSpec(const UiValuePanelSpec *spec, const char *value)
{
    if (spec->label.kind == UI_LABEL_CN)
    {
        LCD_DrawValuePanelCN(spec->rect.x1, spec->rect.y1, spec->rect.x2, spec->rect.y2,
                             spec->label.cn, value, spec->value_color);
    }
    else
    {
        LCD_DrawValuePanel(spec->rect.x1, spec->rect.y1, spec->rect.x2, spec->rect.y2,
                           spec->label.ascii, value, spec->value_color);
    }
}

static void LCD_DrawBitStripSpec(const UiBitStripSpec *spec, uint16_t value)
{
    LCD_DrawBitStrip(spec->x, spec->y, spec->count, value, spec->label);
}

static void LCD_DrawButtonSpec(const UiButtonSpec *spec, uint8_t pressed)
{
    LCD_DrawMode9Button(spec->x, spec->y, spec->label, pressed);
}

static void LCD_DrawValuePanelRow(const UiValuePanelSpec *specs, const char * const *values, uint8_t count)
{
    uint8_t i;

    for (i = 0U; i < count; i++)
    {
        LCD_DrawValuePanelSpec(&specs[i], values[i]);
    }
}

static void LCD_DrawNibbleRow(uint16_t x, uint16_t y, uint8_t first, uint8_t count)
{
    uint8_t i;
    uint16_t px;

    for (i = 0U; i < count; i++)
    {
        px = (uint16_t)(x + (uint16_t)i * NIBBLE_GAP);
        LCD_FillRounded(px, y, (uint16_t)(px + NIBBLE_CELL_W), (uint16_t)(y + NIBBLE_CELL_H),
                        NIBBLE_CORNER_RADIUS, UI_CARD_ALT_BG);
        LCD_DrawRoundedRect(px, y, (uint16_t)(px + NIBBLE_CELL_W), (uint16_t)(y + NIBBLE_CELL_H),
                            NIBBLE_CORNER_RADIUS, UI_BORDER_COLOR);
        LCD_ShowFormatted((uint16_t)(px + NIBBLE_TEXT_X_OFFSET), (uint16_t)(y + NIBBLE_TEXT_Y_OFFSET), 32U,
                          UI_WARN_ACCENT_COLOR, UI_CARD_ALT_BG, "%X",
                          payload_nibbles[(first + i) & 0x07U]);
    }
}

static void LCD_DrawBitStrip(uint16_t x, uint16_t y, uint8_t count, uint16_t value, const uint8_t *label)
{
    uint8_t bit;
    uint8_t on;
    uint16_t px;

    LCD_ShowChinese(x, (uint16_t)(y - BITSTRIP_LABEL_Y_OFFSET), label, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (bit = 0U; bit < count; bit++)
    {
        on = ((value & (uint16_t)(1U << bit)) != 0U) ? 1U : 0U;
        px = (uint16_t)(x + (uint16_t)bit * (BITSTRIP_CELL_W + BITSTRIP_GAP));
        LCD_FillRounded(px, y, (uint16_t)(px + BITSTRIP_CELL_W), (uint16_t)(y + BITSTRIP_CELL_H),
                        BITSTRIP_CORNER_RADIUS,
                        on ? UI_ACCENT_COLOR : UI_CARD_ALT_BG);
        LCD_DrawRoundedRect(px, y, (uint16_t)(px + BITSTRIP_CELL_W), (uint16_t)(y + BITSTRIP_CELL_H),
                            BITSTRIP_CORNER_RADIUS, UI_BORDER_COLOR);
    }
}

static void LCD_DrawPianoKeyStrip(uint16_t x, uint16_t y, uint8_t key_mask)
{
    static const uint8_t piano_black_after[7] = {1U, 1U, 0U, 1U, 1U, 1U, 0U};
    uint8_t key;
    uint8_t on;
    uint16_t key_x;
    uint16_t black_x;
    const uint16_t white_w = 76U;
    const uint16_t white_h = 120U;
    const uint16_t black_w = 38U;
    const uint16_t black_h = 74U;

    for (key = 0U; key < 8U; key++)
    {
        on = ((key_mask & (uint8_t)(1U << key)) != 0U) ? 1U : 0U;
        key_x = (uint16_t)(x + (uint16_t)key * white_w);
        LCD_FillRounded(key_x, y, (uint16_t)(key_x + white_w - 2U),
                        (uint16_t)(y + white_h), 7U,
                        on ? UI_WARN_ACCENT_COLOR : LCD_COLOR_WHITE);
        LCD_DrawRoundedRect(key_x, y, (uint16_t)(key_x + white_w - 2U),
                            (uint16_t)(y + white_h), 7U, UI_BORDER_COLOR);
        LCD_ShowFormatted((uint16_t)(key_x + 26U), (uint16_t)(y + 82U), 24U,
                          on ? UI_BG_COLOR : UI_BORDER_COLOR,
                          on ? UI_WARN_ACCENT_COLOR : LCD_COLOR_WHITE,
                          "%u", (unsigned int)(key + 1U));
    }

    for (key = 0U; key < 7U; key++)
    {
        if (piano_black_after[key] != 0U)
        {
            black_x = (uint16_t)(x + (uint16_t)(key + 1U) * white_w - (black_w / 2U));
            LCD_FillRounded(black_x, y, (uint16_t)(black_x + black_w),
                            (uint16_t)(y + black_h), 5U, LCD_COLOR_BLACK);
            LCD_DrawRoundedRect(black_x, y, (uint16_t)(black_x + black_w),
                                (uint16_t)(y + black_h), 5U, UI_BORDER_COLOR);
        }
    }
}

static void LCD_DrawKeyLedStrip(uint8_t led_mask)
{
    uint8_t led;
    uint8_t on;
    uint16_t x;
    const uint16_t y = 424U;
    const uint16_t led_w = 17U;
    const uint16_t led_h = 20U;
    const uint16_t gap = 7U;

    LCD_ShowFormatted(AUX_KEY_LED_X, AUX_LABEL_Y, 16U, UI_LABEL_COLOR, UI_CARD_CLOCK_BG, "KEY LED");

    for (led = 0U; led < KEY_LED_COUNT; led++)
    {
        on = ((led_mask & (uint8_t)(1U << led)) != 0U) ? 1U : 0U;
        x = (uint16_t)(AUX_KEY_LED_X + (uint16_t)led * (led_w + gap));
        LCD_FillRounded(x, y, (uint16_t)(x + led_w), (uint16_t)(y + led_h), 4U,
                        on ? UI_LED_ON_COLOR : UI_CARD_ALT_BG);
        LCD_DrawRoundedRect(x, y, (uint16_t)(x + led_w), (uint16_t)(y + led_h), 4U,
                            on ? UI_LED_BORDER : UI_BORDER_COLOR);
    }
}

static void LCD_DrawMatrix4x4(uint16_t x, uint16_t y, uint16_t active_mask)
{
    uint8_t row;
    uint8_t col;
    uint8_t bit;
    uint8_t on;
    uint16_t px;
    uint16_t py;

    for (row = 0U; row < 4U; row++)
    {
        for (col = 0U; col < 4U; col++)
        {
            bit = (uint8_t)(row * 4U + col);
            on = ((active_mask & (uint16_t)(1U << bit)) != 0U) ? 1U : 0U;
            px = (uint16_t)(x + (uint16_t)col * 58U);
            py = (uint16_t)(y + (uint16_t)row * 42U);
            LCD_FillRounded(px, py, (uint16_t)(px + 44U), (uint16_t)(py + 30U), 5U,
                            on ? UI_WARN_ACCENT_COLOR : UI_CARD_ALT_BG);
            LCD_DrawRoundedRect(px, py, (uint16_t)(px + 44U), (uint16_t)(py + 30U), 5U, UI_BORDER_COLOR);
        }
    }
}

static void LCD_DrawSegmentH(uint16_t x, uint16_t y, uint16_t w, uint16_t t, uint32_t color)
{
    LCD_Fill((uint16_t)(x + t), y, (uint16_t)(x + w - t - 1U), (uint16_t)(y + t - 1U), color);
}

static void LCD_DrawSegmentV(uint16_t x, uint16_t y, uint16_t h, uint16_t t, uint32_t color)
{
    LCD_Fill(x, (uint16_t)(y + t), (uint16_t)(x + t - 1U), (uint16_t)(y + h - t - 1U), color);
}

static void LCD_DrawSevenSegDigit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t value)
{
    /* 7-seg bit mapping: bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g, bit7=dp
     *   AAA
     *  F   B
     *  F   B
     *   GGG
     *  E   C
     *  E   C
     *   DDD  DP
     */
    static const uint8_t seg_mask[16] = {
        0x3FU, /* 0: abcdef  */  0x06U, /* 1: bc      */  0x5BU, /* 2: abdeg   */  0x4FU, /* 3: abcdg   */
        0x66U, /* 4: bcfg   */  0x6DU, /* 5: acdfg  */  0x7DU, /* 6: acdefg  */  0x07U, /* 7: abc     */
        0x7FU, /* 8: abcdefg */  0x6FU, /* 9: abcdfg */  0x77U, /* A: abcefg  */  0x7CU, /* b: cdefg  */
        0x39U, /* C: adef   */  0x5EU, /* d: bcdeg  */  0x79U, /* E: adefg   */  0x71U  /* F: aefg    */
    };
    const uint16_t t = 10U;
    const uint16_t half_h = (uint16_t)(h / 2U);
    uint8_t mask = seg_mask[value & 0x0FU];
    uint32_t color;

    LCD_FillRounded(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), 8U, UI_DIGIT_BG);
    LCD_DrawRoundedRect(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), 8U, UI_DIGIT_BORDER);

    color = ((mask & 0x01U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentH((uint16_t)(x + 6U), (uint16_t)(y + 6U), (uint16_t)(w - 12U), t, color);
    color = ((mask & 0x02U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + w - t - 6U), (uint16_t)(y + 6U), half_h, t, color);
    color = ((mask & 0x04U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + w - t - 6U), (uint16_t)(y + half_h), half_h, t, color);
    color = ((mask & 0x08U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentH((uint16_t)(x + 6U), (uint16_t)(y + h - t - 6U), (uint16_t)(w - 12U), t, color);
    color = ((mask & 0x10U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + 6U), (uint16_t)(y + half_h), half_h, t, color);
    color = ((mask & 0x20U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + 6U), (uint16_t)(y + 6U), half_h, t, color);
    color = ((mask & 0x40U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentH((uint16_t)(x + 6U), (uint16_t)(y + half_h - (t / 2U)), (uint16_t)(w - 12U), t, color);
}

static LcdMode0Status LCD_ParseMode0Status(uint32_t po_status, uint16_t pio_status)
{
    LcdMode0Status status = {
        .digit_word = po_status,
        .led_status = (uint16_t)(pio_status & 0x0FFFU)
    };

    return status;
}

static uint8_t LCD_MapMode7LogicalKey(uint8_t row_idx, uint8_t col_idx)
{
    static const uint8_t logical_key_map[4][4] = {
        {0xCU, 0xDU, 0xAU, 0xBU},
        {0x8U, 0x9U, 0xEU, 0xFU},
        {0x4U, 0x5U, 0x6U, 0x7U},
        {0x0U, 0x1U, 0x2U, 0x3U}
    };

    if ((row_idx >= 4U) || (col_idx >= 4U))
    {
        return 0U;
    }

    return logical_key_map[row_idx][col_idx];
}

static uint8_t LCD_MapElectricalToGridBit(uint8_t elec_row, uint8_t elec_col)
{
    static const uint8_t grid_bit_map[4][4] = {
        { 3U,  7U, 11U, 15U},
        { 9U, 10U, 12U, 14U},
        { 4U,  5U,  6U,  8U},
        {13U,  0U,  1U,  2U}
    };

    if ((elec_row >= 4U) || (elec_col >= 4U))
    {
        return 0U;
    }
    return grid_bit_map[elec_row][elec_col];
}

/* -- Mode 7 PIO bit layout ---------------------------------------------
 * PIO[15]    : key_valid   (1 = key pressed)
 * PIO[14]    : key_event   (rising-edge detect)
 * PIO[13:12] : reserved
 * PIO[11:10] : col_idx     (0-3)
 * PIO[9:8]   : reserved
 * PIO[7:6]   : row_idx     (0-3)
 * PIO[5:0]   : reserved
 * -------------------------------------------------------------------- */
#define M7_PIO_KEY_VALID_SHIFT              15U
#define M7_PIO_KEY_EVENT_SHIFT              14U
#define M7_PIO_COL_SHIFT                    10U
#define M7_PIO_ROW_SHIFT                    12U
#define M7_PIO_FIELD_MASK                   0x0001U
#define M7_PIO_COL_ROW_MASK                 0x0003U
#define M7_PIO_BUSINESS_MASK                0x03FFU

static LcdMode7Status LCD_ParseMode7Status(uint32_t po_status, uint16_t pio_status)
{
    LcdMode7Status status = {
        .key_value = 0U,
        .key_valid = (uint8_t)((pio_status >> M7_PIO_KEY_VALID_SHIFT) & M7_PIO_FIELD_MASK),
        .key_event = (uint8_t)((pio_status >> M7_PIO_KEY_EVENT_SHIFT) & M7_PIO_FIELD_MASK),
        .multi_key = 0U,
        .key_pressed = 0U,
        .row_idx = (uint8_t)((pio_status >> M7_PIO_ROW_SHIFT) & M7_PIO_COL_ROW_MASK),
        .col_idx = (uint8_t)((pio_status >> M7_PIO_COL_SHIFT) & M7_PIO_COL_ROW_MASK),
        .business_pio = (uint16_t)(pio_status & M7_PIO_BUSINESS_MASK),
        .business_po = po_status
    };

    status.key_pressed = status.key_valid;
    status.key_value = (status.key_valid != 0U) ? LCD_MapMode7LogicalKey(status.row_idx, status.col_idx) : 0U;

    return status;
}

/* -- Mode 8 PO bit layout ----------------------------------------------
 * PO[31]    : step_select
 * PO[30]    : clk_div2
 * PO[29]    : cw_pulse
 * PO[28]    : ccw_pulse
 * PO[27:20] : tick_count
 * PO[19:12] : reserved
 * PO[11:0]  : div_value
 * -------------------------------------------------------------------- */
#define M8_PO_STEP_SELECT_SHIFT             31U
#define M8_PO_CLK_DIV2_SHIFT                30U
#define M8_PO_CW_PULSE_SHIFT                29U
#define M8_PO_CCW_PULSE_SHIFT               28U
#define M8_PO_TICK_COUNT_SHIFT              20U
#define M8_PO_DIV_VALUE_SHIFT               8U
#define M8_PO_FIELD_1BIT_MASK               0x01UL
#define M8_PO_TICK_COUNT_MASK               0xFFUL
#define M8_PO_DIV_VALUE_MASK                0x0FFFUL
#define M8_PIO_LED_MASK                     0x0FFFU

static LcdMode8Status LCD_ParseMode8Status(uint32_t po_status, uint16_t pio_status)
{
    uint8_t digit;
    LcdMode8Status status = {
        .div_value = (uint16_t)((po_status >> M8_PO_DIV_VALUE_SHIFT) & M8_PO_DIV_VALUE_MASK),
        .tick_count = (uint8_t)((po_status >> M8_PO_TICK_COUNT_SHIFT) & M8_PO_TICK_COUNT_MASK),
        .step_select = (uint8_t)((po_status >> M8_PO_STEP_SELECT_SHIFT) & M8_PO_FIELD_1BIT_MASK),
        .clk_div2 = (uint8_t)((po_status >> M8_PO_CLK_DIV2_SHIFT) & M8_PO_FIELD_1BIT_MASK),
        .cw_pulse = (uint8_t)((po_status >> M8_PO_CW_PULSE_SHIFT) & M8_PO_FIELD_1BIT_MASK),
        .ccw_pulse = (uint8_t)((po_status >> M8_PO_CCW_PULSE_SHIFT) & M8_PO_FIELD_1BIT_MASK),
        .led_status = (uint16_t)(pio_status & M8_PIO_LED_MASK)
    };

    for (digit = 0U; digit < 8U; digit++)
    {
        status.display_nibbles[digit] = (uint8_t)((po_status >> ((uint32_t)digit * 4U)) & 0x0FUL);
    }

    return status;
}

/* -- Mode 9 PIO bit layout ---------------------------------------------
 * PIO[15:8]  : reserved
 * PIO[7:3]   : key_mask (5 keys)
 * PIO[6]     : boundary_hit
 * PIO[5:3]   : event_code
 * PIO[2:0]   : cursor_index
 * -------------------------------------------------------------------- */
#define M9_PIO_KEY_MASK_SHIFT               7U
#define M9_PIO_BOUNDARY_HIT_SHIFT           6U
#define M9_PIO_EVENT_CODE_SHIFT             3U
#define M9_PIO_CURSOR_INDEX_MASK            0x0007U
#define M9_PIO_EVENT_CODE_MASK              0x0007U
#define M9_PIO_BOUNDARY_HIT_MASK            0x0001U
#define M9_PIO_KEY_MASK_5BIT                0x001FU
#define M9_PO_CURSOR_ONEHOT_MASK            0xFFUL

static LcdMode9Status LCD_ParseMode9Status(uint32_t po_status, uint16_t pio_status)
{
    LcdMode9Status status = {
        .cursor_onehot = (uint8_t)(po_status & M9_PO_CURSOR_ONEHOT_MASK),
        .cursor_index = (uint8_t)(pio_status & M9_PIO_CURSOR_INDEX_MASK),
        .event_code = (uint8_t)((pio_status >> M9_PIO_EVENT_CODE_SHIFT) & M9_PIO_EVENT_CODE_MASK),
        .boundary_hit = (uint8_t)((pio_status >> M9_PIO_BOUNDARY_HIT_SHIFT) & M9_PIO_BOUNDARY_HIT_MASK),
        .key_mask = (uint8_t)((pio_status >> M9_PIO_KEY_MASK_SHIFT) & M9_PIO_KEY_MASK_5BIT)
    };

    return status;
}

/* -- Mode A PO bit layout ----------------------------------------------
 * PO[31:24] : exp_id
 * PO[23]    : enable
 * PO[22]    : direction
 * PO[21]    : stop_request
 * PO[20]    : drive_active
 * PO[19:16] : aux_code
 * PO[15:0]  : metric_code
 * -------------------------------------------------------------------- */
#define MA_PO_EXP_ID_SHIFT                  24U
#define MA_PO_ENABLE_SHIFT                  23U
#define MA_PO_DIRECTION_SHIFT               22U
#define MA_PO_STOP_REQUEST_SHIFT            21U
#define MA_PO_DRIVE_ACTIVE_SHIFT            20U
#define MA_PO_AUX_CODE_SHIFT                16U
#define MA_PO_FIELD_1BIT_MASK               0x01UL
#define MA_PO_EXP_ID_MASK                   0xFFUL
#define MA_PO_AUX_CODE_MASK                 0x0FUL
#define MA_PO_METRIC_CODE_MASK              0xFFFFUL
#define MA_FMCU_PI_KEY_MASK                 0x00FFU

static LcdModeAStatus LCD_ParseModeAStatus(uint32_t po_status, uint16_t pio_status)
{
    LcdModeAStatus status = {
        .exp_id = (uint8_t)((po_status >> MA_PO_EXP_ID_SHIFT) & MA_PO_EXP_ID_MASK),
        .enable = (uint8_t)((po_status >> MA_PO_ENABLE_SHIFT) & MA_PO_FIELD_1BIT_MASK),
        .direction = (uint8_t)((po_status >> MA_PO_DIRECTION_SHIFT) & MA_PO_FIELD_1BIT_MASK),
        .stop_request = (uint8_t)((po_status >> MA_PO_STOP_REQUEST_SHIFT) & MA_PO_FIELD_1BIT_MASK),
        .drive_active = (uint8_t)((po_status >> MA_PO_DRIVE_ACTIVE_SHIFT) & MA_PO_FIELD_1BIT_MASK),
        .aux_code = (uint8_t)((po_status >> MA_PO_AUX_CODE_SHIFT) & MA_PO_AUX_CODE_MASK),
        .metric_code = (uint16_t)(po_status & MA_PO_METRIC_CODE_MASK),
        .key_mask = (uint8_t)(current_fmcu_pi & MA_FMCU_PI_KEY_MASK),
        .pio_status = pio_status
    };

    return status;
}

static void LCD_DrawMode0View(void)
{
    LcdMode0Status status = LCD_ParseMode0Status(current_xc7a_po, current_xc7a_pio);

    LCD_DrawMode0SevenSegView(status.digit_word);
    LCD_DrawMode0LedView(status.led_status);
}

static void LCD_DrawMode0SevenSegView(uint32_t po_value)
{
    uint8_t digit;
    uint8_t nibble;
    uint16_t x;

    LCD_ShowChinese(SEG_LABEL_X, SEG_LABEL_Y, CH_DIGITS, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (digit = 0U; digit < 8U; digit++)
    {
        nibble = (uint8_t)((po_value >> (28U - (uint32_t)digit * 4U)) & 0x0FU);
        x = (uint16_t)(SEG_DIGIT_X0 + (uint16_t)digit * (SEG_DIGIT_W + SEG_DIGIT_GAP));

        LCD_DrawSevenSegDigit(x, SEG_DIGIT_Y, SEG_DIGIT_W, SEG_DIGIT_H, nibble);
    }
}

static void LCD_DrawMode0LedView(uint16_t pio_value)
{
    uint8_t led;
    uint8_t led_on;
    uint16_t x;

    LCD_ShowChinese(LED_STRIP_LABEL_X, LED_STRIP_LABEL_Y, CH_LED_GROUP, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (led = 0U; led < LED_STRIP_COUNT; led++)
    {
        led_on = ((pio_value & (uint16_t)(1U << led)) != 0U) ? 1U : 0U;
        x = (uint16_t)(LED_STRIP_X0 + (uint16_t)led * (LED_STRIP_CELL_W + LED_STRIP_GAP));

        LCD_FillRounded(x, LED_STRIP_Y, (uint16_t)(x + LED_STRIP_CELL_W - 1U),
                        (uint16_t)(LED_STRIP_Y + LED_STRIP_CELL_H - 1U), 6U,
                        led_on ? UI_LED_ON_COLOR : UI_LED_OFF_COLOR);
        LCD_DrawRoundedRect(x, LED_STRIP_Y, (uint16_t)(x + LED_STRIP_CELL_W - 1U),
                            (uint16_t)(LED_STRIP_Y + LED_STRIP_CELL_H - 1U), 6U, UI_LED_BORDER);
        LCD_ShowFormatted((uint16_t)(x + 10U), (uint16_t)(LED_STRIP_Y + 6U), 16U,
                          led_on ? LCD_COLOR_WHITE : UI_LABEL_COLOR,
                          led_on ? UI_LED_ON_COLOR : UI_LED_OFF_COLOR,
                          "L%02u", (unsigned int)(led + 1U));
    }
}

static void LCD_DrawMode1View(void)
{
    static const UiValuePanelSpec key_chain_panel =
        {{388U, 146U, 734U, 252U}, {UI_LABEL_CN, NULL, CH_KEY_CHAIN}, UI_ACCENT_COLOR};
    static const UiBitStripSpec input_bits = {53U, 296U, 16U, CH_INPUT_BITS};
    char input_word[16];
    uint8_t digit;

    /* Result panel: the 4 result nibbles are what the hardware drives onto the
     * 7-segment displays, so render them as digits instead of plain text. */
    LCD_FillRounded(44U, 146U, 344U, 252U, PANEL_CORNER_RADIUS, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(44U, 146U, 344U, 252U, PANEL_CORNER_RADIUS, UI_BORDER_COLOR);
    LCD_ShowChinese(60U, 156U, CH_RESULT, 16U, UI_LABEL_COLOR, UI_CARD_ALT_BG);
    for (digit = 0U; digit < 4U; digit++)
    {
        LCD_DrawSevenSegDigit((uint16_t)(60U + (uint16_t)digit * 72U), 180U, 52U, 60U,
                              payload_nibbles[4U + digit]);
    }

    snprintf(input_word, sizeof(input_word), "%04X", current_fmcu_pi);
    LCD_DrawValuePanelSpec(&key_chain_panel, input_word);
    LCD_DrawBitStripSpec(&input_bits, current_fmcu_pi);
}

static void LCD_DrawMode2View(void)
{
    static const UiValuePanelSpec panels[] = {
        {{L3_COL_X0, L3_ROW0_Y, L3_COL_X0 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_DECODED_VALUE}, UI_WARN_ACCENT_COLOR},
        {{L3_COL_X1, L3_ROW0_Y, L3_COL_X1 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_SEG_PATTERN}, UI_ACCENT_COLOR},
        {{L3_COL_X2, L3_ROW0_Y, L3_COL_X2 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_INPUT_BITS}, UI_MUTED_VALUE_COLOR},
    };
    char decoded[8];
    char pattern[16];
    char input_word[16];
    const char *values[] = {decoded, pattern, input_word};

    snprintf(decoded, sizeof(decoded), "%X", payload_nibbles[7]);
    snprintf(pattern, sizeof(pattern), "%02X", payload_bytes[3]);
    snprintf(input_word, sizeof(input_word), "PI:%04X", current_fmcu_pi);
    LCD_DrawValuePanelRow(panels, values, 3U);
    LCD_ShowChinese(51U, 246U, CH_DIGITS, 16U, UI_LABEL_COLOR, UI_CARD_BG);
    LCD_DrawNibbleRow(51U, 270U, 0U, 8U);
}

static void LCD_DrawMode3View(void)
{
    static const UiValuePanelSpec event_panel =
        {{184U, 264U, 616U, 328U}, {UI_LABEL_CN, NULL, CH_LAST_EVENT}, UI_ACCENT_COLOR};
    char key_value[20];

    snprintf(key_value, sizeof(key_value), "KEY MASK %02X", (unsigned int)(current_fmcu_pi & 0x00FFU));
    LCD_DrawPianoKeyStrip(92U, 118U, (uint8_t)(current_fmcu_pi & 0x00FFU));

    LCD_DrawValuePanelSpec(&event_panel, key_value);
}

static void LCD_DrawMode4View(void)
{
    static const UiValuePanelSpec panels[] = {
        {{L3_COL_X0, L3_ROW0_Y, L3_COL_X0 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_REGISTER}, UI_WARN_ACCENT_COLOR},
        {{L3_COL_X1, L3_ROW0_Y, L3_COL_X1 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_DIRECTION}, UI_ACCENT_COLOR},
        {{L3_COL_X2, L3_ROW0_Y, L3_COL_X2 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_PIO_STATUS}, UI_MUTED_VALUE_COLOR},
    };
    static const UiBitStripSpec shift_path = {53U, 270U, 16U, CH_SHIFT_PATH};
    char word[16];
    char direction[16];
    char pio_value[16];
    const char *values[] = {word, direction, pio_value};

    snprintf(word, sizeof(word), "%04lX", (unsigned long)(current_xc7a_po & 0xFFFFUL));
    snprintf(direction, sizeof(direction), "%s",
             ((current_xc7a_po & 0x00010000UL) != 0UL) ? "LEFT" : "RIGHT");
    snprintf(pio_value, sizeof(pio_value), "PIO:%04X", current_xc7a_pio);
    LCD_DrawValuePanelRow(panels, values, 3U);
    LCD_DrawBitStripSpec(&shift_path, current_xc7a_pio);
}

static void LCD_DrawMode5View(void)
{
    static const UiValuePanelSpec panels[] = {
        {{L3_COL_X0, L3_ROW0_Y, L3_COL_X0 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_DIGITS}, UI_WARN_ACCENT_COLOR},
        {{L3_COL_X1, L3_ROW0_Y, L3_COL_X1 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_SEG_PATTERN}, UI_ACCENT_COLOR},
        {{L3_COL_X2, L3_ROW0_Y, L3_COL_X2 + L3_COL_W, L3_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_PIO_STATUS}, UI_ACCENT_COLOR},
    };
    static const UiBitStripSpec pio_strip = {141U, 270U, 12U, CH_PIO_BITMAP};
    char digit_value[16];
    char segment_value[16];
    char pio_value[16];
    const char *values[] = {digit_value, segment_value, pio_value};

    snprintf(digit_value, sizeof(digit_value), "DIG:%02lX",
             (unsigned long)((current_xc7a_po >> 8U) & 0xFFUL));
    snprintf(segment_value, sizeof(segment_value), "SEG:%02lX",
             (unsigned long)(current_xc7a_po & 0xFFUL));
    snprintf(pio_value, sizeof(pio_value), "PIO:%03X",
             (unsigned int)(current_xc7a_pio & 0x0FFFU));

    LCD_DrawValuePanelRow(panels, values, 3U);
    LCD_DrawBitStripSpec(&pio_strip, current_xc7a_pio);
}

static void LCD_DrawMode6View(void)
{
    static const UiRect class_cards[] = {
        {42U, 104U, 246U, 196U},
        {298U, 104U, 502U, 196U},
        {554U, 104U, 758U, 196U},
    };
    static const char * const class_groups[] = {"K1/K4/K7", "K2/K5/K8", "K3/K6"};
    static const char * const class_names[] = {"STABLE", "STRETCH", "TOGGLE"};
    static const UiValuePanelSpec pulse_panel =
        {{42U, 214U, 298U, 280U}, {UI_LABEL_CN, NULL, CH_SINGLE_PULSE}, UI_ACCENT_COLOR};
    static const UiBitStripSpec lcd_output_strip = {398U, 276U, 8U, CH_LCD_OUTPUT};
    char stable_value[20];
    char stretch_value[20];
    char toggle_value[20];
    char po_summary[20];
    const char *class_values[] = {stable_value, stretch_value, toggle_value};
    const uint16_t stable_mask = (uint16_t)(current_xc7a_pio & 0x0049U);
    const uint16_t stretch_mask = (uint16_t)(current_xc7a_pio & 0x0092U);
    const uint16_t toggle_mask = (uint16_t)(current_xc7a_pio & 0x0024U);
    uint8_t card;

    snprintf(stable_value, sizeof(stable_value), "PIO:%02X", (unsigned int)stable_mask);
    snprintf(stretch_value, sizeof(stretch_value), "PIO:%02X", (unsigned int)stretch_mask);
    snprintf(toggle_value, sizeof(toggle_value), "PIO:%02X", (unsigned int)toggle_mask);
    snprintf(po_summary, sizeof(po_summary), "PO:%02lX", (unsigned long)(current_xc7a_po & 0xFFUL));

    for (card = 0U; card < 3U; card++)
    {
        const UiRect *rect = &class_cards[card];
        LCD_FillRounded(rect->x1, rect->y1, rect->x2, rect->y2, 8U, UI_CARD_ALT_BG);
        LCD_DrawRoundedRect(rect->x1, rect->y1, rect->x2, rect->y2, 8U, UI_BORDER_COLOR);
        LCD_ShowFormatted((uint16_t)(rect->x1 + 16U), (uint16_t)(rect->y1 + 12U), 16U,
                          UI_LABEL_COLOR, UI_CARD_ALT_BG, "%s", class_groups[card]);
        LCD_ShowFormatted((uint16_t)(rect->x1 + 16U), (uint16_t)(rect->y1 + 36U), 16U,
                          UI_ACCENT_COLOR, UI_CARD_ALT_BG, "%s", class_names[card]);
        LCD_ShowFormatted((uint16_t)(rect->x1 + 16U), (uint16_t)(rect->y1 + 58U), 24U,
                          UI_WARN_ACCENT_COLOR, UI_CARD_ALT_BG, "%s", class_values[card]);
    }

    LCD_DrawValuePanelSpec(&pulse_panel, po_summary);
    LCD_DrawBitStripSpec(&lcd_output_strip, current_xc7a_pio);
}

static void LCD_DrawMode7View(void)
{
    uint8_t row;
    uint8_t col;
    uint16_t label_y;
    uint16_t label_x;
    uint16_t active_mask = 0x0000U;
    char key_line[16];
    char row_col_line[16];
    char event_line[20];
    LcdMode7Status status = LCD_ParseMode7Status(current_xc7a_po, current_xc7a_pio);

    if ((status.key_valid != 0U) && (status.row_idx < 4U) && (status.col_idx < 4U))
    {
        uint8_t grid_bit = LCD_MapElectricalToGridBit(status.row_idx, status.col_idx);
        active_mask = (uint16_t)(1U << grid_bit);
    }

    snprintf(key_line, sizeof(key_line), "KEY:%X",
             (unsigned int)status.key_value);
    snprintf(row_col_line, sizeof(row_col_line), "ROW:%u COL:%u",
             (unsigned int)status.row_idx,
             (unsigned int)status.col_idx);
    snprintf(event_line, sizeof(event_line), "EDGE:%u ERR:%u",
             (unsigned int)status.key_event,
             (unsigned int)status.multi_key);

    LCD_DrawValuePanelCN(LS_COL_X0, LS_ROW0_Y, LS_COL_X1, LS_ROW0_BOT, CH_CODE, key_line, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(LS_COL_X0, LS_ROW1_Y, LS_COL_X1, LS_ROW1_BOT, CH_SCAN, row_col_line, UI_ACCENT_COLOR);
    LCD_DrawValuePanelCN(LS_COL_X0, LS_ROW2_Y, LS_COL_X1, LS_ROW2_BOT, CH_PIO_STATUS, event_line, UI_ACCENT_COLOR);
    LCD_DrawMatrix4x4(LS_MATRIX_X, LS_MATRIX_Y, active_mask);

    for (row = 0U; row < 4U; row++)
    {
        label_y = (uint16_t)(LS_MATRIX_Y + (uint16_t)row * 42U + 20U);
        LCD_ShowFormatted(LS_ROW_LABEL_X, label_y, 16U, UI_LABEL_COLOR, UI_CARD_BG,
                          "R%u", (unsigned int)row);
    }

    for (col = 0U; col < 4U; col++)
    {
        label_x = (uint16_t)(LS_MATRIX_X + (uint16_t)col * 58U + 10U);
        LCD_ShowFormatted(label_x, LS_COL_LABEL_Y, 16U, UI_LABEL_COLOR, UI_CARD_BG,
                          "C%u", (unsigned int)col);
    }
}

static void LCD_DrawMode8View(void)
{
    char param_value[20];
    char count_value[12];
    char step_value[8];
    char dir_value[20];
    uint8_t digit;
    uint8_t nibble;
    uint16_t x;
    LcdMode8Status status = LCD_ParseMode8Status(current_xc7a_po, current_xc7a_pio);
    const uint16_t digit_y = 204U;
    const uint16_t digit_w = 56U;
    const uint16_t digit_h = 72U;
    const uint16_t gap = 30U;

    snprintf(param_value, sizeof(param_value), "DIV:%u", (unsigned int)status.div_value);
    snprintf(count_value, sizeof(count_value), "T:%u", (unsigned int)status.tick_count);
    snprintf(step_value, sizeof(step_value), "%s", (status.step_select != 0U) ? "HALF" : "FULL");
    snprintf(dir_value, sizeof(dir_value), "CW:%u CCW:%u",
             (unsigned int)status.cw_pulse,
             (unsigned int)status.ccw_pulse);

    LCD_DrawValuePanelCN(42U, 104U, 226U, 168U, CH_PARAM, param_value, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(244U, 104U, 390U, 168U, CH_COUNT, count_value, UI_ACCENT_COLOR);
    LCD_DrawValuePanel(408U, 104U, 544U, 168U, "STEP", step_value, UI_ACCENT_COLOR);
    LCD_DrawValuePanel(562U, 104U, 756U, 168U, "DIR", dir_value, UI_ACCENT_COLOR);
    LCD_ShowChinese(44U, 190U, CH_DIGITS, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (digit = 0U; digit < 8U; digit++)
    {
        nibble = status.display_nibbles[digit];
        x = (uint16_t)(44U + (uint16_t)digit * (digit_w + gap));
        LCD_DrawSevenSegDigit(x, digit_y, digit_w, digit_h, nibble);
    }

    LCD_DrawMode0LedView(status.led_status);
}

static uint8_t LCD_GetMode9CursorPos(uint8_t cursor_onehot, uint8_t fallback_index)
{
    uint8_t bit;
    uint8_t onehot_count = 0U;
    uint8_t pos = 0U;

    for (bit = 0U; bit < 8U; bit++)
    {
        if ((cursor_onehot & (uint8_t)(1U << bit)) != 0U)
        {
            pos = bit;
            onehot_count++;
        }
    }

    if (onehot_count == 1U)
    {
        return pos;
    }

    return fallback_index;
}

static void LCD_DrawMode9Button(uint16_t x, uint16_t y, const char *label, uint8_t pressed)
{
    const uint16_t w = 110U;
    const uint16_t h = 50U;
    const uint16_t char_w = 12U;
    const uint16_t text_w = (uint16_t)(strlen(label) * char_w);
    uint16_t text_x = (uint16_t)(x + ((w > text_w) ? ((w - text_w) / 2U) : 0U));
    uint16_t text_y = (uint16_t)(y + 13U);
    uint32_t bg = (pressed != 0U) ? UI_STATUS_OK_BG : UI_CARD_ALT_BG;
    uint32_t fg = (pressed != 0U) ? LCD_COLOR_WHITE : UI_LABEL_COLOR;

    LCD_FillRounded(x, y, (uint16_t)(x + w), (uint16_t)(y + h), 8U, bg);
    LCD_DrawRoundedRect(x, y, (uint16_t)(x + w), (uint16_t)(y + h), 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(text_x, text_y, 24U, fg, bg, "%s", label);
}

static void LCD_DrawMode9View(void)
{
    static const UiValuePanelSpec panels[] = {
        {{44U, 108U, 332U, 172U}, {UI_LABEL_CN, NULL, CH_PIO_STATUS}, UI_ACCENT_COLOR},
        {{44U, 196U, 332U, 260U}, {UI_LABEL_ASCII, "BOUND", NULL}, UI_WARN_ACCENT_COLOR},
        {{44U, 284U, 332U, 348U}, {UI_LABEL_ASCII, "CURSOR", NULL}, UI_MUTED_VALUE_COLOR},
    };
    static const UiButtonSpec buttons[] = {
        {482U, 124U, "UP"},
        {482U, 256U, "DOWN"},
        {356U, 190U, "LEFT"},
        {608U, 190U, "RIGHT"},
        {482U, 190U, "OK"},
    };
    char pio_summary[24];
    char boundary_summary[24];
    char cursor_summary[24];
    const char *values[] = {pio_summary, boundary_summary, cursor_summary};
    LcdMode9Status status = LCD_ParseMode9Status(current_xc7a_po, current_xc7a_pio);
    uint8_t current_pos = LCD_GetMode9CursorPos(status.cursor_onehot, status.cursor_index);
    uint8_t pressed[] = {
        (uint8_t)(status.key_mask % 2U),
        (uint8_t)((status.key_mask / 2U) % 2U),
        (uint8_t)((status.key_mask / 4U) % 2U),
        (uint8_t)((status.key_mask / 8U) % 2U),
        (uint8_t)((status.key_mask / 16U) % 2U),
    };
    uint8_t button;

    snprintf(pio_summary, sizeof(pio_summary), "IDX:%u EVT:%u",
             (unsigned int)current_pos,
             (unsigned int)status.event_code);
    snprintf(boundary_summary, sizeof(boundary_summary), "HIT:%u MASK:%02X",
             (unsigned int)status.boundary_hit,
             (unsigned int)status.key_mask);
    snprintf(cursor_summary, sizeof(cursor_summary), "ONEHOT:%02X",
             (unsigned int)status.cursor_onehot);

    LCD_DrawValuePanelRow(panels, values, 3U);

    for (button = 0U; button < 5U; button++)
    {
        LCD_DrawButtonSpec(&buttons[button], pressed[button]);
    }
}

static void LCD_DrawModeAView(void)
{
    static const UiValuePanelSpec panels[] = {
        {{42U, 108U, 314U, 188U}, {UI_LABEL_CN, NULL, CH_CONTROL}, UI_WARN_ACCENT_COLOR},
        {{346U, 108U, 536U, 188U}, {UI_LABEL_CN, NULL, CH_PARAM}, UI_ACCENT_COLOR},
        {{568U, 108U, 758U, 188U}, {UI_LABEL_CN, NULL, CH_STATUS}, UI_ACCENT_COLOR},
        {{42U, 216U, 384U, 280U}, {UI_LABEL_CN, NULL, CH_OUTPUT}, UI_WARN_ACCENT_COLOR},
        {{416U, 216U, 758U, 280U}, {UI_LABEL_ASCII, "AUX", NULL}, UI_MUTED_VALUE_COLOR},
    };
    static const UiBitStripSpec control_keys = {189U, 318U, 8U, CH_KEY_BITMAP};
    char enable_line[20];
    char metric_line[20];
    char stop_line[20];
    char id_line[20];
    char aux_line[24];
    const char *values[] = {enable_line, metric_line, stop_line, id_line, aux_line};
    LcdModeAStatus status = LCD_ParseModeAStatus(current_xc7a_po, current_xc7a_pio);
    const ExperimentPageDescriptor *descriptor = LCD_FindExperimentPage(current_mode, status.exp_id);
    char direction_char = (status.direction != 0U) ? 'R' : 'F';

    if (status.exp_id == 0xD3U) {
        snprintf(enable_line, sizeof(enable_line), "D3 STP EN:%u",
                 (unsigned int)status.enable);
        snprintf(metric_line, sizeof(metric_line), "DIV:%05u",
                 (unsigned int)status.metric_code);
        snprintf(stop_line, sizeof(stop_line), "HLD:%u OUT:%u",
                 (unsigned int)status.stop_request,
                 (unsigned int)status.drive_active);
        snprintf(id_line, sizeof(id_line), "EXP:D3 DIR:%c", direction_char);
    } else if (status.exp_id == 0xD2U) {
        snprintf(enable_line, sizeof(enable_line), "D2 DC EN:%u",
                 (unsigned int)status.enable);
        snprintf(metric_line, sizeof(metric_line), "SPD:%uHz",
                 (unsigned int)status.metric_code);
        snprintf(stop_line, sizeof(stop_line), "BRK:%u OUT:%u",
                 (unsigned int)status.stop_request,
                 (unsigned int)status.drive_active);
        snprintf(id_line, sizeof(id_line), "EXP:D2 DIR:%c", direction_char);
    } else {
        LCD_DrawGenericExperimentStatusView(descriptor);
        return;
    }

    snprintf(aux_line, sizeof(aux_line), "DUTY:%X PIO:%04X",
             (unsigned int)status.aux_code,
             (unsigned int)status.pio_status);

    LCD_DrawValuePanelRow(panels, values, 5U);
    LCD_DrawBitStripSpec(&control_keys, status.key_mask);
}

static void LCD_DrawGenericExperimentStatusView(const ExperimentPageDescriptor *descriptor)
{
    static const UiValuePanelSpec panels[] = {
        {{L5_COL0_X0, L5_ROW0_Y,  L5_COL0_X1, L5_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_DISPLAY},       UI_WARN_ACCENT_COLOR},
        {{L5_COL1_X0, L5_ROW0_Y,  L5_COL1_X1, L5_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_CONTROL_KEYS}, UI_ACCENT_COLOR},
        {{L5_COL2_X0, L5_ROW0_Y,  L5_COL2_X1, L5_ROW0_BOT}, {UI_LABEL_CN, NULL, CH_PARAM},        UI_MUTED_VALUE_COLOR},
        {{L5_BOT_COL0_X0, L5_ROW1_Y, L5_BOT_COL0_X1, L5_ROW1_BOT}, {UI_LABEL_CN, NULL, CH_OUTPUT},     UI_WARN_ACCENT_COLOR},
        {{L5_BOT_COL1_X0, L5_ROW1_Y, L5_BOT_COL1_X1, L5_ROW1_BOT}, {UI_LABEL_CN, NULL, CH_PIO_STATUS}, UI_ACCENT_COLOR},
    };
    static const UiBitStripSpec control_keys = {L5_BITSTRIP_X, L5_BITSTRIP_Y, 8U, CH_KEY_BITMAP};
    char exp_line[28];
    char key_line[20];
    char po_line[20];
    char pio_line[20];
    char param_line[20];
    const char *values[] = {exp_line, key_line, param_line, po_line, pio_line};
    const char *name = (descriptor != NULL) ? descriptor->name : "UNREGISTERED";
    uint8_t exp_id = (uint8_t)(current_xc7a_po >> 24U);

    snprintf(exp_line, sizeof(exp_line), "EXP:%02X %s", (unsigned int)exp_id, name);
    snprintf(key_line, sizeof(key_line), "KEY:%02X", (unsigned int)(current_fmcu_pi & 0x00FFU));
    snprintf(po_line, sizeof(po_line), "PO:%06lX", (unsigned long)(current_xc7a_po & 0x00FFFFFFUL));
    snprintf(pio_line, sizeof(pio_line), "PIO:%04X", current_xc7a_pio);
    snprintf(param_line, sizeof(param_line), "NO PARAM");

    LCD_DrawValuePanelRow(panels, values, 5U);
    LCD_DrawBitStripSpec(&control_keys, (uint16_t)(current_fmcu_pi & 0x00FFU));
}

static void LCD_DrawModeBView(void)
{
    /* Mode B reuses the generic experiment status layout (same panel structure).
     * All coordinates are controlled by the L5_* layout grid macros. */
    const ExperimentPageDescriptor *descriptor =
        LCD_FindExperimentPage(current_mode, (uint8_t)(current_xc7a_po >> 24U));
    LCD_DrawGenericExperimentStatusView(descriptor);
}

static void LCD_DrawModeCView(void)
{
    const ExperimentPageDescriptor *descriptor =
        LCD_FindExperimentPage(current_mode, (uint8_t)(current_xc7a_po >> 24U));

    LCD_DrawGenericExperimentStatusView(descriptor);
}

static void LCD_UpdateStatusCard(uint32_t status_bg, uint32_t status_text)
{
    LCD_Fill(STATUS_X1, STATUS_Y1, STATUS_X2, STATUS_Y2, UI_HEADER_BG);
    LCD_DrawLine(0U, UI_HEADER_BOTTOM_Y, LCD_WIDTH - 1U, UI_HEADER_BOTTOM_Y, UI_BORDER_COLOR);
    LCD_ShowFormatted(HEADER_TITLE_X, HEADER_TITLE_Y, HEADER_TITLE_TEXT_SIZE, status_text, UI_HEADER_BG,
                      "M%X  %s", current_mode, get_mode_name(current_mode));
    LCD_ShowFormatted(HEADER_RESOURCE_X, HEADER_RESOURCE_Y, 16U, UI_HEADER_SUB, UI_HEADER_BG,
                      "%s", get_mode_resource_subtitle(current_mode));

    if (last_spi_status == HAL_OK)
    {
        LCD_ShowChinese(STATUS_LINK_LABEL_X, UI_STATUS_TEXT_Y, CH_LINK, 16U, status_bg, UI_HEADER_BG);
        LCD_ShowChinese(STATUS_LINK_VALUE_X, UI_STATUS_TEXT_Y, CH_READY, 16U, status_bg, UI_HEADER_BG);
    }
    else
    {
        LCD_ShowChinese(STATUS_LINK_LABEL_X, UI_STATUS_TEXT_Y, CH_LINK, 16U, status_bg, UI_HEADER_BG);
        LCD_ShowChinese(STATUS_LINK_VALUE_X, UI_STATUS_TEXT_Y, CH_LINK_DISCONNECT, 16U, status_bg, UI_HEADER_BG);
    }
}

static void LCD_UpdateDataCard(void)
{
    get_mode_ui_config(current_mode)->draw_view();
}

static void LCD_UpdateAuxCard(void)
{
    char freq_line[24];
    char mode_line[24];
    const ModeUiConfig *config = get_mode_ui_config(current_mode);
    uint8_t mode_text_size;
    uint32_t freq_color;

    snprintf(freq_line, sizeof(freq_line), "%s", get_frequency_name(current_clk_sel));
    snprintf(mode_line, sizeof(mode_line), "M%X %s", current_mode, get_mode_short_name(current_mode));
    mode_text_size = (config->compact_aux_mode_text != 0U) ? 16U : 24U;
    freq_color = (current_freq_full_select_mode != 0U) ? UI_FREQ_FULL_SELECT : UI_WARN_ACCENT_COLOR;

    LCD_Fill(AUX_FREQ_VALUE_X1, AUX_FREQ_VALUE_Y1, AUX_FREQ_VALUE_X2, AUX_FREQ_VALUE_Y2,
             UI_CARD_CLOCK_BG);
    LCD_Fill(AUX_MODE_VALUE_X1, AUX_MODE_VALUE_Y1, AUX_MODE_VALUE_X2, AUX_MODE_VALUE_Y2,
             UI_CARD_CLOCK_BG);
    LCD_Fill(AUX_KEY_LED_VALUE_X1, AUX_KEY_LED_VALUE_Y1, AUX_KEY_LED_VALUE_X2, AUX_KEY_LED_VALUE_Y2,
             UI_CARD_CLOCK_BG);

    LCD_ShowChinese(AUX_FREQ_X, AUX_LABEL_Y, CH_FREQ, 16U, UI_LABEL_COLOR, UI_CARD_CLOCK_BG);
    LCD_ShowFormatted(AUX_FREQ_X, AUX_VALUE_Y, 32U, freq_color, UI_CARD_CLOCK_BG,
                      "%s", freq_line);
    LCD_ShowChinese(AUX_MODE_X, AUX_LABEL_Y, CH_CUR_MODE, 16U, UI_LABEL_COLOR, UI_CARD_CLOCK_BG);
    LCD_ShowFormatted(AUX_MODE_X, 420U, mode_text_size, UI_VALUE_COLOR, UI_CARD_CLOCK_BG,
                      "%s", mode_line);
    if (mode_shows_key_led_strip(current_mode) != 0U)
    {
        LCD_DrawKeyLedStrip(current_key_led_mask);
    }
}

/* Dirty-check orchestrator: compute 13 dirty flags each frame and only
 * redraw the LCD card regions that actually changed. This minimizes LTDC
 * traffic and keeps the 30 Hz refresh budget. */
static void LCD_Display_Update(void)
{
    static uint8_t ui_cache_valid = 0U;
    static uint8_t prev_mode = 0U;
    static uint8_t prev_clk_sel = 0U;
    static uint8_t prev_freq_full_select_mode = 0U;
    static HAL_StatusTypeDef prev_spi_status = HAL_OK;
    static uint16_t prev_rx_word0 = 0U;
    static uint16_t prev_rx_word1 = 0U;
    static uint16_t prev_rx_word2 = 0U;
    static uint16_t prev_fmcu_pi = 0U;
    static uint32_t prev_xc7a_po = 0U;
    static uint16_t prev_xc7a_pio = 0U;
    static uint8_t prev_key_led_mask = 0U;
    static uint8_t prev_view_signature = 0xFFU;
    uint8_t lcd_output_mode;
    uint8_t key_chain_mode;
    uint8_t mode_dirty;
    uint8_t clk_dirty;
    uint8_t status_dirty;
    uint8_t link_state_dirty;
    uint8_t legacy_dirty;
    uint8_t po_dirty;
    uint8_t pio_dirty;
    uint8_t key_led_dirty;
    uint8_t view_signature;
    uint8_t view_dirty;
    uint8_t layout_dirty;
    uint8_t data_dirty;
    uint8_t aux_dirty;
    uint32_t status_bg;
    uint32_t status_text = LCD_COLOR_WHITE;

    lcd_output_mode = mode_has_lcd_output(current_mode);
    key_chain_mode = mode_uses_key_chain_display(current_mode);
    /* Mode A switches between the D2/D3 motor panels and the generic status
     * view without a mode-number change; the two layouts differ, so the
     * signature change must force a full clear to avoid on-screen residue. */
    view_signature = current_mode;
    if (current_mode == 0xAU)
    {
        const uint8_t exp_id = (uint8_t)(current_xc7a_po >> 24U);
        view_signature = ((exp_id == 0xD2U) || (exp_id == 0xD3U)) ? 0xA1U : 0xA0U;
    }
    view_dirty = ((ui_cache_valid == 0U) || (prev_view_signature != view_signature)) ? 1U : 0U;
    mode_dirty = ((ui_cache_valid == 0U) || (prev_mode != current_mode)) ? 1U : 0U;
    clk_dirty = ((ui_cache_valid == 0U) || (prev_clk_sel != current_clk_sel)
                 || (prev_freq_full_select_mode != current_freq_full_select_mode)) ? 1U : 0U;
    status_dirty = ((ui_cache_valid == 0U) || (prev_spi_status != last_spi_status) || (mode_dirty != 0U)) ? 1U : 0U;
    link_state_dirty = ((ui_cache_valid == 0U) ||
                        ((prev_spi_status == HAL_OK) != (last_spi_status == HAL_OK))) ? 1U : 0U;
    legacy_dirty = ((prev_rx_word0 != rx_word0) ||
                    (prev_rx_word1 != rx_word1) ||
                    (prev_rx_word2 != rx_word2) ||
                    (prev_fmcu_pi != current_fmcu_pi)) ? 1U : 0U;
    po_dirty = (prev_xc7a_po != current_xc7a_po) ? 1U : 0U;
    pio_dirty = (prev_xc7a_pio != current_xc7a_pio) ? 1U : 0U;
    key_led_dirty = ((ui_cache_valid == 0U) || (prev_key_led_mask != current_key_led_mask)) ? 1U : 0U;
    layout_dirty = ((ui_cache_valid == 0U) || (mode_dirty != 0U) || (link_state_dirty != 0U) ||
                    (view_dirty != 0U)) ? 1U : 0U;
    data_dirty = ((layout_dirty != 0U) ||
                  ((key_chain_mode != 0U) && (legacy_dirty != 0U)) ||
                  ((lcd_output_mode != 0U) &&
                   ((legacy_dirty != 0U) || (po_dirty != 0U) || (pio_dirty != 0U)))) ? 1U : 0U;
    aux_dirty = ((layout_dirty != 0U) || (clk_dirty != 0U) || (status_dirty != 0U) || (key_led_dirty != 0U)) ? 1U : 0U;

    if ((ui_cache_valid != 0U) &&
        (status_dirty == 0U) &&
        (data_dirty == 0U) &&
        (aux_dirty == 0U) &&
        ((lcd_output_mode == 0U) || ((po_dirty == 0U) && (pio_dirty == 0U))))
    {
        return;
    }

    ui_cache_valid = 1U;
    prev_mode = current_mode;
    prev_clk_sel = current_clk_sel;
    prev_freq_full_select_mode = current_freq_full_select_mode;
    prev_spi_status = last_spi_status;
    prev_rx_word0 = rx_word0;
    prev_rx_word1 = rx_word1;
    prev_rx_word2 = rx_word2;
    prev_fmcu_pi = current_fmcu_pi;
    prev_xc7a_po = current_xc7a_po;
    prev_xc7a_pio = current_xc7a_pio;
    prev_key_led_mask = current_key_led_mask;
    prev_view_signature = view_signature;

    if (last_spi_status == HAL_OK)
    {
        status_bg = UI_STATUS_OK_BG;
    }
    else
    {
        status_bg = UI_STATUS_ERR_BG;
    }

    if (layout_dirty != 0U)
    {
        LCD_ClearDynamicRegions();
    }

    if (status_dirty != 0U)
    {
        LCD_UpdateStatusCard(status_bg, status_text);
        LCD_RefreshRect(STATUS_X1, STATUS_Y1, STATUS_X2, STATUS_Y2);
    }

    if (data_dirty != 0U)
    {
        LCD_UpdateDataCard();
        LCD_RefreshRect((uint16_t)(DATA_X1 + 2U), (uint16_t)(DATA_Y1 + UI_CARD_TITLE_H + 2U),
                        (uint16_t)(DATA_X2 - 2U), (uint16_t)(DATA_Y2 - 2U));
    }

    if (aux_dirty != 0U)
    {
        LCD_UpdateAuxCard();
        LCD_RefreshRect((uint16_t)(CLOCK_X1 + 2U), (uint16_t)(CLOCK_Y1 + UI_CARD_TITLE_H + 2U),
                        (uint16_t)(CLOCK_X2 - 2U), (uint16_t)(CLOCK_Y2 - 2U));
    }
}

static void SDRAM_BasicTest(void)
{
    const uint32_t framebuffer_bytes = (uint32_t)LCD_RGB_WIDTH * (uint32_t)LCD_RGB_HEIGHT * sizeof(uint16_t);
    if (bsp_TestExtSDRAM_Block(LCD_RGB_FB_ADDR, framebuffer_bytes) != 0U)
    {
        Error_Handler();
    }
}

static void Framebuffer_Init(void)
{
    LCD_FillFramebufferColor(0x0000U);
}

static void LCD_FillFramebufferColor(uint16_t rgb565)
{
    uint32_t i;
    uint16_t *framebuffer = (uint16_t *)LCD_RGB_FB_ADDR;
    const uint32_t total_pixels = LCD_RGB_WIDTH * LCD_RGB_HEIGHT;

    for (i = 0U; i < total_pixels; i++)
    {
        framebuffer[i] = rgb565;
    }

    SCB_CleanDCache_by_Addr((uint32_t *)LCD_RGB_FB_ADDR,
                            (int32_t)(total_pixels * sizeof(uint16_t)));
}

static void LCD_DiagnosticTest(void)
{
    while (1)
    {
        LCD_FillFramebufferColor(0xF800U);
        HAL_Delay(1000U);

        LCD_FillFramebufferColor(0x07E0U);
        HAL_Delay(1000U);

        LCD_FillFramebufferColor(0x001FU);
        HAL_Delay(1000U);

        LCD_FillFramebufferColor(0xFFFFU);
        HAL_Delay(1000U);
    }
}

/* -- Mode name tables (indexed by mode number 0x0..0xF) --
 * Adding a new mode name only requires an entry in the appropriate array. */
static const char * const g_mode_names[NUM_MODES] = {
    [0x0] = "RESULT DISPLAY",
    [0x1] = "INPUT MAPPING",
    [0x2] = "DECODER SELECT",
    [0x3] = "EVENT TRIGGER",
    [0x4] = "PARALLEL INPUT",
    [0x5] = "SCAN DISPLAY",
    [0x6] = "SIGNAL SHAPING",
    [0x7] = "MATRIX KEYS",
    [0x8] = "ROTARY CONTROL",
    [0x9] = "NAV CONTROL",
    [0xA] = "CONTROL PANEL",
    [0xB] = "FULL STATUS VIEW",
    [0xC] = "REMOTE CONTROL",
};

static const char * const g_mode_short_names[NUM_MODES] = {
    [0x7] = "KEYPAD",
    [0x8] = "ENCODER",
    [0x9] = "5WAY",
    [0xA] = "CLOCK",
};

const char *get_mode_name(uint8_t mode)
{
    if (mode < NUM_MODES && g_mode_names[mode] != NULL) {
        return g_mode_names[mode];
    }
    return "UNKNOWN";
}

const char *get_mode_short_name(uint8_t mode)
{
    if (mode < NUM_MODES && g_mode_short_names[mode] != NULL) {
        return g_mode_short_names[mode];
    }
    return get_mode_name(mode);
}

const char *get_frequency_name(uint8_t clk_sel)
{
    static const char * const freq_names[] = {
        "0.5Hz", "1Hz",   "2Hz",   "4Hz",   "8Hz",    "16Hz",   "32Hz",
        "64Hz",  "128Hz", "256Hz", "512Hz", "1kHz",   "2kHz",   "4kHz",
        "16kHz", "32kHz", "64kHz", "623.377kHz", "750kHz", "1.263MHz", "2.526MHz",
        "3MHz",  "4.8MHz", "6MHz",  "9.6MHz",   "12MHz",  "16MHz",   "24MHz"
    };

    if (clk_sel < (sizeof(freq_names) / sizeof(freq_names[0])))
    {
        return freq_names[clk_sel];
    }

    return "UNKNOWN";
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    MODIFY_REG(PWR->CR3, PWR_CR3_SCUEN, 0);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    /* -- PLL1 (SYSCLK) --------------------------------------------
     * HSE = 25 MHz
     * PLLM = 5   ? VCO input = 25 / 5 = 5 MHz
     * PLLN = 160 ? VCO = 5 x 160 = 800 MHz
     * PLLP = 2   ? SYSCLK = 800 / 2 = 400 MHz
     * PLLQ = 4   ? PLL1Q = 800 / 4 = 200 MHz (for peripherals)
     * PLLR = 2   ? PLL1R = 800 / 2 = 400 MHz
     * ----------------------------------------------------------- */
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC | RCC_PERIPHCLK_SPI123;
    PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
    /* -- PLL3 (peripheral clocks) ----------------------------------
     * HSE = 25 MHz
     * PLL3M = 5  ? VCO input = 25 / 5 = 5 MHz
     * PLL3N = 88 ? VCO = 5 x 88 = 440 MHz
     * PLL3P = 2  ? SPI123 clock = 440 / 2 = 220 MHz
     * PLL3Q = 4  ? PLL3Q = 440 / 4 = 110 MHz
     * PLL3R = 18 ? LTDC pixel clock = 440 / 18 ' 24.44 MHz
     * ----------------------------------------------------------- */
    PeriphClkInitStruct.PLL3.PLL3M = 5;
    PeriphClkInitStruct.PLL3.PLL3N = 88;
    PeriphClkInitStruct.PLL3.PLL3P = 2;
    PeriphClkInitStruct.PLL3.PLL3Q = 4;
    PeriphClkInitStruct.PLL3.PLL3R = 18;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_CSI_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_EnableCompensationCell();
}

void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x24000000U;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
    MPU_InitStruct.SubRegionDisable = 0x00U;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x60000000U;
    MPU_InitStruct.Size = ARM_MPU_REGION_SIZE_64KB;
    MPU_InitStruct.SubRegionDisable = 0x00U;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    MPU_InitStruct.Number = MPU_REGION_NUMBER2;
    MPU_InitStruct.BaseAddress = EXT_SDRAM_ADDR;
    MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
    MPU_InitStruct.SubRegionDisable = 0x00U;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif

