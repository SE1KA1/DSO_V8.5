/**
 * @file dso_ina226.c
 * @brief INA226AIDGSR 电流监测（软件 I2C BSP_I2C_UI，PB7/PB8，与 PCAL6524/SHT40 同总线）。
 *
 * 配置（首次成功后不再重复写）：
 *   Config 0x0527   AVG=16、VBUSCT/VSHCT=1.1ms、连续 shunt+bus；AlertPol=0(开漏低有效)
 *   Alert Limit     0x0258（600 LSB = 1.5mV ≈ 15mA）
 *   Mask/Enable     0x8000（SOL=1：shunt 过压告警源，锁存）
 * 过流时 ALERT 引脚拉低并锁存；每周期读 Mask/Enable 清锁存使能重新武装。
 *
 * 判警（迟滞）：shunt LSB > 600 → 过流(power_ok=0)；降到 ≤450 才恢复(1)。
 * 总线互斥持锁完成 shunt+M/E 两次读；读失败保留上次判定（默认视为正常）。
 */
#include "dso_ina226.h"
#include "bsp_i2c_ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* INA226 分流寄存器 1 LSB = 2.5µV；Rshunt=100mΩ → cur[mA] = lsb * 0.025 = lsb/40 */
#define INA226_UV_PER_LSB    5     /* 2.5µV，为避免小数 ×2 后 /2：lsb*5/2 µV */
#define INA226_SHUNT_MOHM    DSO_INA226_SHUNT_MOHM

static volatile int32_t s_shunt_lsb = 0;   /* 最近分流原始值（有符号 LSB） */
static volatile uint8_t s_power_ok = 1;    /* 默认正常（LED2 亮） */
static volatile uint8_t s_have_data = 0;   /* 曾读到过有效样本 */
static uint8_t s_configured = 0;
static uint16_t s_fail_streak = 0;
static uint8_t s_normal_count = 0;         /* 连续正常周期（软件锁存恢复计数） */
static uint8_t s_reported_first_ok = 0;
static uint8_t s_reported_over = 0;        /* 状态翻转诊断防刷屏 */

/* ---- 16 位寄存器读写（复用 raw 原语；锁由调用方管理） ---- */

static int ina226_wreg16(uint8_t reg, uint16_t val)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val & 0xFFu);
    return BSP_I2C_UI_WriteRaw(DSO_INA226_I2C_ADDR, buf, 3u);
}

static int ina226_rreg16(uint8_t reg, uint16_t *val)
{
    uint8_t ptr = reg;
    uint8_t d[2];
    if (val == NULL) return -1;
    if (BSP_I2C_UI_WriteRaw(DSO_INA226_I2C_ADDR, &ptr, 1u) != 0) return -1;
    if (BSP_I2C_UI_ReadRaw(DSO_INA226_I2C_ADDR, d, 2u) != 0) return -1;
    *val = (uint16_t)((d[0] << 8) | d[1]);
    return 0;
}

static int ina226_configure(void)
{
    if (ina226_wreg16(DSO_INA226_REG_CONFIG, 0x0527u) != 0) return -1;
    if (ina226_wreg16(DSO_INA226_REG_ALERT_LIMIT, DSO_INA226_ALERT_LSB) != 0) return -1;
    /* M/E: SOL(bit15) 使能分流过压告警 + LEN(bit0) 锁存(ALERT 保持低直到读 M/E)。
     * APOL(bit1)=0 → 开漏低有效。注意：AFF 标志在 bit4(0x0010)，不是 bit9！ */
    if (ina226_wreg16(DSO_INA226_REG_MASK_EN, 0x8001u) != 0) return -1;
    return 0;
}

static void ina226_measure_once(void)
{
    uint16_t me = 0;
    uint16_t shunt_raw = 0;
    int rc;
    int32_t lsb;
    uint8_t aff;
    uint8_t alarm;

    if (!s_configured) {
        BSP_I2C_UI_Lock();
        rc = ina226_configure();
        BSP_I2C_UI_Unlock();
        if (rc != 0) goto fail;
        s_configured = 1u;
        printf("[INA226] configured\r\n");
    }

    /* 先读 Mask/Enable：捕获 AFF(bit9) 告警标志；读本身会清芯片锁存（重新武装） */
    BSP_I2C_UI_Lock();
    rc = ina226_rreg16(DSO_INA226_REG_MASK_EN, &me);
    if (rc == 0) {
        rc = ina226_rreg16(DSO_INA226_REG_SHUNT, &shunt_raw);
    }
    BSP_I2C_UI_Unlock();
    if (rc != 0) goto fail;

    lsb = (int32_t)(int16_t)shunt_raw;   /* 有符号分流值 */
    /* M/E 读回：bit15=SOL 使能, bit4=AFF(告警标志,含锁存事件), bit3=CVRF(转换完成,恒置), bit1=APOL */
    aff = (me & 0x0010u) ? 1u : 0u;
    s_shunt_lsb = lsb;
    s_have_data = 1u;
    s_fail_streak = 0u;

    /* 判警：芯片 AFF（含瞬态锁存）或 分流超限；恢复需连续 3 周期正常（软件锁存防抖） */
    alarm = (aff != 0u) || (lsb > (int32_t)DSO_INA226_ALERT_LSB);
    if (alarm) {
        s_power_ok = 0u;
        s_normal_count = 0u;
    } else if (lsb <= (int32_t)DSO_INA226_RESTORE_LSB) {
        if (++s_normal_count >= 3u) {
            s_power_ok = 1u;
        }
    }

    if (!s_reported_first_ok) {
        s_reported_first_ok = 1u;
        printf("[INA226] first ok shunt=%ld.%03ldmV\r\n",
               (long)(lsb * INA226_UV_PER_LSB / 2 / 1000),
               (long)(lsb * INA226_UV_PER_LSB / 2 % 1000));
    }
    if (!s_power_ok && !s_reported_over) {
        s_reported_over = 1u;
        printf("[INA226] OVER-CURRENT cur=%ldmA (alert low)\r\n", (long)(lsb / 40));
    } else if (s_power_ok && s_reported_over) {
        s_reported_over = 0u;
        printf("[INA226] current normal again\r\n");
    }
    /* 诊断：每周期一行（bring-up 后移除） */
    printf("[INA226] shunt=%ld LSB me=0x%04X cur=%ldmA ok=%u\r\n",
           (long)lsb, (unsigned)me, (long)(lsb / 40), (unsigned)s_power_ok);
    return;

fail:
    if (++s_fail_streak == 10u) {
        printf("[INA226] read fail streak=%u\r\n", (unsigned)s_fail_streak);
    }
    if (s_fail_streak > DSO_INA226_FAIL_MAX) {
        s_have_data = 0u;
    }
}

static void DSO_Ina226_Task(void *pv)
{
    (void)pv;
    printf("[INA226] task start (addr=0x%02X, alert>%d LSB=%d.%03dmV, Rshunt=%dmOhm)\r\n",
           (unsigned)DSO_INA226_I2C_ADDR,
           (int)DSO_INA226_ALERT_LSB,
           (int)(DSO_INA226_ALERT_LSB * 5 / 2 / 1000),
           (int)(DSO_INA226_ALERT_LSB * 5 / 2 % 1000),
           (int)DSO_INA226_SHUNT_MOHM);
    for (;;) {
        ina226_measure_once();
        vTaskDelay(pdMS_TO_TICKS(DSO_INA226_POLL_MS));
    }
}

void DSO_Ina226_Init(void)
{
    /* prio 3（> MainTask 的 2）：GUI_Init 卡死 spin 会饿死 prio-1 任务（同 SHT40） */
    if (xTaskCreate(DSO_Ina226_Task, "INA226", 512, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL INA226\r\n");
    }
}

uint8_t DSO_Ina226_PowerOk(void)
{
    return s_power_ok;
}

int32_t DSO_Ina226_GetCurMa(void)
{
    if (!s_have_data) return 0;
    return s_shunt_lsb / 40;   /* lsb*0.025 mA；Rshunt 固定 100mΩ */
}
