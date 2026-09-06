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
static volatile uint32_t s_bit_delay_us = 0u;   /* 0=默认 2µs；诊断可临时调慢 */

static void i2c_delay(void)
{
    bsp_DelayUS((s_bit_delay_us != 0u) ? s_bit_delay_us : 2u);
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

/* 诊断：设置总线位延迟 µs（0=恢复默认 2µs ≈ 200kHz）。慢速(如 30µs ≈ 10kHz)
 * 用于排除"主控采样过早把器件慢 ACK 误判 NACK"。影响全总线，用完必须设回 0。 */
void BSP_I2C_UI_SetBitDelayUs(uint32_t us)
{
    s_bit_delay_us = us;
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

/* ---- 唤醒脉冲（ATECC608B 等：睡眠中地址 NACK，需 SDA 拉低唤醒） ---- */

void BSP_I2C_UI_WakePulseUs(uint32_t low_us)
{
    /* SDA 拉低 low_us µs（开漏驱动低）→ 释放由上拉复位；SCL 不动。
     * tWLO≥60µs；随后 ~2ms（tWHI≥1.5ms），自检使能时由调用方再加 20ms 余量。 */
    if (low_us < 60u) {
        low_us = 60u;
    }
    if (low_us > 20000u) {
        low_us = 20000u;
    }
    UI_SDA_0();
    bsp_DelayUS(low_us);
    UI_SDA_1();
    bsp_DelayUS(2000);
}

void BSP_I2C_UI_WakePulse(void)
{
    BSP_I2C_UI_WakePulseUs(100u);
}

/* 诊断：持续拉低 SDA 约 ms 毫秒（阻塞）。用于万用表实测芯片 SDA 脚电平。 */
void BSP_I2C_UI_SdaLowMs(uint32_t ms)
{
    uint32_t i;
    UI_SDA_0();
    for (i = 0u; i < ms; i++) {
        bsp_DelayUS(1000);
    }
    UI_SDA_1();
}

/* 自检：驱动 SDA 低并回读 IDR。@return 1=确实读到低（引脚在驱动）；0=读仍高（配置/接线问题） */
int BSP_I2C_UI_SdaLowProbe(void)
{
    int v;
    UI_SDA_0();
    bsp_DelayUS(120);
    v = (UI_SDA_READ() != 0u) ? 0 : 1;
    UI_SDA_1();
    bsp_DelayUS(10);
    return v;
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

int BSP_I2C_UI_DiagTxBytes(uint8_t addr7, const uint8_t *buf, uint8_t len,
                           uint8_t *ack_log)
{
    int rc = 0;
    uint8_t i;

    if (buf == NULL || len == 0u || ack_log == NULL) {
        return -1;
    }

    i2c_start();
    i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_WR));
    if (i2c_wait_ack() != 0U) {
        rc = -1;
    }
    ack_log[0] = (rc == 0) ? 1u : 0u;
    /* 无论地址是否 ACK 都继续发（NACK 后总线仍可继续，诊断不中断） */
    for (i = 0u; i < len; i++) {
        i2c_send_byte(buf[i]);
        ack_log[1u + i] = (i2c_wait_ack() == 0U) ? 1u : 0u;
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
