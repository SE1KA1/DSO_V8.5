/**
 * @file dso_switch_panel.c
 * @brief 云桥拨码开关 + LED 状态模型（写透到 PCAL6524 真实硬件）。
 *
 * 拨码开关经 4×SN74LVC257 多路复用器（U4-U7）由 PCAL6524HEAZ (I2C 0x22) 控制：
 *   B1-B8 → P0_0..P0_7（U4/U5 1B-4B）
 *   B9    → P1_0（U6 1B）
 *   B10   悬空（不可控，bit9 恒 0）
 *   B11,B12,B14 → P1_1,P1_2,P1_3（U6 2B-4B）
 *   B13   → P1_4（U7 1B）
 * 开关 ON=低电平、OFF=高电平(3.3V)。mux 输出使能 G# 接 PH8（低有效，置 LOW 使能）。
 * LED 3 个：LED1=STM32心跳, LED2=温度报警(黄), LED3=电流报警(红)。
 *
 * 模型位图沿用 14 位（bit0=B1 … bit13=B14），bit9(B10) 恒 0；协议不变。
 */
#include "dso_switch_panel.h"
#include "bsp.h"
#include "bsp_i2c_ui.h"
#include "bsp_pcal6524.h"
#include <stdio.h>

/* 14 位开关 bitmap：bit0=B1 ... bit13=B14（bit9=B10 保留恒 0） */
static uint16_t s_sw_bitmap = 0;

/* 3 个 LED：0=灭 1=亮 */
static uint8_t s_led[SW_LED_COUNT] = {0, 0, 0};

/* B10（idx 9）悬空不可控 */
#define SW_IDX_UNUSED  9

/* 由 14 位开关 bitmap 生成 PCAL6524 P0P1 输出（开关 ON=低电平 → 取反） */
static uint16_t switch_to_p0p1(uint16_t sw)
{
    uint16_t v = 0;

    v  = (uint16_t)(sw & 0x00FFu);                     /* B1-B8 → P0.0-7 */
    v |= (uint16_t)(((sw >> 8)  & 1u) << 8);           /* B9  → P1.0 */
    v |= (uint16_t)(((sw >> 10) & 1u) << 9);           /* B11 → P1.1 */
    v |= (uint16_t)(((sw >> 11) & 1u) << 10);          /* B12 → P1.2 */
    v |= (uint16_t)(((sw >> 13) & 1u) << 11);          /* B14 → P1.3 */
    v |= (uint16_t)(((sw >> 12) & 1u) << 12);          /* B13 → P1.4 */

    return (uint16_t)~v;                               /* ON=低电平 */
}

/* 使能 SN74LVC257 输出。万用表实测 STM32→过桥板 PH7/PH8 硬件对调：
 *   PH7 → U4-U7 的 G#（低有效使能，输出 LOW）
 *   PH8 → PCAL6524 的 INT#（空闲高，输入上拉不与其开漏输出争抢）
 * 故每次写硬件前都重新断言 PH7=LOW。 */
static void switch_phy_enable(void)
{
    GPIO_InitTypeDef g;

    __HAL_RCC_GPIOH_CLK_ENABLE();
    /* PH7 = mux G#：GPIO 输出 LOW 使能 */
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Pin       = GPIO_PIN_7;
    g.Alternate = 0U;
    HAL_GPIO_Init(GPIOH, &g);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_RESET);  /* PH7=LOW: mux 使能 */

    /* PH8 = PCAL6524 INT#：输入上拉（空闲高） */
    g.Pin       = GPIO_PIN_8;
    g.Mode      = GPIO_MODE_INPUT;
    g.Pull      = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOH, &g);
}

static void switch_write_hw(void)
{
    switch_phy_enable();
    (void)BSP_PCAL6524_WriteP0P1(switch_to_p0p1(s_sw_bitmap));
}

void DSO_Switch_Init(void)
{
    int rc;

    s_sw_bitmap = 0;
    s_led[0] = 1;  /* 心跳默认亮 */
    s_led[1] = 0;  /* 温度报警默认灭（应答时按 SHT40 现算） */
    s_led[2] = 0;  /* 电流报警默认灭（应答时按 INA226 现算） */

    switch_phy_enable();

    /* 初始化 PCAL6524 并写初始开关态（全 OFF） */
    rc = BSP_PCAL6524_Init();
    printf("[SW] pcal init=%d\r\n", rc);   /* 诊断：0=PCAL6524@0x22 应答正常 */

    /* 诊断：扫描 PB7/PB8 总线 0x20-0x27，定位 PCAL6524 地址并确认 MCP23017(0x26) 存活 */
//    {
//        uint8_t a;
//        for (a = 0x20u; a <= 0x27u; a++) {
//            printf("[I2C] probe 0x%02X -> %d\r\n", a, BSP_I2C_UI_Probe(a));
//        }
//    }

    /* 诊断：B1=OFF（P0_0 输出高），其余开关 ON（低）——验证 PCAL6524→mux 走线（定位后移除） */
    {
        uint16_t test_bits = 0;
        uint8_t i;
//        for (i = 0; i < (uint8_t)SW_TOTAL; i++) {
//            if (i == SW_IDX_UNUSED) continue;   /* B10 悬空 */
////            if (i == 1) continue;               /* B1 OFF → P0_0 输出高 */
//            test_bits |= (uint16_t)(1u << i);
//        }
        s_sw_bitmap = test_bits;
        switch_write_hw();
        printf("[SW] TEST B1=OFF p0p1=0x%04X\r\n",
               (unsigned)switch_to_p0p1(test_bits));

        /* 诊断：读回 PCAL6524 寄存器（打印返回码；0=成功）。末尾重读 OUT0 验证总线是否存活 */
//        {
//            uint8_t rb;
//            int r;
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x04u, &rb); printf("[SW] rd OUT0=0x%02X rc=%d\r\n", rb, r);
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x05u, &rb); printf("[SW] rd OUT1=0x%02X rc=%d\r\n", rb, r);
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x0Cu, &rb); printf("[SW] rd CFG0=0x%02X rc=%d\r\n", rb, r);
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x0Du, &rb); printf("[SW] rd CFG1=0x%02X rc=%d\r\n", rb, r);
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x00u, &rb); printf("[SW] rd IN0 =0x%02X rc=%d\r\n", rb, r);   /* 引脚实际电平 */
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x01u, &rb); printf("[SW] rd IN1 =0x%02X rc=%d\r\n", rb, r);
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x5Cu, &rb); printf("[SW] rd ODC =0x%02X rc=%d\r\n", rb, r);   /* 输出端口配置 0x5C：0=推挽 */
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x40u, &rb); printf("[SW] rd DRV0=0x%02X rc=%d\r\n", rb, r);   /* 驱动强度 0x40 */
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x41u, &rb); printf("[SW] rd DRV1=0x%02X rc=%d\r\n", rb, r);   /* 驱动强度 0x41 */
//            r = BSP_I2C_UI_ReadReg(PCAL6524_I2C_ADDR7, 0x04u, &rb); printf("[SW] rd OUT0bis=0x%02X rc=%d\r\n", rb, r); /* 总线存活检查 */
//        }
        printf("[SW] PH7 IDR=%d PH8 IDR=%d\r\n",
               (int)((GPIOH->IDR >> 7) & 1u), (int)((GPIOH->IDR >> 8) & 1u));
    }
}

uint8_t DSO_Switch_Get(int idx)
{
    if (idx < 0 || idx >= SW_TOTAL) return 0;
    return (s_sw_bitmap >> idx) & 1u;
}

void DSO_Switch_Toggle(int idx)
{
    if (idx < 0 || idx >= SW_TOTAL) return;
    if (idx == SW_IDX_UNUSED) return;   /* B10 悬空不可控 */
    s_sw_bitmap ^= (uint16_t)(1u << idx);
    switch_write_hw();
}

void DSO_Switch_Set(int idx, uint8_t on)
{
    if (idx < 0 || idx >= SW_TOTAL) return;
    if (idx == SW_IDX_UNUSED) return;   /* B10 悬空不可控 */
    if (on) {
        s_sw_bitmap |= (uint16_t)(1u << idx);
    } else {
        s_sw_bitmap &= (uint16_t)~(1u << idx);
    }
    switch_write_hw();
}

uint16_t DSO_Switch_GetBitmap(void)
{
    return s_sw_bitmap;
}

uint8_t DSO_Switch_GetLedBitmap(void)
{
    uint8_t b = 0;
    uint8_t i;
    for (i = 0; i < SW_LED_COUNT; i++) {
        if (s_led[i]) b |= (uint8_t)(1u << i);
    }
    return b;
}

uint8_t DSO_Switch_GetLed(int led)
{
    if (led < 0 || led >= SW_LED_COUNT) return 0;
    return s_led[led];
}

void DSO_Switch_SetLed(int led, uint8_t on)
{
    if (led < 0 || led >= SW_LED_COUNT) return;
    s_led[led] = on ? 1u : 0u;
}

int DSO_Switch_Is10Bit(int idx)
{
    return (idx >= 0 && idx < SW_DIP10_COUNT) ? 1 : 0;
}
