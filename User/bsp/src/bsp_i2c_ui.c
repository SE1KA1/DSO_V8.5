/**
 * @file bsp_i2c_ui.c
 * @brief 面板软件 I2C 总线（PB8=SCL, PB7=SDA）。
 *
 * 原语从 bsp_ui_io_expander.c 原样搬移（GPIO 模拟 I2C，开漏 + 外部上拉），
 * 供 MCP23017(0x26) 与 PCAL6524(0x20) 共享；FreeRTOS 互斥锁防两任务并发交错。
 */
#include "bsp_i2c_ui.h"
#include "bsp.h"          /* bsp_DelayUS, HAL_GPIO, __HAL_RCC */
#include "stm32h7xx_hal.h" /* CoreDebug/DWT（唤醒脉冲实测用）、SystemCoreClock */
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
#define UI_SCL_READ()  (((I2C_UI_GPIO->IDR) & I2C_UI_SCL_PIN) != 0U)

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
        /* **tHD;DAT（关键修正）**：I2C 规范要求 SCL 下降沿之后继续保持 SDA ≥300ns，
         * 让从机跨过其输入端在下降沿附近的不确定区，再允许 SDA 改变/释放。
         * 原实现是 `UI_SCL_0()` 之后**立即**进入下一轮去改 SDA（第 8 位则是立即释放），
         * 保持时间为零 → 输入采样严格的从机（如本板 ATECC608B）可能把某位采错，
         * 于是整帧 CRC 不符、芯片回 Status 0xFF（"CRC/通信错、未执行"）。
         * 这里统一在每次下降沿后保持一个位延迟代价很小（会话档 500µs 时约使帧时长翻倍，
         * 默认 2µs 时几乎无感），且对所有从机都是更合规的时序。 */
        i2c_delay();
        if (i == 7U) {
            UI_SDA_1();   /* 保持够了再释放 SDA，准备收 ACK */
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

static volatile uint8_t s_ack_multisa = 0u;   /* ACK 多点采样档，默认关 */

void BSP_I2C_UI_SetAckMultiSample(uint8_t on)
{
    s_ack_multisa = (on != 0u) ? 1u : 0u;
}

static uint8_t i2c_wait_ack(void)
{
    uint8_t re;

    UI_SDA_1();
    i2c_delay();
    UI_SCL_1();
    if (s_ack_multisa != 0u) {
        /* 多点采样：**高电平期起点与终点各采一次，任一为低即判 ACK**。
         * ⚠️ 曾经写成"32 点铺满窗口、采到低就立刻结束"，那会把 ACK 时隙的 SCL 高电平
         * 压到几十 ns；从机可能没采到第 9 个时钟 → **后续响应帧位计数错位**（表现为
         * 地址 ACK 正常但响应帧 CRC 恒错）。故这里必须**撑满标准高电平期**再降 SCL。 */
        uint8_t early = (UI_SDA_READ() == 0u) ? 1u : 0u;
        i2c_delay();                                  /* 保持与单点采样相同的时钟占空 */
        re = (early != 0u || UI_SDA_READ() == 0u) ? 0u : 1u;
    } else {
        i2c_delay();
        re = UI_SDA_READ() ? 1U : 0U;
    }
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
    if (us > 1000u) {   /* 防诊断误设把整条总线拖死 */
        us = 1000u;
    }
    s_bit_delay_us = us;
}

uint32_t BSP_I2C_UI_GetBitDelayUs(void)
{
    return s_bit_delay_us;
}

/* ---- DWT 周期计数（仅诊断计时：实测唤醒低电平/位周期的真实 µs） ---- */

static void ui_dwt_enable(void)
{
    /* 就地使能写法同 dso_freq_meter.c；只置 TRCENA/CYCCNTENA，不占外设 */
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0u) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0u) {
        DWT->CYCCNT = 0u;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

static uint32_t ui_cyc_now(void)
{
    ui_dwt_enable();
    return DWT->CYCCNT;
}

static uint32_t ui_cyc_to_us(uint32_t cyc)
{
    if (SystemCoreClock == 0u) {
        return 0u;
    }
    return (uint32_t)(((uint64_t)cyc * 1000000ull) / (uint64_t)SystemCoreClock);
}

/* ns 级换算（ProbeAckTimed 量 ACK 时刻用） */
static uint32_t ui_cyc_to_ns(uint32_t cyc)
{
    if (SystemCoreClock == 0u) {
        return 0u;
    }
    return (uint32_t)(((uint64_t)cyc * 1000000000ull) / (uint64_t)SystemCoreClock);
}

/* µs → 周期数（把 SCL 高电平期换算成 DWT 周期做窗口边界） */
static uint32_t ui_us_to_cyc(uint32_t us)
{
    return (uint32_t)(((uint64_t)us * (uint64_t)SystemCoreClock) / 1000000ull);
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

uint32_t BSP_I2C_UI_WakePulseMeasUs(uint32_t low_us)
{
    uint32_t t0;
    uint32_t t1;

    if (low_us < 60u) {
        low_us = 60u;
    }
    if (low_us > 20000u) {
        low_us = 20000u;
    }

    /* 规范形态：SCL 先确保为高（空闲），再把 SDA 拉低 ≥tWLO 后释放。
     * 不产生任何地址字节、不广播，因此不打扰同总线的 PCAL6524/SHT40/INA226。 */
    UI_SCL_1();
    i2c_delay();
    t0 = ui_cyc_now();
    UI_SDA_0();
    bsp_DelayUS(low_us);
    UI_SDA_1();
    t1 = ui_cyc_now();
    return ui_cyc_to_us(t1 - t0);
}

void BSP_I2C_UI_MarkerPulse(uint32_t low_us)
{
    if (low_us < 60u) {
        low_us = 60u;
    }
    if (low_us > 20000u) {
        low_us = 20000u;
    }
    /* SCL 保持空闲高；SDA 长低（远长于任何正常位）→ 屏上唯一的触发特征 */
    UI_SCL_1();
    i2c_delay();
    UI_SDA_0();
    bsp_DelayUS(low_us);
    UI_SDA_1();
}

/* 与 i2c_send_byte 位序完全一致的地址发送，额外在最后一个数据位（被我们驱动为低）
 * 期间回读 SDA —— 即"采样链路自检"，证明该瞬间线路上我们拉的低能被读到。 */
static void ack_timed_send_addr(uint8_t b, uint8_t *ctrl_low)
{
    uint8_t i;
    uint8_t ctrl = 0u;

    for (i = 0u; i < 8u; i++) {
        if ((b & 0x80u) != 0u) {
            UI_SDA_1();
        } else {
            UI_SDA_0();
        }
        i2c_delay();
        UI_SCL_1();
        i2c_delay();
        if (i == 7u) {
            ctrl = UI_SDA_READ() ? 0u : 1u;   /* 此刻 SDA 由我们驱动为低 → 应读到低 */
        }
        UI_SCL_0();
        if (i == 7u) {
            UI_SDA_1();
        }
        b <<= 1;
    }
    if (ctrl_low != NULL) {
        *ctrl_low = ctrl;
    }
}

/* 内部：START + 地址字节 + 第 9 个时钟窗口采样 + STOP。
 * 填 rise2low_ns/low_ns/n_samples/tail_low/ctrl_low；t_lo_us/t_hi_us 由调用方负责。
 * t_frame_start（非 NULL 时输出）：地址帧**首个边沿之前**的时刻（供算 tWHI）。 */
static void addr_frame_timed(uint8_t addr7, BSP_I2C_UI_ProbeTiming *t,
                             uint32_t *t_frame_start)
{
    uint32_t t_rise;
    uint32_t t_now;
    uint32_t t_end;
    uint32_t t_first = 0u;
    uint32_t t_last_low = 0u;
    uint32_t half_us;
    uint32_t n = 0u;
    uint8_t saw_low = 0u;
    uint8_t last = 0u;
    uint8_t ctrl = 0u;

    half_us = (s_bit_delay_us != 0u) ? s_bit_delay_us : 2u;

    if (t_frame_start != NULL) {
        *t_frame_start = ui_cyc_now();   /* START 前一刻：通信正式开始 */
    }
    i2c_start();
    ack_timed_send_addr((uint8_t)((addr7 << 1) | I2C_UI_WR), &ctrl);

    /* 第 9 个时钟：SCL 拉高后按 ~50ns 粒度连续采样 SDA，直到高电平期结束 */
    UI_SDA_1();            /* 释放 SDA，等从机应答 */
    i2c_delay();
    UI_SCL_1();
    t_rise = ui_cyc_now();
    t_end = t_rise + ui_us_to_cyc(half_us);
    for (;;) {
        t_now = ui_cyc_now();
        if ((int32_t)(t_now - t_end) >= 0) {   /* 窗口结束（按差值判，避免绝对比较） */
            break;
        }
        n++;
        if (UI_SDA_READ()) {
            last = 0u;
        } else {
            last = 1u;
            if (saw_low == 0u) {
                saw_low = 1u;
                t_first = t_now;
            }
            t_last_low = t_now;
        }
    }
    UI_SCL_0();
    i2c_delay();
    i2c_stop();

    t->rise2low_ns = (saw_low != 0u) ? ui_cyc_to_ns(t_first - t_rise) : 0xFFFFFFFFu;
    t->low_ns      = (saw_low != 0u) ? ui_cyc_to_ns(t_last_low - t_first) : 0u;
    t->n_samples   = n;
    t->tail_low    = last;
    t->ctrl_low    = ctrl;
}

int BSP_I2C_UI_ProbeAckTimed(uint8_t addr7, BSP_I2C_UI_ProbeTiming *t)
{
    if (t == NULL) {
        return -1;
    }
    t->t_lo_us = 0u;       /* 本函数不含唤醒 */
    t->t_hi_us = 0u;
    addr_frame_timed(addr7, t, NULL);
    return (t->tail_low != 0u) ? 0 : -1;
}

int BSP_I2C_UI_WakeProbeTimed(uint8_t addr7, uint32_t low_us, uint32_t wait_us,
                              BSP_I2C_UI_ProbeTiming *t)
{
    uint32_t ta;
    uint32_t tb;
    uint32_t t_frame = 0u;

    if (t == NULL) {
        return -1;
    }
    if (low_us < 60u) {
        low_us = 60u;
    }
    if (low_us > 20000u) {
        low_us = 20000u;
    }

    /* 1) 唤醒低脉冲（SCL 保持空闲高，不产生地址字节）+ 实测 tWLO */
    UI_SCL_1();
    i2c_delay();
    ta = ui_cyc_now();
    UI_SDA_0();
    bsp_DelayUS(low_us);
    UI_SDA_1();
    tb = ui_cyc_now();
    t->t_lo_us = ui_cyc_to_us(tb - ta);

    /* 2) 唤醒后的等待：用 bsp_DelayUS 而非 vTaskDelay —— 不依赖 tick 粒度，
     *    保证核查的是"真实等待时间"（规范 tWHI ≥1500µs）。 */
    bsp_DelayUS(wait_us);

    /* 3) 地址帧 + ACK 窗口；tWHI 实测 = 唤醒释放 → START 建立之前。
     *    注意这是**保守**取值：线路上的第一个边沿（START 的 SDA 下降沿）还要再晚
     *    约一个位延迟，故"实测 ≥1500µs"成立时，真实间隔更大。 */
    addr_frame_timed(addr7, t, &t_frame);
    t->t_hi_us = ui_cyc_to_us(t_frame - tb);

    return (t->tail_low != 0u) ? 0 : -1;
}

/* ---- 总线恢复与空闲电平 ---- */

void BSP_I2C_UI_IdleLevels(uint8_t *sda_high, uint8_t *scl_high)
{
    if (sda_high != NULL) { *sda_high = UI_SDA_READ() ? 1u : 0u; }
    if (scl_high != NULL) { *scl_high = UI_SCL_READ() ? 1u : 0u; }
}

int BSP_I2C_UI_BusRecover(uint8_t *sda_high, uint8_t *scl_high)
{
    uint8_t sda = 0u;
    uint8_t scl = 0u;
    uint8_t i;

    /* 若从机把 SDA 卡低（上次事务被中断），用 9 个 SCL 脉冲把它的移位寄存器
     * 走完，再补一个 STOP 释放总线。SDA 全程只释放（不驱动低）。 */
    UI_SDA_1();
    for (i = 0u; i < 9u; i++) {
        UI_SCL_0();
        bsp_DelayUS(5u);
        UI_SCL_1();
        bsp_DelayUS(5u);
    }
    /* 标准 STOP：SDA 低 → SCL 高 → SDA 高 */
    i2c_stop();

    BSP_I2C_UI_IdleLevels(&sda, &scl);
    if (sda_high != NULL) { *sda_high = sda; }
    if (scl_high != NULL) { *scl_high = scl; }
    return (sda != 0u && scl != 0u) ? 0 : -1;
}

/* ATECC608B Wake（规范 0x00 字节法）：以 ≤100kHz 连发 n 个 0x00 字节。
 * 关键：默认位延迟 2µs 时一个 0x00 字节只产生 ≈39µs 低电平 < tWLO 60µs，
 * 必然唤不醒；故这里强制 5µs 半周期（≈93kHz，1 字节 SDA 低 ≈86µs），
 * 并在返回前恢复原值——调用方无需感知速率。 */
int BSP_I2C_UI_WakeZeroBytesAt(uint8_t n, uint32_t half_us, uint8_t stop)
{
    uint32_t saved = s_bit_delay_us;
    uint8_t i;
    int acked = 0;

    if (n == 0u) {
        n = 1u;
    }
    if (n > 4u) {
        n = 4u;
    }
    if (half_us < 1u) {
        half_us = I2C_UI_SLOW_HALF_DELAY_US;
    }
    if (half_us > 1000u) {
        half_us = 1000u;
    }

    s_bit_delay_us = half_us;

    i2c_start();
    for (i = 0u; i < n; i++) {
        i2c_send_byte(0x00u);            /* 8 位全 0 → SDA 连续低（5µs 档 ≈86µs） */
        if (i2c_wait_ack() == 0u) {
            /* acked 语义＝"有器件 ACK 了这第一个 0x00"：注意该 0x00 **就是地址字节**，
             * 即 I2C 通用呼叫地址 general call（0000 000）——本总线上有器件会应答它，
             * 故 acked=1 属正常现象，**不能作为"ATECC 已唤醒"的判据**。
             * 相较之下 BSP_I2C_UI_WakePulseMeasUs() 不产生任何地址字节，是更干净的唤醒法。 */
            acked = 1;
        }
    }

    if (stop != 0u) {
        i2c_stop();
    } else {
        /* 不发 STOP 变体：此刻 i2c_wait_ack 结束时 SCL 已在低、SDA 已释放。
         * 顺序必须是"先释放 SDA（SCL 低时 SDA 变化不构成任何条件）→ 再抬 SCL"，
         * 这样全程不产生 STOP 条件。若照旧调 i2c_stop()（SDA 低→SCL 高→SDA 高）
         * 反而**必然**生成一个 STOP —— 这正是该变体要避免的。 */
        UI_SDA_1();
        i2c_delay();
        UI_SCL_1();
        i2c_delay();
    }

    s_bit_delay_us = saved;              /* 恢复原档位 */
    return acked;
}

int BSP_I2C_UI_WakeZeroBytes(uint8_t n)
{
    /* 既有行为不变：5µs 半周期（≈93kHz）+ 标准 STOP 收尾 */
    return BSP_I2C_UI_WakeZeroBytesAt(n, I2C_UI_SLOW_HALF_DELAY_US, 1u);
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

int BSP_I2C_UI_WriteRead(uint8_t addr7, const uint8_t *wbuf, uint8_t wlen,
                         uint8_t *rbuf, uint8_t rlen)
{
    int rc = 0;
    uint8_t i;

    if (wbuf == NULL || wlen == 0u || rbuf == NULL || rlen == 0u) {
        return -1;
    }

    i2c_start();
    i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_WR));
    if (i2c_wait_ack() != 0U) {
        rc = -1;
    } else {
        for (i = 0U; i < wlen; i++) {
            i2c_send_byte(wbuf[i]);
            if (i2c_wait_ack() != 0U) {
                rc = -1;
                break;
            }
        }
        if (rc == 0) {
            i2c_start();                     /* 重复起始（EEPROM 随机读必需） */
            i2c_send_byte((uint8_t)((addr7 << 1) | I2C_UI_RD));
            if (i2c_wait_ack() != 0U) {
                rc = -1;
            } else {
                for (i = 0U; i < rlen; i++) {
                    rbuf[i] = i2c_read_byte();
                    if (i == (uint8_t)(rlen - 1u)) {
                        i2c_nack();
                    } else {
                        i2c_master_ack();
                    }
                }
            }
        }
    }
    i2c_stop();
    return rc;
}
