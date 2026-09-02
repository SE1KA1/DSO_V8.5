/**
 * @file bsp_i2c_ui.c
 * @brief 面板软件 I2C 总线（PB8=SCL, PB7=SDA）。
 *
 * 原语从 bsp_ui_io_expander.c 原样搬移（GPIO 模拟 I2C，开漏 + 外部上拉），
 * 供 MCP23017(0x26) 与 PCAL6524(0x20) 共享；FreeRTOS 互斥锁防两任务并发交错。
 */
#include "bsp_i2c_ui.h"
#include "bsp.h"          /* bsp_DelayUS, HAL_GPIO, __HAL_RCC */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define I2C_UI_GPIO     GPIOB
#define I2C_UI_SCL_PIN  GPIO_PIN_8
#define I2C_UI_SDA_PIN  GPIO_PIN_7

#define I2C_UI_WR   0u
#define I2C_UI_RD   1u

/* STM32H7 GPIO_TypeDef 为 BSRRL/BSRRH 分寄存器 */
#define UI_SCL_1()   (I2C_UI_GPIO->BSRRL = (uint16_t)I2C_UI_SCL_PIN)
#define UI_SCL_0()   (I2C_UI_GPIO->BSRRH = (uint16_t)I2C_UI_SCL_PIN)
#define UI_SDA_1()   (I2C_UI_GPIO->BSRRL = (uint16_t)I2C_UI_SDA_PIN)
#define UI_SDA_0()   (I2C_UI_GPIO->BSRRH = (uint16_t)I2C_UI_SDA_PIN)
#define UI_SDA_READ()  (((I2C_UI_GPIO->IDR) & I2C_UI_SDA_PIN) != 0U)

static SemaphoreHandle_t s_i2c_ui_mtx;

static void i2c_delay(void)
{
    bsp_DelayUS(2);
}

static void i2c_stop(void)
{
    UI_SDA_0();
    i2c_delay();
    UI_SCL_1();
    i2c_delay();
    UI_SDA_1();
    i2c_delay();
}

static void i2c_start(void)
{
    UI_SDA_1();
    UI_SCL_1();
    i2c_delay();
    UI_SDA_0();
    i2c_delay();
    UI_SCL_0();
    i2c_delay();
}

static void i2c_send_byte(uint8_t b)
{
    uint8_t i;
    for (i = 0U; i < 8U; i++) {
        if ((b & 0x80U) != 0U) {
            UI_SDA_1();
        } else {
            UI_SDA_0();
        }
        i2c_delay();
        UI_SCL_1();
        i2c_delay();
        UI_SCL_0();
        if (i == 7U) {
            UI_SDA_1();   /* 释放 SDA 准备收 ACK */
        }
        b <<= 1;
    }
}

static uint8_t i2c_read_byte(void)
{
    uint8_t i;
    uint8_t v = 0U;
    for (i = 0U; i < 8U; i++) {
        v <<= 1U;
        UI_SCL_1();
        i2c_delay();
        if (UI_SDA_READ()) {
            v++;
        }
        UI_SCL_0();
        i2c_delay();
    }
    return v;
}

static uint8_t i2c_wait_ack(void)
{
    uint8_t re;
    UI_SDA_1();
    i2c_delay();
    UI_SCL_1();
    i2c_delay();
    re = UI_SDA_READ() ? 1U : 0U;
    UI_SCL_0();
    i2c_delay();
    return re;
}

static void i2c_nack(void)
{
    UI_SDA_1();
    i2c_delay();
    UI_SCL_1();
    i2c_delay();
    UI_SCL_0();
    i2c_delay();
}

/* 主设备收完一字节后拉低 SDA 在第 9 个时钟发 ACK（多字节读的前 len-1 字节） */
static void i2c_master_ack(void)
{
    UI_SDA_0();
    i2c_delay();
    UI_SCL_1();
    i2c_delay();
    UI_SCL_0();
    i2c_delay();
    UI_SDA_1();
    i2c_delay();
}

/* ---- 总线初始化与互斥 ---- */

void BSP_I2C_UI_Init(void)
{
    GPIO_InitTypeDef gpio;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode      = GPIO_MODE_OUTPUT_OD;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Pin       = I2C_UI_SCL_PIN | I2C_UI_SDA_PIN;
    gpio.Alternate = 0U;
    HAL_GPIO_Init(I2C_UI_GPIO, &gpio);

    i2c_stop();

    if (s_i2c_ui_mtx == NULL) {
        s_i2c_ui_mtx = xSemaphoreCreateMutex();
    }
}

void BSP_I2C_UI_Lock(void)
{
    if (s_i2c_ui_mtx != NULL &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreTake(s_i2c_ui_mtx, portMAX_DELAY);
    }
}

void BSP_I2C_UI_Unlock(void)
{
    if (s_i2c_ui_mtx != NULL &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreGive(s_i2c_ui_mtx);
    }
}

/* ---- 寄存器访问 ---- */

int BSP_I2C_UI_WriteReg(uint8_t addr7, uint8_t reg, uint8_t val)
{
    int rc = 0;

    i2c_start();
    i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_WR));
    if (i2c_wait_ack() != 0U) {
        rc = -1;
    } else {
        i2c_send_byte(reg);
        if (i2c_wait_ack() != 0U) {
            rc = -1;
        } else {
            i2c_send_byte(val);
            if (i2c_wait_ack() != 0U) {
                rc = -1;
            }
        }
    }
    i2c_stop();
    return rc;
}

int BSP_I2C_UI_Probe(uint8_t addr7)
{
    int rc = 0;

    i2c_start();
    i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_WR));
    if (i2c_wait_ack() != 0U) {
        rc = -1;
    }
    i2c_stop();
    return rc;
}

int BSP_I2C_UI_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *val)
{
    int rc = 0;

    if (val == NULL) {
        return -1;
    }

    i2c_start();
    i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_WR));
    if (i2c_wait_ack() != 0U) {
        rc = -1;
    } else {
        i2c_send_byte(reg);
        if (i2c_wait_ack() != 0U) {
            rc = -1;
        } else {
            i2c_start();   /* restart */
            i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_RD));
            if (i2c_wait_ack() != 0U) {
                rc = -1;
            } else {
                *val = i2c_read_byte();
                i2c_nack();
            }
        }
    }
    i2c_stop();
    return rc;
}

/* ---- 原始访问（无寄存器地址的器件，如 SHT40 命令/读取） ---- */

int BSP_I2C_UI_WriteRaw(uint8_t addr7, const uint8_t *buf, uint8_t len)
{
    int rc = 0;
    uint8_t i;

    if (buf == NULL || len == 0u) {
        return -1;
    }

    i2c_start();
    i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_WR));
    if (i2c_wait_ack() != 0U) {
        rc = -1;
    } else {
        for (i = 0U; i < len; i++) {
            i2c_send_byte(buf[i]);
            if (i2c_wait_ack() != 0U) {
                rc = -1;
                break;
            }
        }
    }
    i2c_stop();
    return rc;
}

int BSP_I2C_UI_ReadRaw(uint8_t addr7, uint8_t *buf, uint8_t len)
{
    int rc = 0;
    uint8_t i;

    if (buf == NULL || len == 0u) {
        return -1;
    }

    i2c_start();
    i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_RD));
    if (i2c_wait_ack() != 0U) {
        rc = -1;
    } else {
        for (i = 0U; i < len; i++) {
            buf[i] = i2c_read_byte();
            if (i == (uint8_t)(len - 1u)) {
                i2c_nack();      /* 最后一字节 NACK，通知从机停止 */
            } else {
                i2c_master_ack();  /* 继续 */
            }
        }
    }
    i2c_stop();
    return rc;
}
