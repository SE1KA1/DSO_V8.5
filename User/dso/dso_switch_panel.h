/**
 * @file dso_switch_panel.h
 * @brief 云桥拨码开关 + LED 状态模型（写透到 PCAL6524HEAZ I2C 0x22 真实硬件）。
 *
 * 开关经 4×SN74LVC257 多路复用器控制：
 *   B1-B8 → P0_0..P0_7；B9→P1_0；B11/B12/B14→P1_1..P1_3；B13→P1_4；
 *   B10 悬空不可控（模型 bit9 恒 0）。ON=低电平，OFF=高电平。
 * LED 为内存状态（后续接真实 GPIO）。
 */
#ifndef DSO_SWITCH_PANEL_H
#define DSO_SWITCH_PANEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SW_DIP10_COUNT  10
#define SW_DIP4_COUNT   4
#define SW_TOTAL        (SW_DIP10_COUNT + SW_DIP4_COUNT)
#define SW_LED_COUNT    3

/** 初始化开关/LED 内存状态。 */
void DSO_Switch_Init(void);

/** 读第 idx 个开关 (0..13): 0=OFF 1=ON。idx 0-9 为 10 位组，10-13 为 4 位组。 */
uint8_t DSO_Switch_Get(int idx);

/** 翻转第 idx 个开关。 */
void DSO_Switch_Toggle(int idx);

/** 确定性置位：on=1 置 ON，on=0 置 OFF。 */
void DSO_Switch_Set(int idx, uint8_t on);

/** 14 位开关 bitmap：bit0=SW1 ... bit13=SW14。 */
uint16_t DSO_Switch_GetBitmap(void);

/** 3 位 LED bitmap：bit0=LED1(心跳) bit1=LED2(电源) bit2=LED3(报警)。 */
uint8_t DSO_Switch_GetLedBitmap(void);

/** 读 LED (0..2): 0=灭 1=亮。LED1=心跳 LED2=电源 LED3=报警。 */
uint8_t DSO_Switch_GetLed(int led);

/** 设置 LED 状态。 */
void DSO_Switch_SetLed(int led, uint8_t on);

/** 返回 idx 属于哪一组: 1=10位组, 0=4位组。 */
int DSO_Switch_Is10Bit(int idx);

#ifdef __cplusplus
}
#endif
#endif /* DSO_SWITCH_PANEL_H */
