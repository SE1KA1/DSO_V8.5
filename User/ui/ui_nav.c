/**
 * @file ui_nav.c
 * @brief 页面导航与 EC1/EC2 编码器输入路由（由 UI_EcPollTask 周期调用 UI_Nav_Poll）。
 *
 * 硬件：MCP23017 I2C IO 扩展，EC1=GPB0..2、EC2=GPB3..5（A/B/R）。
 * 正交表解码 + 去抖 + 连发冷却；示波器页支持 EC1 长按进入侧栏菜单模式。
 *
 * 页面与旋钮映射（摘要）：
 *   UI_SCR_SCOPE  — EC1/EC2 调 ms/div、V/div 或 CH 位移；长按 EC1 切 WAVE↔MENU。
 *   UI_SCR_CHANNEL — EC1 切换焦点（CH1/CH2 的 ON/OFF 与耦合），EC2 改选项。
 *   UI_SCR_TRIGGER — EC1 移动焦点行，EC2 改选项（源/边沿/模式/电平预设）。
 *   UI_SCR_SYSTEM  — EC1 选行，EC2 切换 8-bit/10-bit 采集格式。
 *   UI_SCR_MAIN_MENU — UP/DOWN 选卡片，OK 进入对应子页。
 *
 * 返回键：子页 → 示波器；主菜单 → 欢迎页。
 */
#include "bsp.h"
#include "bsp_ui_io_expander.h"
#include "ui_app_config.h"
#include "ui_nav.h"
#include "ui_trigger_ui.h"
#include "UI_WaveformCtrl.h"
#include "ui_system_setup.h"

static volatile UI_ScreenId_t    s_screen = UI_SCR_SCOPE;
static volatile uint8_t          s_redraw_req;
static volatile int              s_menu_focus;
static volatile UI_ScopeUiMode_t s_scope_ui_mode = UI_SCOPE_UI_WAVE;

/* EC1: GPB2=A GPB1=B GPB0=R；EC2: GPB5=A GPB4=B GPB3=R（跳线切到 EC2 后有效） */
#define EC1_AB_SHIFT     1
/* 宏定义: EC2_AB_SHIFT - 当前模块使用的配置常量。 */
#define EC2_AB_SHIFT     4

/* 按键连发间隔（ms），兼作去抖 */
#define BTN_COOLDOWN_MS  180u
/* EC11 按键：连续 N 次采样一致才翻转滤波状态，避免按住时毛刺被当成「松手」导致长按在按住阶段误触发 */
#define EC_BTN_DEBOUNCE_MATCHES  2u

/* 波形页：长按阈值 */
#define EC_SCOPE_LONG_MS 550u

/* 旋钮功能模式：0=档位调节（ms/div, V/div），1=位置调节（CH1/CH2 垂直位移） */
#define KNOB_MODE_SCALE    0
/* 宏定义: KNOB_MODE_POSITION - 当前模块使用的配置常量。 */
#define KNOB_MODE_POSITION 1
static uint8_t s_knob_mode = KNOB_MODE_SCALE;

/* 正交状态转移 → -1 / 0 / +1（与常见 Gray 码表一致） */
static const int8_t s_quad_tbl[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
};

static uint8_t s_ec1_prev_ab;
static uint8_t s_ec2_prev_ab;
static uint8_t s_ec1_btn_prev;
static uint8_t s_ec2_btn_prev;
static uint8_t s_ec1_btn_filt;
static uint8_t s_ec2_btn_filt;
static uint8_t s_ec1_btn_debounce;
static uint8_t s_ec2_btn_debounce;
static uint8_t s_ec_btn_db_arm;
static TickType_t s_ec1_last_ms;
static TickType_t s_ec2_last_ms;
static uint8_t s_io_inited;
static uint8_t s_enc_arm;
static int16_t s_ec1_pulse_accum;
static int16_t s_ec2_pulse_accum;
/* 上次有效单步方向 ±1，用于快转时 A/B 同时跳变（对角）时补 ±2 步 */
static int8_t  s_ec1_last_step;
static int8_t  s_ec2_last_step;

/* 波形页 EC1/EC2：按下时刻与组合判定（松手时再决定是否执行原长按） */
static TickType_t s_ec1_scope_press_tick;
static TickType_t s_ec2_scope_press_tick;
/* 本段按下内若发生过对侧旋钮旋转 → 组合模式，松手时不执行本键原长按 */
static uint8_t    s_ec1_scope_combo_lock;
static uint8_t    s_ec2_scope_combo_lock;
/* 从其它页切入波形页时，吞掉已按下按键的首次松手，避免跨页误触发 */
static uint8_t    s_ec1_scope_suppress_release;
static uint8_t    s_ec2_scope_suppress_release;
static uint8_t    s_scope_enter_pending;

/*
*********************************************************************************************************
*   函 数 名: nav_sync_scope_entry
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void nav_sync_scope_entry(uint8_t ec1_down, uint8_t ec2_down, TickType_t now)
{
    s_ec1_scope_press_tick = now;
    s_ec2_scope_press_tick = now;
    s_ec1_scope_combo_lock = 0u;
    s_ec2_scope_combo_lock = 0u;
    s_ec1_scope_suppress_release = ec1_down;
    s_ec2_scope_suppress_release = ec2_down;
    s_ec1_btn_prev = ec1_down;
    s_ec2_btn_prev = ec2_down;
    s_ec1_pulse_accum = 0;
    s_ec2_pulse_accum = 0;
    s_ec1_last_step = 0;
    s_ec2_last_step = 0;
}

/*
*********************************************************************************************************
*   函 数 名: nav_set_screen_internal
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void nav_set_screen_internal(UI_ScreenId_t id)
{
    if (id >= UI_SCR_COUNT) {
        return;
    }
    if (s_screen != id && id == UI_SCR_SCOPE) {
        s_scope_enter_pending = 1u;
        s_scope_ui_mode = UI_SCOPE_UI_WAVE;
    }
    s_screen = id;
    if (id == UI_SCR_CHANNEL) {
        UI_Channel_OnScreenShown();
    }
    if (id == UI_SCR_TRIGGER) {
        UI_Trigger_OnScreenShown();
    }
    if (id == UI_SCR_SYSTEM) {
        UI_System_OnScreenShown();
    }
}

/*
*********************************************************************************************************
*   函 数 名: nav_menu_enter_from_focus
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void nav_menu_enter_from_focus(void)
{
    switch (s_menu_focus) {
    case 0:
        nav_set_screen_internal(UI_SCR_SCOPE);
        s_scope_ui_mode = UI_SCOPE_UI_WAVE;
        break;
    case 1:
        s_scope_ui_mode = UI_SCOPE_UI_WAVE;
        nav_set_screen_internal(UI_SCR_CHANNEL);
        break;
    case 2:
        s_scope_ui_mode = UI_SCOPE_UI_WAVE;
        nav_set_screen_internal(UI_SCR_TRIGGER);
        break;
    case 3:
        s_scope_ui_mode = UI_SCOPE_UI_WAVE;
        nav_set_screen_internal(UI_SCR_MEASURE);
        break;
    case 4:
        s_scope_ui_mode = UI_SCOPE_UI_WAVE;
        nav_set_screen_internal(UI_SCR_SYSTEM);
        break;
    default:
        break;
    }
}

/*
*********************************************************************************************************
*   函 数 名: nav_apply_ok
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void nav_apply_ok(void)
{
    if (s_screen == UI_SCR_WELCOME) {
        nav_set_screen_internal(UI_SCR_SCOPE);
        s_scope_ui_mode = UI_SCOPE_UI_WAVE;
        return;
    }
    if (s_screen == UI_SCR_MAIN_MENU) {
        nav_menu_enter_from_focus();
        return;
    }
}

/*
*********************************************************************************************************
*   函 数 名: nav_apply_back
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void nav_apply_back(void)
{
    if (s_screen == UI_SCR_WELCOME) {
        return;
    }
    if (s_screen == UI_SCR_MAIN_MENU) {
        nav_set_screen_internal(UI_SCR_WELCOME);
        return;
    }
    /* 子页面返回示波器（不再经过独立主菜单页） */
    nav_set_screen_internal(UI_SCR_SCOPE);
    s_scope_ui_mode = UI_SCOPE_UI_WAVE;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_SetScreen
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Nav_SetScreen(UI_ScreenId_t id)
{
    nav_set_screen_internal(id);
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_GetScreen
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
UI_ScreenId_t UI_Nav_GetScreen(void)
{
    return (UI_ScreenId_t)s_screen;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_GetScopeUiMode
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
UI_ScopeUiMode_t UI_Nav_GetScopeUiMode(void)
{
    return (UI_ScopeUiMode_t)s_scope_ui_mode;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_GetMenuFocus
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int UI_Nav_GetMenuFocus(void)
{
    if (s_menu_focus < 0) {
        s_menu_focus = 0;
    }
    if (s_menu_focus > 4) {
        s_menu_focus = 4;
    }
    return (int)s_menu_focus;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_GetKnobMode
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int UI_Nav_GetKnobMode(void)
{
    return (int)s_knob_mode;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_GetKnobModeLabel
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const char *UI_Nav_GetKnobModeLabel(void)
{
    return (s_knob_mode == KNOB_MODE_SCALE) ? "Scale" : "Position";
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_MarkDirty
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Nav_MarkDirty(void)
{
    s_redraw_req = 1u;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_ConsumeRedraw
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int UI_Nav_ConsumeRedraw(void)
{
    if (s_redraw_req != 0u) {
        s_redraw_req = 0u;
        return 1;
    }
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_Poll
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Nav_Poll(void)
{
    uint8_t gpb;
    uint8_t ab1, ab2;
    uint8_t ec1_down;
    uint8_t ec2_down;
    int8_t  step1 = 0;
    int8_t  step2 = 0;
    TickType_t now;

    if (s_io_inited == 0u) {
        BSP_UI_IO_Init();
        s_io_inited = 1u;
    }

    if (BSP_UI_IO_ReadPortB(&gpb) != 0) {
        return;
    }

    {
        uint8_t raw_ec1 = (uint8_t)(((gpb & (1u << 0)) == 0u) ? 1u : 0u);
        uint8_t raw_ec2 = (uint8_t)(((gpb & (1u << 3)) == 0u) ? 1u : 0u);

        if (s_ec_btn_db_arm == 0u) {
            s_ec1_btn_filt     = raw_ec1;
            s_ec2_btn_filt     = raw_ec2;
            s_ec1_btn_debounce = 0u;
            s_ec2_btn_debounce = 0u;
            s_ec_btn_db_arm    = 1u;
        } else {
            if (raw_ec1 == s_ec1_btn_filt) {
                s_ec1_btn_debounce = 0u;
            } else if (++s_ec1_btn_debounce >= EC_BTN_DEBOUNCE_MATCHES) {
                s_ec1_btn_filt     = raw_ec1;
                s_ec1_btn_debounce = 0u;
            }
            if (raw_ec2 == s_ec2_btn_filt) {
                s_ec2_btn_debounce = 0u;
            } else if (++s_ec2_btn_debounce >= EC_BTN_DEBOUNCE_MATCHES) {
                s_ec2_btn_filt     = raw_ec2;
                s_ec2_btn_debounce = 0u;
            }
        }
        ec1_down = s_ec1_btn_filt;
        ec2_down = s_ec2_btn_filt;
    }

    /* 编码器：取 A、B 两相 2bit Gray，边沿判向 */
    ab1 = (uint8_t)(((gpb >> (EC1_AB_SHIFT + 1)) & 1u) << 1) | (uint8_t)(((gpb >> EC1_AB_SHIFT) & 1u));
    ab2 = (uint8_t)(((gpb >> (EC2_AB_SHIFT + 1)) & 1u) << 1) | (uint8_t)(((gpb >> EC2_AB_SHIFT) & 1u));

    now = xTaskGetTickCount();

    if (s_scope_enter_pending != 0u) {
        nav_sync_scope_entry(ec1_down, ec2_down, now);
        s_scope_enter_pending = 0u;
    }

    if (s_enc_arm == 0u) {
        s_ec1_prev_ab = ab1;
        s_ec2_prev_ab = ab2;
        s_enc_arm       = 1u;
    } else {
        uint8_t old1 = (uint8_t)(s_ec1_prev_ab & 3u);
        uint8_t old2 = (uint8_t)(s_ec2_prev_ab & 3u);

        step1 = s_quad_tbl[(uint8_t)((old1 << 2) | (ab1 & 3u))];
        step2 = s_quad_tbl[(uint8_t)((old2 << 2) | (ab2 & 3u))];
        /* 采样偏慢时易见 00↔11 / 01↔10 对角跳变，表值为 0；按上一有效方向补双倍步进 */
        if (step1 == 0 && ((old1 ^ ab1) & 3u) == 3u && s_ec1_last_step != 0) {
            step1 = (int8_t)(2 * s_ec1_last_step);
        }
        if (step2 == 0 && ((old2 ^ ab2) & 3u) == 3u && s_ec2_last_step != 0) {
            step2 = (int8_t)(2 * s_ec2_last_step);
        }

        s_ec1_prev_ab = ab1;
        s_ec2_prev_ab = ab2;

        if (step1 != 0) {
            s_ec1_last_step = (step1 > 0) ? (int8_t)1 : (int8_t)-1;
        }
        if (step2 != 0) {
            s_ec2_last_step = (step2 > 0) ? (int8_t)1 : (int8_t)-1;
        }
    }

    /*
     * 波形页组合键轴屏蔽：
     *   只按住 EC2 时转 EC1→CycleSolo，应忽略 EC2 轴误码；
     *   只按住 EC1 时转 EC2→Y-T/Roll，应忽略 EC1 轴误码。
     *   两键同时按住时不屏蔽（EC2 轴→触发模式循环）。
     */
    {
        int8_t s1 = step1;
        int8_t s2 = step2;

        if (s_screen == UI_SCR_SCOPE && s_scope_ui_mode == UI_SCOPE_UI_WAVE) {
            if (ec2_down != 0u && ec1_down == 0u) {
                s2 = 0;
            }
            if (ec1_down != 0u && ec2_down == 0u) {
                s1 = 0;
            }
        }

    /* ────────────────── EC1 旋转 ────────────────── */
    if (s_screen == UI_SCR_MAIN_MENU ||
        (s_screen == UI_SCR_SCOPE && s_scope_ui_mode == UI_SCOPE_UI_MENU)) {
        if (s_enc_arm != 0u && step1 != 0) {
            s_ec1_pulse_accum += (int16_t)step1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (s_menu_focus < 4) {
                    s_menu_focus++;
                    s_redraw_req = 1u;
                }
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (s_menu_focus > 0) {
                    s_menu_focus--;
                    s_redraw_req = 1u;
                }
            }
        }
    } else if (s_screen == UI_SCR_CHANNEL) {
        if (s_enc_arm != 0u && step1 != 0) {
            s_ec1_pulse_accum += (int16_t)step1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Channel_Ec1Detent(1);
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Channel_Ec1Detent(-1);
            }
        }
    } else if (s_screen == UI_SCR_TRIGGER) {
        if (s_enc_arm != 0u && step1 != 0) {
            s_ec1_pulse_accum += (int16_t)step1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Trigger_Ec1Detent(1);
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Trigger_Ec1Detent(-1);
            }
        }
    } else if (s_screen == UI_SCR_SYSTEM) {
        if (s_enc_arm != 0u && step1 != 0) {
            s_ec1_pulse_accum += (int16_t)step1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_System_Ec1Detent(1);
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_System_Ec1Detent(-1);
            }
        }
    } else if (s_screen == UI_SCR_SCOPE && s_scope_ui_mode == UI_SCOPE_UI_WAVE) {
        /* EC1 旋转：按住EC2→CycleSolo；否则按模式：档位→ms/div，位置→CH1位移 */
        if (s_enc_arm != 0u && s1 != 0) {
            s_ec1_pulse_accum += (int16_t)s1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec2_down != 0u) {
                    UI_Waveform_CycleSolo();
                } else if (s_knob_mode == KNOB_MODE_SCALE) {
                    UI_Waveform_StepTimePerDiv(1);
                } else {
                    UI_Waveform_CH1_MoveDown();
                }
                s_redraw_req = 1u;
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec2_down != 0u) {
                    UI_Waveform_CycleSolo();
                } else if (s_knob_mode == KNOB_MODE_SCALE) {
                    UI_Waveform_StepTimePerDiv(-1);
                } else {
                    UI_Waveform_CH1_MoveUp();
                }
                s_redraw_req = 1u;
            }
        }
    } else {
        s_ec1_pulse_accum = 0;
        s_ec1_last_step   = 0;
    }

    /* ────────────────── EC2 旋转 ────────────────── */
    if (s_screen == UI_SCR_SCOPE && s_scope_ui_mode == UI_SCOPE_UI_WAVE) {
        /* EC2 旋转：EC1+EC2同按→触发模式；按住EC1→Y-T/Roll；否则按模式：档位→V/div，位置→CH2位移 */
        if (s_enc_arm != 0u && s2 != 0) {
            s_ec2_pulse_accum += (int16_t)s2;
            while (s_ec2_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec1_down != 0u && ec2_down != 0u) {
                    UI_Waveform_CycleTrigMode();
                } else if (ec1_down != 0u) {
                    UI_Waveform_ToggleDisplayMode();
                } else if (s_knob_mode == KNOB_MODE_SCALE) {
                    UI_Waveform_StepVoltPerDiv(1);
                } else {
                    UI_Waveform_CH2_MoveDown();
                }
                s_redraw_req = 1u;
            }
            while (s_ec2_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec1_down != 0u && ec2_down != 0u) {
                    UI_Waveform_CycleTrigMode();
                } else if (ec1_down != 0u) {
                    UI_Waveform_ToggleDisplayMode();
                } else if (s_knob_mode == KNOB_MODE_SCALE) {
                    UI_Waveform_StepVoltPerDiv(-1);
                } else {
                    UI_Waveform_CH2_MoveUp();
                }
                s_redraw_req = 1u;
            }
        }
    } else if (s_screen == UI_SCR_CHANNEL) {
        if (s_enc_arm != 0u && s2 != 0) {
            s_ec2_pulse_accum += (int16_t)s2;
            while (s_ec2_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Channel_Ec2Detent(1);
            }
            while (s_ec2_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Channel_Ec2Detent(-1);
            }
        }
    } else if (s_screen == UI_SCR_TRIGGER) {
        if (s_enc_arm != 0u && s2 != 0) {
            s_ec2_pulse_accum += (int16_t)s2;
            while (s_ec2_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Trigger_Ec2Detent(1);
            }
            while (s_ec2_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_Trigger_Ec2Detent(-1);
            }
        }
    } else if (s_screen == UI_SCR_SYSTEM) {
        if (s_enc_arm != 0u && s2 != 0) {
            s_ec2_pulse_accum += (int16_t)s2;
            while (s_ec2_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_System_Ec2Detent(1);
            }
            while (s_ec2_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                UI_System_Ec2Detent(-1);
            }
        }
    } else {
        s_ec2_pulse_accum = 0;
        s_ec2_last_step   = 0;
    }

    } /* scope step mask */

    /* ────────────────── 波形页按键组合判定 ────────────────── */
    if (s_screen == UI_SCR_SCOPE) {
        if (ec1_down != 0u && s_ec1_btn_prev == 0u) {
            s_ec1_scope_press_tick = now;
            s_ec1_scope_combo_lock = 0u;
        }
        if (ec2_down != 0u && s_ec2_btn_prev == 0u) {
            s_ec2_scope_press_tick = now;
            s_ec2_scope_combo_lock = 0u;
        }
        if (ec1_down != 0u && step2 != 0) {
            s_ec1_scope_combo_lock = 1u;
        }
        if (ec2_down != 0u && step1 != 0) {
            s_ec2_scope_combo_lock = 1u;
        }
    }

    /* ────────────────── EC1 按键 ────────────────── */
    /* 波形页：WAVE 短按=RUN/STOP，MENU 短按=确认；长按=进入菜单/确认；其它页=确认 */
    {
        if (s_screen == UI_SCR_SCOPE) {
            if (ec1_down == 0u && s_ec1_btn_prev != 0u) {
                if (s_ec1_scope_suppress_release != 0u) {
                    s_ec1_scope_suppress_release = 0u;
                } else {
                    TickType_t dur = now - s_ec1_scope_press_tick;
                    if (dur >= pdMS_TO_TICKS(EC_SCOPE_LONG_MS)) {
                        if (s_ec1_scope_combo_lock == 0u) {
                            if (s_scope_ui_mode == UI_SCOPE_UI_WAVE) {
                                s_scope_ui_mode = UI_SCOPE_UI_MENU;
                            } else {
                                nav_menu_enter_from_focus();
                            }
                            s_redraw_req = 1u;
                        }
                    } else if (dur >= pdMS_TO_TICKS(20u)) {
                        if ((now - s_ec1_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                            if (s_scope_ui_mode == UI_SCOPE_UI_MENU) {
                                nav_menu_enter_from_focus();
                            } else {
                                UI_Waveform_ToggleRunStop();
                            }
                            s_redraw_req  = 1u;
                            s_ec1_last_ms = now;
                        }
                    }
                }
            }
        } else if (ec1_down != 0u && s_ec1_btn_prev == 0u) {
            if ((now - s_ec1_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                nav_apply_ok();
                s_redraw_req   = 1u;
                s_ec1_last_ms = now;
            }
        }
        s_ec1_btn_prev = ec1_down;
    }

    /* ────────────────── EC2 按键 ────────────────── */
    /* 波形页：短按=返回；其它页下降沿=返回 */
    {
        if (s_screen == UI_SCR_SCOPE) {
            if (ec2_down == 0u && s_ec2_btn_prev != 0u) {
                if (s_ec2_scope_suppress_release != 0u) {
                    s_ec2_scope_suppress_release = 0u;
                } else {
                    TickType_t dur = now - s_ec2_scope_press_tick;
                    if (dur >= pdMS_TO_TICKS(EC_SCOPE_LONG_MS)) {
                        if (s_ec2_scope_combo_lock == 0u) {
                            s_knob_mode = (s_knob_mode == KNOB_MODE_SCALE)
                                        ? KNOB_MODE_POSITION : KNOB_MODE_SCALE;
                            s_redraw_req   = 1u;
                        }
                    } else if (dur >= pdMS_TO_TICKS(20u)) {
                        if ((now - s_ec2_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                            if (s_scope_ui_mode == UI_SCOPE_UI_MENU) {
                                s_scope_ui_mode = UI_SCOPE_UI_WAVE;
                            } else {
                                nav_set_screen_internal(UI_SCR_WELCOME);
                            }
                            s_redraw_req   = 1u;
                            s_ec2_last_ms = now;
                        }
                    }
                }
            }
        } else if (ec2_down != 0u && s_ec2_btn_prev == 0u) {
            if ((now - s_ec2_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                nav_apply_back();
                s_redraw_req   = 1u;
                s_ec2_last_ms = now;
            }
        }
        s_ec2_btn_prev = ec2_down;
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_Nav_OnKey
*   功能说明: 维护 UI 页面导航、菜单焦点、旋钮模式和按键确认/返回行为。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_Nav_OnKey(UI_NavKey_t key)
{
    s_redraw_req = 1u;

    switch (key) {
    case UI_NAV_KEY_UP:
        if (s_menu_focus > 0 &&
            (s_screen == UI_SCR_MAIN_MENU ||
             (s_screen == UI_SCR_SCOPE && s_scope_ui_mode == UI_SCOPE_UI_MENU))) {
            s_menu_focus--;
        }
        break;
    case UI_NAV_KEY_DOWN:
        if (s_menu_focus < 4 &&
            (s_screen == UI_SCR_MAIN_MENU ||
             (s_screen == UI_SCR_SCOPE && s_scope_ui_mode == UI_SCOPE_UI_MENU))) {
            s_menu_focus++;
        }
        break;
    case UI_NAV_KEY_OK:
        nav_apply_ok();
        break;
    case UI_NAV_KEY_BACK:
        nav_apply_back();
        break;
    default:
        break;
    }
}
