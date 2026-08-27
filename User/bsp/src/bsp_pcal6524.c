/**
 * @file bsp_pcal6524.c
 * @brief PCAL6524HEAZ 24-bit I2C IO 扩展器驱动。
 *
 * 面板开关经 4×SN74LVC257 多路复用器（U4-U7）由本器件控制：
 *   P0_0..P0_7 → U4/U5 的 1B-4B（B1-B8）
 *   P1_0..P1_3 → U6 的 1B-4B（B9, B11, B12, B14）
 *   P1_4      → U7 的 1B（B13）
 * 开关 ON=低电平、OFF=高电平(3.3V)。
 *
 * 寄存器（PCAL 家族标准布局，按 PCAL6524 数据手册）：
 *   Output: 0x04(P0) / 0x05(P1)    Config(方向): 0x0C(P0) / 0x0D(P1)
 * 上电默认全部为输入（带弱上拉），故先写输出值再切方向，避免毛刺。
 */
#include "bsp_pcal6524.h"
#include "bsp_i2c_ui.h"

/* 宏定义: PCAL6524_REG_OUT0 - P0 输出寄存器。 */
#define PCAL6524_REG_OUT0  0x04u
/* 宏定义: PCAL6524_REG_OUT1 - P1 输出寄存器。 */
#define PCAL6524_REG_OUT1  0x05u
/* 宏定义: PCAL6524_REG_CFG0 - P0 方向寄存器（0=输出）。 */
#define PCAL6524_REG_CFG0  0x0Cu
/* 宏定义: PCAL6524_REG_CFG1 - P1 方向寄存器（0=输出）。 */
#define PCAL6524_REG_CFG1  0x0Du
/* 宏定义: PCAL6524_REG_ODC - 输出端口配置（0=推挽，1=开漏；三组端口全局）。 */
#define PCAL6524_REG_ODC   0x5Cu

int BSP_PCAL6524_Init(void)
{
    int rc;

    BSP_I2C_UI_Init();

    BSP_I2C_UI_Lock();
    /* 先写输出（全 OFF=1）再设方向为输出，避免上电毛刺 */
    rc  = BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, PCAL6524_REG_OUT0, 0xFFu);
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, PCAL6524_REG_OUT1, 0xFFu);
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, PCAL6524_REG_CFG0, 0x00u);
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, PCAL6524_REG_CFG1, 0x00u);
    /* 输出端口配置 0x5C = 0x00 → 三组端口全部推挽驱动（0=推挽，1=开漏） */
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, PCAL6524_REG_ODC, 0x00u);
    /* 驱动强度 0x40-0x43 = 0xFF → Port 0/1 驱动强度最大（1x），避免弱驱动拉不动 mux 负载 */
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, 0x40u, 0xFFu);
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, 0x41u, 0xFFu);
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, 0x42u, 0xFFu);
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, 0x43u, 0xFFu);
    BSP_I2C_UI_Unlock();

    return rc;
}

int BSP_PCAL6524_WriteP0P1(uint16_t bits)
{
    int rc;

    BSP_I2C_UI_Lock();
    rc  = BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, PCAL6524_REG_OUT0,
                              (uint8_t)(bits & 0xFFu));
    rc |= BSP_I2C_UI_WriteReg(PCAL6524_I2C_ADDR7, PCAL6524_REG_OUT1,
                              (uint8_t)((bits >> 8) & 0xFFu));
    BSP_I2C_UI_Unlock();

    return rc;
}
