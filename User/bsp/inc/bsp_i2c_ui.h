/**
 * @file bsp_i2c_ui.h
 * @brief 面板软件 I2C 总线（PB8=SCL, PB7=SDA，开漏 + 外部上拉）。
 *
 * 由 MCP23017(0x26) 与 PCAL6524(0x20) 共享。FreeRTOS 互斥锁串行化
 * 两个任务（EC 轮询每 8ms 读 MCP23017；开关命令任务写 PCAL6524）对总线的访问。
 */
#ifndef BSP_I2C_UI_H
#define BSP_I2C_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 PB7/PB8 开漏 GPIO 与总线互斥锁。幂等，可多次调用。 */
void BSP_I2C_UI_Init(void);

/** 总线互斥锁：调度器未启动或锁未创建时为空操作（仿 DebugUartLock）。 */
void BSP_I2C_UI_Lock(void);
void BSP_I2C_UI_Unlock(void);

/** 诊断：设置总线位延迟 µs（0=恢复默认 2µs）。慢速用于排除 ACK 采样过早误判。 */
void BSP_I2C_UI_SetBitDelayUs(uint32_t us);

/**
 * ATECC608B 等睡眠唤醒脉冲：SDA 拉低 ≥100µs 后释放（SCL 不动），等 ~2ms。
 * 睡眠中的器件对 I2C 地址 NACK，唤醒后才应答。共享总线上对其他从机无地址字节，安全。
 */
void BSP_I2C_UI_WakePulse(void);

/** 参数化唤醒：SDA 拉低 low_us µs（钳位 60..20000）后释放并等 2ms。 */
void BSP_I2C_UI_WakePulseUs(uint32_t low_us);

/* ---- ATECC608B 实测修复：ACK 采样与可判定波形（2026-09-23） ----
 * 背景：示波器确认芯片在"地址字节第 9 个时钟、SCL 高电平期间"确实拉低了 SDA
 * （地址位型 0xC0），而现行单点采样读到高 → 把 ACK 判成 NACK。 */

/** ACK 多点采样档：0=单点（默认，其它器件时序路径不变）；1=SCL 高电平期内多点采样，
 *  任一次读到低即判 ACK（覆盖"ACK 来得晚 / 低电平短"两种漏读）。仅在 ATECC 会话内开。 */
void BSP_I2C_UI_SetAckMultiSample(uint8_t on);

/** 多点采样档的采样点数（把 SCL 高电平期等分；见 i2c_wait_ack 实现） */
#define I2C_UI_ACK_SAMPLES          32u

/** 示波器同步标记：SCL 保持空闲高，SDA 拉低 low_us（钳位 60..20000）后释放。
 *  不产生任何地址字节；低电平远长于任何正常位，屏上是唯一可识别的触发特征。 */
void BSP_I2C_UI_MarkerPulse(uint32_t low_us);

/** ATECC 唤醒/探测的逐项 DWT 实测结果（tWLO/tWHI/ACK 窗口一并收集） */
typedef struct {
    uint32_t t_lo_us;      /* 唤醒低电平实测宽度（规格 tWLO ≥60µs） */
    uint32_t t_hi_us;      /* 唤醒释放 → 地址帧首个边沿 实测间隔（规格 tWHI ≥1500µs） */
    uint32_t rise2low_ns;  /* ACK 窗口：SCL 上升沿 → SDA 首次读为低（0xFFFFFFFF=整期未低） */
    uint32_t low_ns;       /* ACK 窗口：SDA 低电平宽度 */
    uint32_t n_samples;    /* ACK 窗口**实际**采样次数（防"空循环被当成没 ACK"） */
    uint8_t  tail_low;     /* 窗口末次采样是否为低（= 单点采样应得结论） */
    uint8_t  ctrl_low;     /* 同帧自检：我们驱动低时是否读到低（1=采样链路当时是活的） */
} BSP_I2C_UI_ProbeTiming;

/**
 * 带 DWT 计时的地址探测（诊断用，**不含唤醒**）：START + addr+W，在第 9 个时钟的
 * SCL 高电平期内以 ~50ns 粒度连续采样 SDA，量出 ACK 的真实时刻与宽度；并在同一帧内
 * 做采样链路自检。t_lo_us/t_hi_us 置 0（本函数不含唤醒）。
 * @return 0 = tail_low 为低（应为 ACK）；-1 = 为高（NACK）
 */
int BSP_I2C_UI_ProbeAckTimed(uint8_t addr7, BSP_I2C_UI_ProbeTiming *t);

/**
 * **唤醒 + 等待 + 探测一步到位**，并把规格两项都在**同一帧内**实测出来：
 * GPIO 脉冲唤醒（实测 t_lo_us）→ `bsp_DelayUS(wait_us)`（不依赖 tick，便于核查）→
 * 记录时刻 → 地址帧 + ACK 窗口 → 回填 t_hi_us =（地址帧首个边沿 − 唤醒释放）。
 * 用于核查 ATECC 规范"tWLO ≥60µs、其后 tWHI ≥1500µs"是否真正满足。
 * @param low_us  唤醒低电平宽度（钳位 60..20000）
 * @param wait_us 唤醒释放后的等待（规格要求 ≥1500）
 * @return 0 = 检测到 ACK
 */
int BSP_I2C_UI_WakeProbeTimed(uint8_t addr7, uint32_t low_us, uint32_t wait_us,
                              BSP_I2C_UI_ProbeTiming *t);

/**
 * 带实测的唤醒脉冲（ATECC608 规范形态）：SDA 拉低 low_us µs（钳位 60..20000）
 * 后释放，SCL 保持空闲高、**不产生任何地址字节、不广播**（区别于 0x00 字节法）。
 * @return 用 DWT->CYCCNT 实测到的**实际低电平 µs**（证明 bsp_DelayUS 的真实精度）
 *
 * ⚠️ **本函数不含唤醒后的等待**（与 WakePulseUs 不同）——调用方必须自己满足
 * 规范的 tWHI（唤醒释放后 ≥1500µs 才能访问），否则违例。需要"唤醒+等待+探测"
 * 一步到位时请用 BSP_I2C_UI_WakeProbeTimed()。
 */
uint32_t BSP_I2C_UI_WakePulseMeasUs(uint32_t low_us);

/** 读回当前位延迟 µs（0=未设置，即默认 2µs ≈200kHz）。 */
uint32_t BSP_I2C_UI_GetBitDelayUs(void);

/** 读空闲电平（只读 IDR，不驱动总线）。@param sda_high/scl_high 可传 NULL。 */
void BSP_I2C_UI_IdleLevels(uint8_t *sda_high, uint8_t *scl_high);

/**
 * 总线恢复（标准 9 时钟法）：释放 SDA，SCL 打 9 个脉冲，再发 STOP。
 * 用于解开"从机把 SDA 卡低"造成的死总线（如上一次唤醒/事务被中断）。
 * @param sda_high/scl_high 非 NULL 时输出**恢复后**的空闲电平（1=高）
 * @return 0=两线空闲为高（总线已释放）；-1=仍有线被从机拉低
 */
int BSP_I2C_UI_BusRecover(uint8_t *sda_high, uint8_t *scl_high);

/** 100kHz 标准模式半周期（bsp_DelayUS(5) 实测 ≈5.3µs → 位周期 ≈10.7µs ≈ 93kHz） */
#define I2C_UI_SLOW_HALF_DELAY_US   5u

/**
 * ATECC608B Wake（规范 0x00 字节法）：以 ≤100kHz 连发 n 个 0x00 字节
 * （START + 0x00… + STOP，逐字节忽略 NACK），SDA 连续低 ≈ n×86µs ≥ tWLO 60µs，
 * 释放后 SDA 上升沿唤醒芯片。函数内部临时把位延迟强制为 100kHz、返回前恢复，
 * 故调用方无需（也不会忘记）降速。睡眠中器件忽略 SCL，无需额外控制。
 * @param n 0x00 字节数（钳位 1..4）；推荐 3
 * @return 0=正常（无器件应答 0x00）；1=有器件 ACK 0x00（总线存在应答通用呼叫的器件）
 */
int BSP_I2C_UI_WakeZeroBytes(uint8_t n);

/**
 * 0x00 字节法**参数化**版（用于逐条验证唤醒变体）：
 * @param n       0x00 字节数（钳位 1..4）
 * @param half_us 位延迟半周期 µs（钳位 1..1000）；20µs ≈ 25kHz（满足"≤50kHz 做 Wake"）
 * @param stop    1=按标准以 STOP 收尾；0=**不发 STOP**——字节发完后停在 SCL 低，
 *                先释放 SDA 再抬 SCL，全程不产生 STOP 条件（若用 i2c_stop()，
 *                SDA 低→SCL 高→SDA 高 反而必然生成一个 STOP）
 * @return 0=无器件应答 0x00；1=有器件 ACK（通用呼叫应答，非唤醒判据）
 */
int BSP_I2C_UI_WakeZeroBytesAt(uint8_t n, uint32_t half_us, uint8_t stop);

/** 诊断：持续拉低 SDA 约 ms 毫秒（阻塞），供万用表实测芯片 SDA 脚电平。 */
void BSP_I2C_UI_SdaLowMs(uint32_t ms);

/** 自检：驱动 SDA 低并回读。@return 1=引脚确实被拉低；0=读仍高（OD 配置/接线问题）。 */
int BSP_I2C_UI_SdaLowProbe(void);

/**
 * 向 7-bit 地址器件写单寄存器。
 * @return 0 成功；-1 器件无应答 / 参数错误。
 */
int BSP_I2C_UI_WriteReg(uint8_t addr7, uint8_t reg, uint8_t val);

/**
 * 从 7-bit 地址器件读单寄存器（含 restart）。
 * @return 0 成功；-1 器件无应答 / 参数错误。
 */
int BSP_I2C_UI_ReadReg(uint8_t addr7, uint8_t reg, uint8_t *val);

/**
 * 地址探测：仅发送 addr+W 并检查 ACK，无寄存器访问、无副作用。
 * @return 0 器件应答；-1 无应答。
 */
int BSP_I2C_UI_Probe(uint8_t addr7);

/**
 * 原始写：START + addr+W + len 字节（逐字节收 ACK）+ STOP。
 * 用于无寄存器地址的器件（如 SHT40 命令字节）。
 * @return 0 成功；-1 无应答 / 参数错误。
 */
int BSP_I2C_UI_WriteRaw(uint8_t addr7, const uint8_t *buf, uint8_t len);

/**
 * 逐字节 ACK 记录写（ATECC 诊断用）：START + addr+W + len 字节，
 * 每个字节（含地址）的 ACK 结果记录到 ack_log[i]（1=ACK 0=NACK），
 * 中途 NACK 也继续发完剩余字节（诊断需要），最后 STOP。
 * @param ack_log 长度须 ≥ len+1：ack_log[0]=地址、ack_log[1+i]=buf[i]
 * @return 0=地址 ACK；-1=地址 NACK/参数错误
 */
int BSP_I2C_UI_DiagTxBytes(uint8_t addr7, const uint8_t *buf, uint8_t len,
                           uint8_t *ack_log);

/**
 * 原始读：START + addr+R + len 字节（前 len-1 字节主 ACK、最后 NACK）+ STOP。
 * @return 0 成功；-1 无应答 / 参数错误。
 */
int BSP_I2C_UI_ReadRaw(uint8_t addr7, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* BSP_I2C_UI_H */
