/**
 * @file dso_atecc608b.c
 * @brief ATECC608B-SSHDA-T 驱动：I2C 命令/状态机（软件 I2C BSP_I2C_UI，PB7/PB8，7-bit 0x60）。
 *
 * 协议（Microchip DS40002138A/DS40002154B §4.1/§7）：
 *   写：START [addr+W] [word=0x03] {Count Op P1 P2lo P2hi Data… CRC16lo CRC16hi} STOP
 *   读：忙/空闲时设备对读地址 NACK → 轮询；就绪后读回输出组
 *        [Count][数据(count-3)…][CRC16lo][CRC16hi]（FIFO，可分多次读）
 *   无效组恒 Count=4：[4][状态][CRC16lo][CRC16hi]；状态 0x00=成功。
 *   CRC16：poly 0x8005（反射 0xA001），init 0x0000，LSB 先发。
 *
 * 唤醒：Sleep 中的芯片对**任何 I2C 地址一律 NACK**（看起来像总线上没有器件），
 * 必须先 Wake：SDA 在 SCL 空闲高时被拉低 ≥tWLO(60µs)，释放后上升沿唤醒，
 * 再等 ≥tWHI(1.5ms) 才能访问 7-bit 地址。两条实现：
 *   - GPIO 脉冲法（BSP_I2C_UI_WakePulseMeasUs，本文件主用）：不产生任何地址字节，
 *     不打扰同总线的 PCAL6524/SHT40/INA226；
 *   - 0x00 字节法（BSP_I2C_UI_WakeZeroBytes）：规范里的传统写法，但其第一个 0x00
 *     就是 I2C 通用呼叫地址（general call），本总线有器件会应答它，故该返回值
 *     **不能**当唤醒判据。
 * 关键约束：唤醒与寻址必须在**同一持锁窗口**内（中途只等 tWHI）——把锁放开会让
 * SHT40/INA226 的 1s 轮询插进窗口，探针也会落到唤醒窗口之外（曾因此恒判 NACK）。
 *
 * 每帧独立持锁（BSP_I2C_UI_Lock/Unlock）。
 *
 * 【档位与持锁的约定】BSP_I2C_UI_SetBitDelayUs() 影响**全总线**：必须
 * **先还原为 0 再 Unlock()**，否则同总线的 SHT40/INA226/PCAL 事务会被拖到慢档
 * （2026-10-09 用逻辑分析仪抓到过 1kHz 的 INA226 帧，根因就是解锁前没还原）。
 *
 * 【编码约定 —— 编译期坑，务必遵守】本文件是 UTF-8，但 Keil AC5 按 ANSI(GBK)
 * 解析源码：一个中文字符的末字节若紧邻 `"` 或 `\`，会与它凑成一个合法 GBK 汉字，
 * 把它吃掉 —— 表现是 `error: #8: missing closing quote`（或 `\r` 的 `\` 被吃掉）。
 * 因此**本文件的所有 printf 字符串一律只用 ASCII**（诊断输出在任意终端也干净）；
 * 中文只出现在注释里（注释里被吃掉一个字符无害）。新增打印请沿用 [ATECC-x] 前缀。
 */
#include "dso_atecc608b.h"
#include "bsp_i2c_ui.h"
#include "debug_uart_lock.h"
#include "stm32h7xx_hal.h"   /* HAL_GetTick（Arm 0 上电事实核对） */
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>   /* memcpy（授权自测里搬运签名） */

#define ATECC_RETRY_MS   1u          /* 忙时轮询间隔 */
#define ATECC_PROBE_N    3u          /* 探测重试次数 */

/* Config 区关键字节偏移（ATECC608 存储器映射；注意 word n 的字节偏移 = n*4，
 * 故 LockValue/LockConfig 位于 word 0x15 的 byte2/byte3 = 0x56/0x57） */
#define ATECC_CFG_REV_OFF           0x08u   /* RevNum（608B 低字节 0x60） */
#define ATECC_CFG_SN8_OFF           0x0Cu   /* SN[8] */
#define ATECC_CFG_LOCK_VALUE_OFF    0x56u   /* LockValue（0x00=已锁） */
#define ATECC_CFG_LOCK_CONFIG_OFF   0x57u   /* LockConfig（0x00=已锁） */
#define ATECC_CFG_SLOT_LOCKED_OFF   0x58u   /* SlotLocked[0..1] */

static uint8_t s_last_status = 0xFFu;   /* 最近执行状态码 */
static uint8_t s_status_valid = 0u;     /* 1=本轮确实从芯片读到了状态字节（区分初值 0xFF） */
/* 会话档位延迟（半周期 µs）：默认取宏；诊断可在运行时改（速率扫描用）。
 * 注意 Exec 会用它覆盖总线档位，故调用方设的档位会被覆盖——扫档必须走这个变量。 */
static uint32_t s_session_bit_us = DSO_ATECC_SESSION_BIT_US;
/* 当前工作地址：默认 0x60；探测发现实际配置地址（如 0x50/0x64）后更新 */
static uint8_t s_addr = DSO_ATECC608B_I2C_ADDR;
/* CRC 约定：0=标准 CRC-16/ARC（正品）；1=位序镜像（本批仿制件，见头文件说明）。
 * 只由 CRC 约定判定臂或上层显式设置，默认 0 —— 对正品器件行为零影响。 */
static uint8_t s_crc_mirror = 0u;
/* 镜像约定下"它读 CRC 两字节的次序"未定：0=先低后高（与它自己的发送次序一致，默认），
 * 1=先高后低。仅由判定臂按实测结果设置；标准约定下无意义。 */
static uint8_t s_crc_swap = 0u;
/* Read(32 字节) 的**块地址单位**（4 字节字/块）：规格值 8（32B 块 = 8 字，见头文件）。
 * 本片若对 Param2 编码与规格不同，ReadConfig32 首次失败后自动判别一次并改这里。 */
static uint8_t s_rd32_unit = 8u;
static uint8_t s_rd32_probed = 0u;   /* 1 = 已判别过，不再自动试 */

/* ---- CRC16（poly 0x8005 反射 0xA001，init 0，LSB 先发） ---- */

static uint16_t atecc_crc16(const uint8_t *d, uint16_t len)
{
    uint16_t crc = 0u;
    uint16_t i;
    uint8_t j;
    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)d[i];
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/* CRC 续算（供跨段拼接 [Count]+data 时使用） */
static uint16_t atecc_crc16_cont(const uint8_t *d, uint16_t len, uint16_t crc)
{
    uint16_t i;
    uint8_t j;
    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)d[i];
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/* 16 位位序镜像：把 CRC 值的两半各自按位反转并交换字节。
 * 镜像约定下，"我方标准 CRC 的 rev16"正是本批器件期望/回送的数值。 */
static uint16_t atecc_rev16(uint16_t v)
{
    uint16_t r = 0u;
    uint8_t i;
    for (i = 0u; i < 16u; i++) {
        r = (uint16_t)((r << 1) | (uint16_t)((v >> i) & 1u));
    }
    return r;
}

/* 按当前约定把"计算出的标准 CRC"变换成**上/下线实际使用的数值**：
 * 调用方仍按 `(crc & 0xFF) 先发、crc >> 8 后发` 的固定写法使用即可 ——
 * 需要换字节序时，这里把返回值对调，使"先发的低字节"就是该先发的那个。
 * 发送前与校验前都调用它，保证两个方向自洽（同一约定必须同时用于写与读）。 */
static uint16_t atecc_crc_wire(uint16_t crc)
{
    uint16_t w = (s_crc_mirror != 0u) ? atecc_rev16(crc) : crc;
    if (s_crc_swap != 0u) {
        w = (uint16_t)((w << 8) | (w >> 8));
    }
    return w;
}

/* 32 字节读时，第 block 块在 Param2 里的地址（按当前认定的单位） */
static uint16_t atecc_rd32_addr(uint8_t block)
{
    return (uint16_t)((uint16_t)block * (uint16_t)s_rd32_unit);
}

uint8_t DSO_Atecc608b_LastStatus(void)
{
    return s_last_status;
}

void DSO_Atecc608b_SetCrcMirror(uint8_t on)
{
    s_crc_mirror = (on != 0u) ? 1u : 0u;
}

uint8_t DSO_Atecc608b_CrcMirror(void)
{
    return s_crc_mirror;
}

/* ---- 命令执行（组包 + 状态机轮询 + 响应校验） ---- */

/* 响应帧 CRC 失败时打印原始字节：用来区分"帧错位"（count 不对）还是"数据读错"
 * （count 对但数据乱）——后者是速率/接线问题，前者是协议/时序问题。 */
static void atecc_dump_resp(const char *tag, uint8_t cnt, const uint8_t *b, uint8_t n)
{
    char hx[3u * 12u + 1u];
    uint8_t m = (n > 12u) ? 12u : n;
    uint8_t i;

    for (i = 0u; i < m; i++) {
        (void)snprintf(&hx[3u * i], 4u, "%02X ", b[i]);
    }
    hx[3u * m] = '\0';
    DebugUartLock();
    printf("[ATECC] %s: count=%u raw=%s\r\n", tag, (unsigned)cnt, hx);
    DebugUartUnlock();
}

/* 内部实现；对外的 DSO_Atecc608b_Exec 只是在本体外面开关 ACK 多点采样档 */
static int atecc_exec_body(uint8_t opcode, uint8_t p1, uint16_t p2,
                           const uint8_t *in, uint16_t in_len,
                           uint8_t *out, uint16_t out_max)
{
    uint8_t group[DSO_ATECC608B_GROUP_MAX];  /* 写命令组 */
    uint8_t body[DSO_ATECC608B_GROUP_MAX];   /* 读回数据+CRC 暂存 */
    uint8_t first;
    uint16_t count;
    uint16_t crc;
    uint16_t data_len;
    uint32_t deadline;
    int rc = 0;
    uint16_t i;

    s_last_status = 0xFFu;
    s_status_valid = 0u;      /* 本轮尚未从芯片读到状态字节 */
    if ((in_len > 0u && in == NULL) || (out_max > 0u && out == NULL)) {
        return DSO_ATECC_ERR_PARAM;
    }

#if ATECC_WAKE_BEFORE_EXEC
    /* 芯片在 Sleep 时对任何命令都不响应 —— 每帧先按规范 Wake */
    if (DSO_Atecc608b_Wake(NULL, NULL) != 0) {
        return DSO_ATECC_ERR_TIMEOUT;
    }
#endif
    count = (uint16_t)(7u + in_len);          /* Count+Op+2×Param+Data+2×CRC */
    if (count < 7u || count > DSO_ATECC608B_GROUP_MAX) {
        return DSO_ATECC_ERR_PARAM;
    }

    group[0] = (uint8_t)count;
    group[1] = opcode;
    group[2] = p1;
    group[3] = (uint8_t)(p2 & 0xFFu);
    group[4] = (uint8_t)(p2 >> 8);
    for (i = 0u; i < in_len; i++) {
        group[5u + i] = in[i];
    }
    crc = atecc_crc_wire(atecc_crc16(group, count - 2u));
    group[count - 2u] = (uint8_t)(crc & 0xFFu);   /* LSB 先发 */
    group[count - 1u] = (uint8_t)(crc >> 8);

    /* 写：[word=0x03] + 命令组，单帧 */
    {
        uint8_t frame[DSO_ATECC608B_GROUP_MAX + 1u];
        frame[0] = DSO_ATECC608B_WORD_CMD;
        for (i = 0u; i < count; i++) {
            frame[1u + i] = group[i];
        }
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_WriteRaw(s_addr, frame,
                                 (uint8_t)(count + 1u));
        BSP_I2C_UI_Unlock();
    }
    if (rc != 0) {
        return DSO_ATECC_ERR_IO;
    }

    /* 状态机：忙时读地址 NACK → 1ms 重试，直到拿到 Count 或超时 */
    deadline = (uint32_t)xTaskGetTickCount() + pdMS_TO_TICKS(DSO_ATECC608B_RESP_MS);
    for (;;) {
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_ReadRaw(s_addr, &first, 1u);
        BSP_I2C_UI_Unlock();
        if (rc == 0) {
            break;
        }
        if ((int32_t)(deadline - (uint32_t)xTaskGetTickCount()) <= 0) {
            return DSO_ATECC_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(ATECC_RETRY_MS));
    }
    count = first;

    if (count == 4u) {
        /* 错误组：[4][status][crc_lo][crc_hi]（首字节已读，余 3 字节） */
        uint8_t rest[3];
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_ReadRaw(s_addr, rest, 3u);
        BSP_I2C_UI_Unlock();
        if (rc != 0) {
            return DSO_ATECC_ERR_IO;
        }
        /* **先记录状态字节再做 CRC 校验**：芯片状态（如 0xFF=通信错）比"我们的 CRC 是否
         * 对得上"更重要——本片错误组的 CRC 恰好过不了，若把状态码挡在校验之后就会丢失，
         * 重发判据也就永远不成立。 */
        s_last_status = rest[0];
        s_status_valid = 1u;

        crc = atecc_crc16(&first, 1u);            /* CRC 覆盖 [count][status] */
        crc = atecc_crc_wire(atecc_crc16_cont(&rest[0], 1u, crc));
        if ((uint8_t)(crc & 0xFFu) != rest[1] || (uint8_t)(crc >> 8) != rest[2]) {
            atecc_dump_resp("errgrp crc", first, rest, 3u);
            return DSO_ATECC_ERR_CRC;
        }
        if (s_last_status != 0x00u) {
            return DSO_ATECC_ERR_EXEC;
        }
        return 0;                                 /* 成功且无输出 */
    }

    if (count < 5u || count > (uint16_t)(out_max + 3u)) {
        return DSO_ATECC_ERR_PARAM;               /* 输出超预期 */
    }
    data_len = (uint16_t)(count - 3u);
    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_ReadRaw(s_addr, body, (uint8_t)(count - 1u));
    BSP_I2C_UI_Unlock();
    if (rc != 0) {
        return DSO_ATECC_ERR_IO;
    }
    /* CRC 覆盖 [count]+data（共 count-2 字节）；尾两字节为收到的 CRC */
    crc = atecc_crc16(&first, 1u);
    crc = atecc_crc_wire(atecc_crc16_cont(body, data_len, crc));
    if ((uint8_t)(crc & 0xFFu) != body[data_len] ||
        (uint8_t)(crc >> 8) != body[data_len + 1u]) {
        atecc_dump_resp("datagrp crc", first, body, data_len + 2u);
        return DSO_ATECC_ERR_CRC;
    }
    for (i = 0u; i < data_len; i++) {
        out[i] = body[i];
    }
    /* 数据组响应里没有状态字节：标记为"已确认成功"，否则日志里的 st= 会一直显示初值
     * 0xFF，与 rc=0 并排出现时会误导（0xFF 是"通信错"的意思）。 */
    s_last_status = 0x00u;
    s_status_valid = 1u;
    return (int)data_len;
}

int DSO_Atecc608b_Exec(uint8_t opcode, uint8_t p1, uint16_t p2,
                       const uint8_t *in, uint16_t in_len,
                       uint8_t *out, uint16_t out_max)
{
    int rc;
    uint8_t attempt;
    uint32_t saved = BSP_I2C_UI_GetBitDelayUs();

    /* ATECC 会话内：换到会话档位延迟 + 用多点采样判 ACK（用完立刻还原，保证
     * SHT40/INA226/PCAL 的时序路径完全不变）。
     *
     * **按数据手册重发（关键）**：Status `0xFF` = "CRC or Other Communications error
     * —— 命令未被正确接收，芯片没有解析也没有执行，应由 I/O 驱动层重发"，
     * 且规格明确"CRC 错误总是最先返回、可重发"。我们此前从不重发 → 每条命令都
     * 直接失败。这里按手册做最多 3 次重发（每次都重新唤醒 + 重新组包发送）。 */
    for (attempt = 0u; attempt < 3u; attempt++) {
        BSP_I2C_UI_SetBitDelayUs(s_session_bit_us);
        BSP_I2C_UI_SetAckMultiSample(1u);
        rc = atecc_exec_body(opcode, p1, p2, in, in_len, out, out_max);
        BSP_I2C_UI_SetAckMultiSample(0u);
        BSP_I2C_UI_SetBitDelayUs(saved);

        /* 只有"通信错（0xFF）"值得重发；其它错误重发无用。
         * 注意用 s_status_valid 区分"芯片确实回了 0xFF"与"什么都没读到"。 */
        if (!(s_status_valid != 0u && s_last_status == 0xFFu)) {
            break;
        }
        DebugUartLock();
        printf("[ATECC] status 0xFF (comm error) -> retransmit #%u\r\n",
               (unsigned)(attempt + 1u));
        DebugUartUnlock();
    }
    return rc;
}

/* ---- 唤醒（TNGTLS 规范） ---- */

static int atecc_readprobe(uint8_t addr);   /* 前置声明：供 Probe / 诊断使用 */

int DSO_Atecc608b_Wake(uint8_t *zero_acked, uint16_t *ready_ms)
{
    uint32_t t0;
    uint32_t el;
    uint8_t acked = 0u;   /* 0=未用 0x00 字节法/无器件应答（供调用方参考，非唤醒判据） */
    int p = -1;

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        /* 调度器未启动：不能 vTaskDelay，只能 GPIO 脉冲唤醒 */
        BSP_I2C_UI_WakePulse();
        if (zero_acked != NULL) { *zero_acked = 0u; }
        if (ready_ms != NULL)   { *ready_ms = 0xFFFFu; }
        return DSO_ATECC_ERR_TIMEOUT;
    }

    /* 唤醒与寻址同处一个持锁窗口：中途释放锁会让同总线任务（SHT40/INA226 的 1s
     * 轮询）插进来，且探针可能落到唤醒窗口之外 → 只会看到恒 NACK。
     *
     * ⚠️ 唤醒方法：**优先 GPIO 脉冲法**（SCL 保持空闲高、SDA 拉低 ≥60µs 后释放），
     * 0x00 字节法降为兜底。原因（2026-10-11 查明）：ATECC 的 word address
     * `0x00` = **Reset**、`0x01` = Sleep、`0x02` = Idle、`0x03` = Command；
     * 而 0x00 字节法的**第一个字节就是地址 0x00（通用呼叫）**，本总线上有器件会应答它
     * （实测 gc_ack=1），若 ATECC 也参与应答，则后续 0x00 就是给它下 word 0x00=Reset
     * —— 那等于**每条命令前先复位芯片**，命令自然全部不被执行（响应状态 0xFF）。
     * GPIO 脉冲法不产生任何地址/数据字节，无此风险；实测裸读用它的结果正是文档记载的
     * 唤醒状态（count=4/status=0x11）。 */
    t0 = (uint32_t)xTaskGetTickCount();
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_WakePulseMeasUs(100u);       /* GPIO 脉冲（SCL 保持高） */
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    p = BSP_I2C_UI_Probe(s_addr);
    if (p != 0) {
        /* 兜底 1：自检使能时 tWHIST 可达 20ms，保守再等一轮 */
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
        p = BSP_I2C_UI_Probe(s_addr);
    }
    /* 兜底 2 **已移除**：不再用 0x00 字节法"唤醒"。
     * 数据手册确认 ATECC 的 word address `0x00` = **Reset**，而 0x00 字节法的首字节
     * 正是地址 0x00（通用呼叫）、其后全是 0x00 —— 若被芯片当作命令接口收到，等于
     * **给刚唤醒的芯片下一条 Reset**，会把它送回睡眠（随后地址一律 NACK，正是实测现象）。
     * 规范要求的唤醒条件只是"SDA 在 SCL 为高时保持低 ≥tWLO"，GPIO 脉冲法已完全满足。 */
    BSP_I2C_UI_Unlock();
    if (zero_acked != NULL) { *zero_acked = acked; }

    el = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
    if (ready_ms != NULL) {
        *ready_ms = (p == 0) ? (uint16_t)el : 0xFFFFu;
    }
    return (p == 0) ? 0 : DSO_ATECC_ERR_TIMEOUT;
}

/* ---- 常用包装 ---- */

int DSO_Atecc608b_InfoRevision(uint8_t rev[4])
{
    int rc;
    if (rev == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    /* Info opcode 0x30，Param1=0x00(Revision)，无数据，回 4 字节版本 */
    rc = DSO_Atecc608b_Exec(ATECC_OP_INFO, 0x00u, 0x0000u,
                            NULL, 0u, rev, 4u);
    return (rc == 4) ? 0 : rc;
}

int DSO_Atecc608b_ReadConfig32(uint8_t block, uint8_t out[32])
{
    int rc;

    if (out == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    rc = DSO_Atecc608b_Exec(ATECC_OP_READ, ATECC_READ32_P1, atecc_rd32_addr(block),
                            NULL, 0u, out, 32u);
    if (rc == 32 || s_rd32_probed != 0u) {
        return (rc == 32) ? 0 : rc;
    }

    /* **一次性块地址单位判别**（每次上电最多跑一次）：
     * 规格是"地址以 4 字节字计、32B 块 → blk*8"，且 Param1 低 4 位必须为 0。
     * 若本片实现不同，用同一块的备选单位再试 —— 非法的候选会被芯片以
     * 0x0F(ExecutionError) 拒掉，所以"哪一次成功"本身就自证地址是对的，
     * 不是猜测。判别结果只影响本次运行，日志会写明。 */
    {
        static const uint8_t cand[2] = { 32u, 1u };   /* 备选：按字节 / 按块号 */
        uint8_t k;
        uint8_t probe[32];
        int prc;

        for (k = 0u; k < 2u; k++) {
            prc = DSO_Atecc608b_Exec(ATECC_OP_READ, ATECC_READ32_P1,
                                     (uint16_t)((uint16_t)block * (uint16_t)cand[k]),
                                     NULL, 0u, probe, 32u);
            if (prc == 32) {
                s_rd32_unit = cand[k];
                memcpy(out, probe, 32u);
                DebugUartLock();
                printf("[ATECC-rd] 32B block-addr unit autodetect: blk*%u accepted "
                       "(spec = blk*8) -> using blk*%u\r\n",
                       (unsigned)cand[k], (unsigned)cand[k]);
                DebugUartUnlock();
                rc = 32;
                break;
            }
        }
    }
    s_rd32_probed = 1u;
    return (rc == 32) ? 0 : rc;
}

int DSO_Atecc608b_Probe(void)
{
    /* 候选地址：0x35 = ATECC608B-TNGTLS 出厂默认（权威）；0x36 备选；
     * 其余为历史候选。每个候选都先按规范 Wake，再"读探针 NACK 才发命令"
     * （读也 ACK → 内存映射型器件如 EEPROM，跳过以防误写）。 */
    static const uint8_t cand[] = { 0x36u, 0x60u, 0x35u, 0x50u, 0x64u, 0x68u };
    uint8_t rev[4];
    uint8_t ci;
    uint8_t n;
    int rc = DSO_ATECC_ERR_TIMEOUT;
    for (ci = 0u; ci < (uint8_t)(sizeof(cand) / sizeof(cand[0])); ci++) {
        s_addr = cand[ci];
        if (DSO_Atecc608b_Wake(NULL, NULL) != 0) {
            continue;   /* 未就绪（睡眠/无器件/该地址无应答） */
        }
        if (atecc_readprobe(cand[ci]) == 0) {
            continue;   /* 读也 ACK → 内存映射型（EEPROM 类），跳过，防误写 */
        }
        for (n = 0u; n < ATECC_PROBE_N; n++) {
            rc = DSO_Atecc608b_InfoRevision(rev);
            if (rc == 0) {
                return 0;   /* s_addr 已是找到的地址 */
            }
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
        }
    }
    return rc;
}

/* ---- 上电探测（一次性任务，打印后自删；无周期上报） ---- */

/* 只读探针：不带任何写、尝试读 1 字节。
 * 带内存映射的器件(EEPROM 等)任何时候 ACK 读；ATECC 空闲/无输出时读地址 NACK。 */
static int atecc_readprobe(uint8_t addr)
{
    uint8_t b;
    int rc;
    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_ReadRaw(addr, &b, 1u);
    BSP_I2C_UI_Unlock();
    return rc;
}

#if DSO_ATECC_DIAG_BATTERY
/* ==================== 以下为一次性诊断代码（判定电池 + 各臂） ====================
 * 用 DSO_ATECC_DIAG_BATTERY 整体开关：置 0 时本区间**不参与编译**
 * （只留 probe_task 的 #else 分支：唤醒 + 只读 Info + Config dump），
 * 既让开机干净，也避免一堆未被引用的 static 函数产生告警。
 * 2026-10-10 换新 ATECC608B-SSHDA-T 后已确认芯片工作，默认置 0。 */

/* 已知器件地址（不打标签跳过读探针）：PCAL6524 0x22、MCP23017 0x26/0x27、
 * INA226 0x40/0x41、SHT40 0x44、ATECC608B 0x60（0x35/0x36 已实测排除） */
static int atecc_is_known(uint8_t addr)
{
    return (addr == 0x22u || addr == 0x26u || addr == 0x27u ||
            addr == 0x35u || addr == 0x36u ||
            addr == 0x40u || addr == 0x41u || addr == 0x44u);
}

/* 扫描 **0x01-0x7F** 收集应答地址（Arm E 用）—— 原从 0x08 起，会漏掉
 * CBUS(0x01)/保留段(0x02-0x07)；0x00（通用呼叫）由 Arm J 单独覆盖。
 * woke=0：冷态基线，完全不唤醒 —— 即"普通 i2cdetect 扫描"，
 *         规范明确：Sleep 中的芯片在这条基线上不可见。
 * woke=1：每 ATECC_SCAN_BATCH 个地址在**同一持锁窗口**内重新 GPIO 唤醒一次 + 等 tWHI，
 *         使芯片在整个扫描期间保持醒态 —— 这样即使它的地址不是候选值也会被逮到。
 * @param bit_us 位延迟（µs 半周期）——慢档用于排除"芯片 ACK 慢、主控采样过早误判 NACK" */
#define ATECC_SCAN_BATCH   8u

static uint8_t atecc_scan_collect(uint8_t *out, uint8_t out_max, uint8_t woke,
                                  uint32_t bit_us, uint8_t batch)
{
    uint8_t addr;
    uint8_t n = 0u;
    uint8_t in_batch = 0u;

    if (batch < 1u) {
        batch = 1u;
    }
    for (addr = 0x01u; addr <= 0x7Fu; addr++) {
        if (in_batch == 0u) {
            BSP_I2C_UI_SetBitDelayUs(bit_us);        /* 本批用该档 */
            BSP_I2C_UI_Lock();                       /* 分批持锁（不做整段占锁） */
            if (woke != 0u) {
                (void)BSP_I2C_UI_WakePulseMeasUs(100u);   /* 唤醒 + 本批探针同窗口 */
                vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            }
        }
        if (BSP_I2C_UI_Probe(addr) == 0) {
            if (n < out_max) {
                out[n] = addr;
            }
            n++;
        }
        in_batch++;
        if (in_batch >= batch) {
            in_batch = 0u;
            BSP_I2C_UI_SetBitDelayUs(0u);            /* 先还原档位再解锁 */
            BSP_I2C_UI_Unlock();
        }
    }
    if (in_batch != 0u) {
        BSP_I2C_UI_SetBitDelayUs(0u);
        BSP_I2C_UI_Unlock();
    }
    return (n > out_max) ? out_max : n;
}

/* 打印一个地址列表（(k)=atecc_is_known 已知器件） */
static void atecc_diag_addrs(const char *tag, const uint8_t *l, uint8_t n)
{
    uint8_t i;
    DebugUartLock();
    printf("%s(%u):", tag, (unsigned)n);
    for (i = 0u; i < n; i++) {
        printf(" 0x%02X", (unsigned)l[i]);
        if (atecc_is_known(l[i])) {
            printf("(k)");
        }
    }
    printf("\r\n");
    DebugUartUnlock();
}

/* 打印差集（只在醒态出现的地址 = ATECC 候选）。@return 差集个数 */
static uint8_t atecc_diag_diff(const char *tag, const uint8_t *cold, uint8_t nc,
                               const uint8_t *woke, uint8_t nw)
{
    uint8_t i;
    uint8_t j;
    uint8_t found;
    uint8_t n = 0u;

    DebugUartLock();
    printf("%s:", tag);
    for (i = 0u; i < nw; i++) {
        found = 0u;
        for (j = 0u; j < nc; j++) {
            if (woke[i] == cold[j]) {
                found = 1u;
                break;
            }
        }
        if (found == 0u) {
            printf(" 0x%02X", (unsigned)woke[i]);
            n++;
        }
    }
    if (n == 0u) {
        printf(" (none)");
    }
    printf("\r\n");
    DebugUartUnlock();
    return n;
}

/* 唤醒窗口内一次性探完「候选 + 同窗口控制器件」。
 * 核心修正：唤醒与寻址必须在**同一持锁窗口**内完成（中途只等 tWHI）——把锁放开会让
 * SHT40/INA226 的 1s 轮询插进窗口，探针也会落到唤醒窗口之外（曾因此恒判 NACK）。
 * @param method  0=GPIO 脉冲法（SCL 保持高、无地址字节）；1=0x00 字节法（general call）
 * @param out_ack 长度 ≥6：0x60/0x35/0x36 候选 + 0x22/0x44/0x41 控制器件 的 ACK 位
 * @return method=0：实测低电平 µs；method=1：general-call 应答位
 */
static uint32_t atecc_wake_window(uint8_t method, uint8_t out_ack[6])
{
    static const uint8_t cand[6] = { 0x60u, 0x35u, 0x36u,   /* 候选 */
                                     0x22u, 0x44u, 0x41u }; /* 同窗口控制器件 */
    uint32_t ret;
    uint8_t i;

    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);   /* ≤100kHz，用完立刻还原 */
    BSP_I2C_UI_Lock();
    if (method == 0u) {
        ret = BSP_I2C_UI_WakePulseMeasUs(100u);
    } else {
        ret = (uint32_t)BSP_I2C_UI_WakeZeroBytes(DSO_ATECC_WAKE_ZB_N);
    }
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));       /* 持锁等 tWHI（≥1.5ms） */
    for (i = 0u; i < 6u; i++) {
        out_ack[i] = (uint8_t)((BSP_I2C_UI_Probe(cand[i]) == 0) ? 1u : 0u);
    }
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    return ret;
}

/* Arm C 用：唤醒 → 等 wait_ms → 探 addr（同窗口持锁） */
static uint8_t atecc_probe_after_wait(uint8_t addr, uint16_t wait_ms)
{
    uint8_t a;

    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_WakePulseMeasUs(100u);
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    a = (uint8_t)((BSP_I2C_UI_Probe(addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    return a;
}

/* Arm H 用：用 ATECC **自己的地址字节**做唤醒 —— CryptoAuthLib hal_i2c_wake 的写法：
 * 100kHz 下 START + [addr+W] + 0x00，靠地址字节/数据字节的连续 0 位凑出 ≥tWLO 的低电平。
 * 这是最后一种"文档记载的唤醒形态"（另一种是 GPIO 脉冲、以及 0x00 通用呼叫字节法）。
 * @param burst  0=单次；1=连发 10 次
 * @param addr_acked 输出：唤醒序列中**地址字节本身**是否被 ACK（醒着的芯片才会 ACK）
 * @return 1=唤醒后的地址探测被 ACK
 */
static uint8_t atecc_wake_own_addr(uint8_t addr, uint8_t burst,
                                   uint8_t *addr_acked)
{
    static const uint8_t z = 0x00u;
    uint8_t acklog[4];
    uint8_t aa = 0u;
    uint8_t n = (burst != 0u) ? 10u : 1u;
    uint8_t r;
    uint8_t i;

    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
    BSP_I2C_UI_Lock();
    for (i = 0u; i < n; i++) {
        if (BSP_I2C_UI_DiagTxBytes(addr, &z, 1u, acklog) == 0) {
            aa = 1u;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    r = (uint8_t)((BSP_I2C_UI_Probe(addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    if (addr_acked != NULL) {
        *addr_acked = aa;
    }
    return r;
}

/* Arm D 用：指定位延迟（µs 半周期）下唤醒并探 addr（同窗口持锁） */
static uint8_t atecc_probe_at_rate(uint8_t addr, uint32_t bit_us)
{
    uint8_t a;

    BSP_I2C_UI_SetBitDelayUs(bit_us);
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_WakePulseMeasUs(100u);
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    a = (uint8_t)((BSP_I2C_UI_Probe(addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    return a;
}

/* ---- 分层判定实验（Arm 0/A/B/H/G/C/D/E/F）：全程**只读** ----
 * 只用 地址探测 / Info / Read；无 Write、无 Lock、无 OTP（调试红线）。
 * 每臂只回答一个问题、输出一行，最后一行是总结论。
 * @return 探到的 ATECC 候选地址（0 = 未探到）
 */
static uint8_t atecc608b_diag_battery(void)
{
    static const uint8_t cand[3] = { 0x60u, 0x35u, 0x36u };
    static const uint16_t wms[3] = { 2u, 20u, 50u };      /* Arm C 等待档 */
    static const uint32_t bds[4] = { 5u, 10u, 30u, 60u }; /* Arm D 位延迟档 */
#if DSO_ATECC_DIAG_ARM_F
    static const uint32_t pws[5] = { 60u, 100u, 250u, 1000u, 5000u };
#endif
    uint8_t ack[6];
    uint8_t cold[16];
    uint8_t woke[16];
    uint8_t woke8[16];
    uint8_t nc = 0u;
    uint8_t nw = 0u;
    uint8_t nw8 = 0u;
    uint8_t nd = 0u;
    uint8_t nd8 = 0u;
    uint8_t r;
    uint8_t i;
    uint8_t a;
    uint8_t j00 = 0u;       /* Arm J：通用呼叫地址 0x00 是否给出 ATECC 型响应 */
    uint8_t hit = 0u;       /* 命中的候选地址 */
    uint8_t ctrl_ok = 0u;   /* 同窗口控制器件是否应答 */
    uint8_t sda;
    uint8_t scl;
    uint8_t sda2;
    uint8_t scl2;
    uint8_t sd3[3];
    uint8_t aa;
    uint32_t meas;

    /* ---- Arm 0：上电事实核对 + 冷态基线 ----
     * 顺序有讲究：冷态基线必须排在**任何可能唤醒芯片的动作之前**。
     * BSP_I2C_UI_SdaLowProbe() 会把 SDA 拉低 120µs（≥tWLO 60µs）——它本身就是一次
     * 唤醒脉冲，故只能排在冷态扫描之后；BusRecover 的 9 个 SCL 脉冲不含长低电平，
     * 无唤醒作用（只把可能卡低的从机放开）。 */
    BSP_I2C_UI_Lock();
    BSP_I2C_UI_IdleLevels(&sda, &scl);
    BSP_I2C_UI_Unlock();

    /* 冷态基线（Arm E 的对照项）：此时未发过任何唤醒脉冲 —— 即规范点名的
     * "上电延时后跑普通 i2cdetect 扫描"，Sleep 中的芯片对地址一律 NACK。 */
    nc = atecc_scan_collect(cold, (uint8_t)sizeof(cold), 0u,
                            DSO_ATECC_SCAN_FAST_US, 8u);
    atecc_diag_addrs("[ATECC-0 ] cold(no wake yet)", cold, nc);

    /* 这三项测量必须持锁：电池延后 1.2s 后 SHT40/INA226 的 1s 轮询已在跑，
     * 不持锁会被别的任务在同一根线上交错驱动，读出假值（曾读到 sda_drive=0，
     * 与 idle SDA=1 及同轮三个器件 ACK 自相矛盾）。sda_drive 连测 3 次便于识别偶发。 */
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_BusRecover(&sda2, &scl2);
    for (i = 0u; i < 3u; i++) {
        sd3[i] = (uint8_t)BSP_I2C_UI_SdaLowProbe();
    }
    BSP_I2C_UI_Unlock();
    DebugUartLock();
    printf("[ATECC-0 ] por=%ums(need>=20) idle SDA=%u SCL=%u | recover SDA=%u SCL=%u "
           "sda_drive=%u/%u/%u id_bit=%uus(ident rate)\r\n",
           (unsigned)HAL_GetTick(), (unsigned)sda, (unsigned)scl,
           (unsigned)sda2, (unsigned)scl2,
           (unsigned)sd3[0], (unsigned)sd3[1], (unsigned)sd3[2],
           (unsigned)DSO_ATECC_BIT_DELAY_US);
    DebugUartUnlock();

    /* ---- Arm A：GPIO 脉冲法 + 唤醒窗口内一次性探完候选与控制器件（核心修正） ---- */
    for (r = 0u; r < DSO_ATECC_DIAG_ROUNDS; r++) {
        meas = atecc_wake_window(0u, ack);
        for (i = 0u; i < 3u; i++) {
            if (ack[i] != 0u && hit == 0u) {
                hit = cand[i];
            }
        }
        if (ack[3] != 0u || ack[4] != 0u || ack[5] != 0u) {
            ctrl_ok = 1u;
        }
        DebugUartLock();
        printf("[ATECC-A%u] gpio-pulse meas=%uus(need>=60) 0x60=%s 0x35=%s 0x36=%s "
               "| ctrl 0x22=%s 0x44=%s 0x41=%s\r\n",
               (unsigned)(r + 1u), (unsigned)meas,
               ack[0] ? "ACK" : "NACK", ack[1] ? "ACK" : "NACK", ack[2] ? "ACK" : "NACK",
               ack[3] ? "ACK" : "NACK", ack[4] ? "ACK" : "NACK", ack[5] ? "ACK" : "NACK");
        DebugUartUnlock();
    }

    /* ---- Arm B：0x00 字节法（general call）对照 ---- */
    meas = atecc_wake_window(1u, ack);
    for (i = 0u; i < 3u; i++) {
        if (ack[i] != 0u && hit == 0u) {
            hit = cand[i];
        }
    }
    if (ack[3] != 0u || ack[4] != 0u || ack[5] != 0u) {
        ctrl_ok = 1u;
    }
    DebugUartLock();
    printf("[ATECC-B ] zb0x00x%u gc_ack=%u(general-call ack, NOT a wake indicator) "
           "0x60=%s 0x35=%s 0x36=%s | ctrl 0x22=%s 0x44=%s 0x41=%s\r\n",
           (unsigned)DSO_ATECC_WAKE_ZB_N, (unsigned)meas,
           ack[0] ? "ACK" : "NACK", ack[1] ? "ACK" : "NACK", ack[2] ? "ACK" : "NACK",
           ack[3] ? "ACK" : "NACK", ack[4] ? "ACK" : "NACK", ack[5] ? "ACK" : "NACK");
    DebugUartUnlock();

    /* ---- Arm H：用 ATECC 自己的地址字节唤醒（单次 / 连发 10 次）----
     * 地址字节 ACK 与否本身也是判据：醒着的芯片会 ACK 自己的地址。 */
    a = atecc_wake_own_addr(s_addr, 0u, &aa);
    ack[0] = a;
    ack[1] = aa;
    a = atecc_wake_own_addr(s_addr, 1u, &aa);
    if ((ack[0] != 0u || a != 0u) && hit == 0u) {
        hit = s_addr;
    }
    DebugUartLock();
    printf("[ATECC-H ] own-addr wake(0x%02X): x1 probe=%s(addr_byte=%s) "
           "x10 probe=%s(addr_byte=%s)\r\n",
           (unsigned)s_addr,
           ack[0] ? "ACK" : "NACK", ack[1] ? "ACK" : "NACK",
           a ? "ACK" : "NACK", aa ? "ACK" : "NACK");
    DebugUartUnlock();

    /* ---- Arm G：信标波形 + ACK 窗口实测 + 多采样复探 ----
     * 背景更正：示波器一度报"第 9 个时钟 SDA 被拉低"，但本臂与 Arm I 的实测一致表明
     * ——ACK 窗口内 SDA 一次都没低，而**同一帧内**我们自己拉的低读得回来
     * （ctrl_self_test=OK）→ 芯片确实不应答；此前观测应为 0xC0 的 6 个 0 位
     * （0xC0=1100_0000，SCL 翻转期间 SDA 持续为低，肉眼极像 ACK）或控制器件真实的 ACK。
     * 本臂同时给示波器一段干净可判定的波形：500µs 标记 + 32 个相同的 0xC0 字节。 */
    {
        uint8_t n_ack = 0u;
        uint8_t n_ms = 0u;
        uint8_t n_low = 0u;
        uint8_t ctrl_ok = 0u;
        uint32_t r2l = 0xFFFFFFFFu;
        uint32_t lows = 0u;
        uint32_t nsmp = 0u;
        uint8_t tail = 0u;
        BSP_I2C_UI_ProbeTiming tm;

        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        /* G1 信标：持锁 → 500µs 标记脉冲（= 一次合规唤醒，兼示波器触发特征）
         * → 等 tWHI → 背靠背探 0x60 x N（首帧紧跟一次真唤醒；不逐帧唤醒，波形才干净） */
        BSP_I2C_UI_Lock();
        BSP_I2C_UI_MarkerPulse(500u);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        for (i = 0u; i < DSO_ATECC_BEACON_N; i++) {
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                n_ack++;
            }
        }
        BSP_I2C_UI_Unlock();

        /* G2 计时探针：唤醒+等待+探测一体（同时实测 tWLO/tWHI），跑 3 次取末次 */
        for (i = 0u; i < 3u; i++) {
            BSP_I2C_UI_Lock();
            (void)BSP_I2C_UI_WakeProbeTimed(0x60u, 100u,
                                            DSO_ATECC_TWHI_MS * 1000u, &tm);
            BSP_I2C_UI_Unlock();
            r2l = tm.rise2low_ns;
            lows = tm.low_ns;
            nsmp = tm.n_samples;
            tail = tm.tail_low;
            if (r2l != 0xFFFFFFFFu) {
                n_low++;
            }
            if (tm.ctrl_low != 0u) {
                ctrl_ok = 1u;
            }
        }

        /* G3 多采样复探：判据改为"高电平期内任一次为低即 ACK"，同样逐帧唤醒 */
        BSP_I2C_UI_SetAckMultiSample(1u);
        BSP_I2C_UI_Lock();
        for (i = 0u; i < DSO_ATECC_BEACON_N; i++) {
            (void)BSP_I2C_UI_WakePulseMeasUs(100u);
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                n_ms++;
            }
        }
        BSP_I2C_UI_Unlock();
        BSP_I2C_UI_SetAckMultiSample(0u);
        BSP_I2C_UI_SetBitDelayUs(0u);

        if (n_ms != 0u && hit == 0u) {
            hit = 0x60u;
        }
        DebugUartLock();
        printf("[ATECC-G ] beacon 0x60 x%u: ACK=%u/%u (first frame after 500us wake)\r\n",
               (unsigned)DSO_ATECC_BEACON_N, (unsigned)n_ack,
               (unsigned)DSO_ATECC_BEACON_N);
        printf("[ATECC-G ] ack-timed 0x60 x3: low_seen=%u/3 samples=%lu rise2low=%luns "
               "low_width=%luns tail=%s ctrl_self_test=%s\r\n",
               (unsigned)n_low, (unsigned long)nsmp, (unsigned long)r2l,
               (unsigned long)lows, tail ? "LOW(=ACK)" : "HIGH(=NACK)",
               (ctrl_ok != 0u) ? "OK(read our own low)" : "FAIL");
        printf("[ATECC-G ] multisa 0x60 x%u (wake each): ACK=%u/%u -> %s\r\n",
               (unsigned)DSO_ATECC_BEACON_N, (unsigned)n_ms,
               (unsigned)DSO_ATECC_BEACON_N,
               (n_ms != 0u) ? "FIXED: ACK sampling was the bug"
                            : "still NACK (see ack-timed + scope)");
        DebugUartUnlock();
    }

    /* ---- Arm I：规范时序合规报告（tWLO ≥60µs，其后 tWHI ≥1500µs）----
     * 用户要求核查的两项都在**同一帧内 DWT 实测**：低电平实际宽度 + 唤醒释放到地址帧
     * 首个边沿的实际间隔。四组参数含"正好卡规格边界"的组合；判定与打印同源于头文件
     * 的 DSO_ATECC_TWLO_MIN_US / DSO_ATECC_TWHI_MIN_US。全程只读。 */
    {
        static const uint32_t lo_tab[4] = { 60u,  100u, 100u, 300u  };
        static const uint32_t hi_tab[4] = { 1500u, 1500u, 2000u, 5000u };
        uint8_t n_ack = 0u;
        uint8_t lo_pass = 0u;
        uint8_t hi_pass = 0u;
        uint8_t a;
        BSP_I2C_UI_ProbeTiming tm;

        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        DebugUartLock();
        printf("[ATECC-I ] spec: tWLO>=%uus then tWHI>=%uus (both measured in-frame by DWT)\r\n",
               (unsigned)DSO_ATECC_TWLO_MIN_US, (unsigned)DSO_ATECC_TWHI_MIN_US);
        DebugUartUnlock();
        for (i = 0u; i < 4u; i++) {
            BSP_I2C_UI_Lock();
            a = (uint8_t)((BSP_I2C_UI_WakeProbeTimed(0x60u, lo_tab[i], hi_tab[i],
                                                    &tm) == 0) ? 1u : 0u);
            BSP_I2C_UI_Unlock();
            if (a != 0u) {
                n_ack++;
                if (hit == 0u) {
                    hit = 0x60u;
                }
            }
            if (tm.t_lo_us >= DSO_ATECC_TWLO_MIN_US) { lo_pass++; }
            if (tm.t_hi_us >= DSO_ATECC_TWHI_MIN_US) { hi_pass++; }
            DebugUartLock();
            printf("[ATECC-I ] #%u lo=%uus hi=%uus -> tWLO=%luus(%s) tWHI=%luus(%s) "
                   "win_samples=%lu tail=%s ctrl=%s ack=%s%s\r\n",
                   (unsigned)(i + 1u), (unsigned)lo_tab[i], (unsigned)hi_tab[i],
                   (unsigned long)tm.t_lo_us,
                   (tm.t_lo_us >= DSO_ATECC_TWLO_MIN_US) ? "PASS" : "FAIL",
                   (unsigned long)tm.t_hi_us,
                   (tm.t_hi_us >= DSO_ATECC_TWHI_MIN_US) ? "PASS" : "FAIL",
                   (unsigned long)tm.n_samples,
                   tm.tail_low ? "LOW" : "HIGH",
                   tm.ctrl_low ? "OK" : "FAIL",
                   a ? "ACK" : "NACK",
                   (i == 2u) ? "  <- current impl 100/2000" : "");
            DebugUartUnlock();
        }
        BSP_I2C_UI_SetBitDelayUs(0u);
        DebugUartLock();
        printf("[ATECC-I ] timing verdict: tWLO %u/4 PASS  tWHI %u/4 PASS  ack=%u/4 -> %s\r\n",
               (unsigned)lo_pass, (unsigned)hi_pass, (unsigned)n_ack,
               ((lo_pass == 4u) && (hi_pass == 4u))
                   ? "spec timing SATISFIED (not the cause)"
                   : "spec timing VIOLATED -> fix the waits");
        DebugUartUnlock();
    }

    /* ---- Arm J：通用呼叫地址 0x00 专项探测（补上最后一块覆盖缺口）----
     * 事实：我们的全地址扫描从 **0x08 起**，0x00 从未被当作地址探过；而 Arm B 观测到
     * `gc_ack=1`（有器件应答通用呼叫）—— 这此前被**归因**于 INA226，但从未证实。
     * 若芯片的 I2C 地址被改写过（使用说明：地址"可改 1 次"），它可能就在 0x00。
     * 判据：把 s_addr 临时设为 0x00，跑一遍**完整只读** ATECC 命令（Info）——
     * 真 ATECC 会给出合法的 Count+数据+CRC16(0xA001) 响应帧，其它器件给不出。
     * 只读：无配置区写、无 Lock、无 OTP（0x03/0x07/0x30/0x00 等字节也不会触发
     * INA226 的通用呼叫专用动作——那些只认 0x04/0x06）。 */
    {
        uint8_t rev4[4] = { 0u, 0u, 0u, 0u };
        uint8_t old_addr = s_addr;
        int jrc;

        s_addr = 0x00u;
        jrc = DSO_Atecc608b_InfoRevision(rev4);
        DebugUartLock();
        printf("[ATECC-J ] addr 0x00 (general call, last coverage gap): Info rc=%d "
               "st=0x%02X rev=%02X%02X%02X%02X -> %s\r\n",
               jrc, (unsigned)DSO_Atecc608b_LastStatus(),
               (unsigned)rev4[0], (unsigned)rev4[1],
               (unsigned)rev4[2], (unsigned)rev4[3],
               (jrc == 0) ? "ATECC-LIKE RESPONSE AT 0x00" : "no ATECC response");
        DebugUartUnlock();
        if (jrc != 0) {
            s_addr = old_addr;      /* 未命中：还原工作地址 */
        } else {
            j00 = 1u;               /* 命中：地址 0x00 上有 ATECC 型响应 */
        }
    }

    /* ---- Arm K：0x60 / 0x35 走**完整命令流**（与 Arm J 对 0x00 同款方法）----
     * 此前 0x60/0x35/0x36 只用"地址字节是否 ACK"判定；本臂把**完整的 Info 命令组**
     * （word 0x03 + Count/Op/P1/P2 + CRC16，CRC 现场计算）发给它们：
     *   K1 逐字节记录 ACK 掩码（DiagTxBytes 即使在地址 NACK 后也继续发完，故能看出
     *      "死在哪一位"）；K2 再走官方驱动路径 DSO_Atecc608b_Exec（含内部唤醒、
     *      命令组、读轮询、响应 CRC 校验），成功则直接拿到 4 字节 RevNum。
     * 全程只读（Info 是纯读命令）——无 Write、无 Lock、无 OTP。 */
    {
        static const uint8_t kaddrs[2] = { 0x60u, 0x35u };
        uint8_t kframe[8];
        uint8_t kack[10];
        uint8_t krev[4];
        uint16_t kcrc;
        uint8_t ka;
        int krc;

        /* 与 atecc_exec_body 的 Info(Revision) 帧同构；CRC 现场算，避免魔数 */
        kframe[0] = DSO_ATECC608B_WORD_CMD;   /* 0x03 */
        kframe[1] = 7u;                       /* Count = 7 + 0 数据 */
        kframe[2] = ATECC_OP_INFO;            /* 0x30 */
        kframe[3] = 0x00u;                    /* P1 = Revision */
        kframe[4] = 0x00u;                    /* P2 lo */
        kframe[5] = 0x00u;                    /* P2 hi */
        kcrc = atecc_crc16(&kframe[1], 5u);   /* CRC 覆盖 Count..P2hi（5 字节） */
        kframe[6] = (uint8_t)(kcrc & 0xFFu);
        kframe[7] = (uint8_t)(kcrc >> 8);

        for (ka = 0u; ka < 2u; ka++) {
            /* K1 逐字节 ACK 掩码（按会话档 + 多点采样判 ACK） */
            BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);
            BSP_I2C_UI_Lock();
            (void)BSP_I2C_UI_WakePulseMeasUs(100u);
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            krc = BSP_I2C_UI_DiagTxBytes(kaddrs[ka], kframe, 8u, kack);
            BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
            BSP_I2C_UI_Unlock();
            DebugUartLock();
            printf("[ATECC-K ] 0x%02X write-frame(9B) rc=%d ack[addr=%u w03=%u cnt=%u op=%u "
                   "p1=%u p2l=%u p2h=%u crcl=%u crch=%u]\r\n",
                   (unsigned)kaddrs[ka], krc,
                   (unsigned)kack[0], (unsigned)kack[1], (unsigned)kack[2],
                   (unsigned)kack[3], (unsigned)kack[4], (unsigned)kack[5],
                   (unsigned)kack[6], (unsigned)kack[7], (unsigned)kack[8]);
            DebugUartUnlock();

            /* K2 完整命令流：先 GPIO 脉冲唤醒，再走官方驱动路径（内部再唤醒一次、
             * 组包、读轮询、响应 CRC 校验）——这就是"真正读芯片"的那条路 */
            krev[0] = 0u; krev[1] = 0u; krev[2] = 0u; krev[3] = 0u;
            BSP_I2C_UI_Lock();
            (void)BSP_I2C_UI_WakePulseMeasUs(100u);
            BSP_I2C_UI_Unlock();
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            s_addr = kaddrs[ka];
            krc = DSO_Atecc608b_InfoRevision(krev);
            DebugUartLock();
            printf("[ATECC-K ] 0x%02X full-flow Info: rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
                   (unsigned)kaddrs[ka], krc,
                   (unsigned)DSO_Atecc608b_LastStatus(),
                   (unsigned)krev[0], (unsigned)krev[1],
                   (unsigned)krev[2], (unsigned)krev[3],
                   (krc == 0) ? "<ATECC608B RESPONDED!>" : "(no response)");
            DebugUartUnlock();
            if (krc == 0) {
                hit = kaddrs[ka];
            }
        }
        s_addr = DSO_ATECC608B_I2C_ADDR;
    }

    /* ---- Arm L：逐条落实用户四方案（L1 已在任务最早时机执行）----
     * 方案 2/3c 已实现（GPIO 脉冲唤醒、General Call 0x00，见 Arm A/B）；
     * 这里补方案 3a/3b/4 的三个**未做过的变体**：
     *   L2 = 0x00 法降到 25kHz（20µs 半周期，满足"≤50kHz 做 Wake"）
     *   L3 = 0x00 法**不发 STOP**（正确实现：停在 SCL 低→先释放 SDA→再抬 SCL）
     *   L4 = 一次唤醒 + 2ms 间隔重试 5 次（GPIO 法 / 0x00 法各一组）
     * 全程只读。 */
    {
        uint8_t l2_ack;
        uint8_t l3_ack;
        uint8_t l4_gpio = 0u;
        uint8_t l4_zb = 0u;
        uint8_t lr;

        /* L2 */
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakeZeroBytesAt(3u, 20u, 1u);      /* 25kHz + 标准 STOP */
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        l2_ack = (uint8_t)((BSP_I2C_UI_Probe(0x60u) == 0) ? 1u : 0u);
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();

        /* L3 */
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakeZeroBytesAt(3u, 20u, 0u);      /* 25kHz + 不发 STOP */
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        l3_ack = (uint8_t)((BSP_I2C_UI_Probe(0x60u) == 0) ? 1u : 0u);
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();

        /* L4a：GPIO 唤醒一次 → 2ms 间隔重试 5 次 */
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakePulseMeasUs(100u);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        for (lr = 0u; lr < 5u; lr++) {
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                l4_gpio = (uint8_t)(lr + 1u);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2u));
        }
        BSP_I2C_UI_Unlock();

        /* L4b：0x00 法唤醒一次 → 2ms 间隔重试 5 次 */
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakeZeroBytesAt(3u, I2C_UI_SLOW_HALF_DELAY_US, 1u);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        for (lr = 0u; lr < 5u; lr++) {
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                l4_zb = (uint8_t)(lr + 1u);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2u));
        }
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();

        if ((l2_ack | l3_ack | l4_gpio | l4_zb) != 0u && hit == 0u) {
            hit = 0x60u;
        }
        DebugUartLock();
        printf("[ATECC-L2] 0x00 x3 @25kHz stop=1  -> 0x60=%s\r\n",
               l2_ack ? "ACK" : "NACK");
        printf("[ATECC-L3] 0x00 x3 @25kHz stop=0 (no STOP) -> 0x60=%s\r\n",
               l3_ack ? "ACK" : "NACK");
        printf("[ATECC-L4] one wake then retry5 @2ms: gpio=%s zb0x00=%s\r\n",
               l4_gpio ? "ACK" : "NACK(5/5)", l4_zb ? "ACK" : "NACK(5/5)");
        DebugUartUnlock();
    }

    /* ---- Arm M：**逻辑分析仪判定夹具**（专为拍摄优化，裁定"硬件 vs 软件"）----
     * 思路：让同一屏里同时出现"嫌疑器件"与"必然应答的对照器件"，用独立仪表的
     * I2C 协议解码器读数：
     *   解码 0x60=NACK 而 0x44/0x41=ACK → 位时序/速率/判据/总线/锁全部无罪 → 器件问题；
     *   解码 0x60=ACK 而固件报 NACK   → 我们采样读错 → 软件问题。
     * 波形：1ms 的 SDA 低电平标记（唯一触发特征，比 Arm G 的 500µs 标记更长）
     *   → 等 tWHI → 依次探 0x60 / 0x35 / 0x44 / 0x41 → 重复 3 轮；
     *   全程持锁、探针跑在 25kHz 解码友好档（标记本身不涉时钟，与档位无关）。只读。 */
    {
        static const uint8_t maddrs[4] = { 0x60u, 0x35u, 0x44u, 0x41u };
        uint8_t mack[4];
        uint8_t msum[4] = { 0u, 0u, 0u, 0u };
        uint8_t mround;
        uint8_t mi;

        for (mround = 0u; mround < 3u; mround++) {
            BSP_I2C_UI_Lock();
            BSP_I2C_UI_MarkerPulse(1000u);              /* 1ms：唯一触发特征 */
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_LA_SLOW_US);   /* 25kHz，便于解码 */
            for (mi = 0u; mi < 4u; mi++) {
                mack[mi] = (uint8_t)((BSP_I2C_UI_Probe(maddrs[mi]) == 0) ? 1u : 0u);
                if (mack[mi] != 0u) {
                    msum[mi]++;
                }
            }
            BSP_I2C_UI_SetBitDelayUs(0u);            /* 先还原档位再解锁 */
            BSP_I2C_UI_Unlock();
            DebugUartLock();
            printf("[ATECC-M%u] 0x60=%s 0x35=%s | ctrl 0x44=%s 0x41=%s (expect: NACK NACK ACK ACK)\r\n",
                   (unsigned)(mround + 1u),
                   mack[0] ? "ACK" : "NACK", mack[1] ? "ACK" : "NACK",
                   mack[2] ? "ACK" : "NACK", mack[3] ? "ACK" : "NACK");
            DebugUartUnlock();
        }

        if ((msum[0] | msum[1]) != 0u && hit == 0u) {
            hit = (msum[0] != 0u) ? 0x60u : 0x35u;
        }
        DebugUartLock();
        printf("[ATECC-M ] LA fixture: 3 rounds, 1ms marker each; decode the SAME capture: "
               "0x60/0x35=NACK + 0x44/0x41=ACK -> hardware; 0x60=ACK -> our sampling\r\n");
        DebugUartUnlock();
    }

    /* ---- Arm C：Wake→寻址 等待档（是否只是 tWHI 窗口不对） ---- */
    DebugUartLock();
    printf("[ATECC-C ] tWHI sweep(0x%02X):", (unsigned)s_addr);
    DebugUartUnlock();
    for (i = 0u; i < 3u; i++) {
        a = atecc_probe_after_wait(s_addr, wms[i]);
        if (a != 0u && hit == 0u) {
            hit = s_addr;
        }
        DebugUartLock();
        printf(" %ums=%s", (unsigned)wms[i], a ? "ACK" : "NACK");
        DebugUartUnlock();
    }
    DebugUartLock();
    printf("\r\n");
    DebugUartUnlock();

    /* ---- Arm D：位速率档（是否芯片 ACK 慢、主控采样过早误判 NACK） ---- */
    DebugUartLock();
    printf("[ATECC-D ] bitrate sweep:");
    DebugUartUnlock();
    for (i = 0u; i < 4u; i++) {
        a  = atecc_probe_at_rate(0x60u, bds[i]);
        if (a != 0u && hit == 0u) {
            hit = 0x60u;
        }
        ack[0] = a;
        a = atecc_probe_at_rate(0x35u, bds[i]);
        if (a != 0u && hit == 0u) {
            hit = 0x35u;
        }
        DebugUartLock();
        printf(" %uus:0x60=%s/0x35=%s", (unsigned)bds[i],
               ack[0] ? "ACK" : "NACK", a ? "ACK" : "NACK");
        DebugUartUnlock();
    }
    DebugUartLock();
    printf("\r\n");
    DebugUartUnlock();

    /* ---- Arm E：差分全地址扫描（醒态 @ 识别档 + 慢档，与 Arm 0 冷态基线相比）----
     * 差集里的地址 = 只可能出现在醒态的器件 = ATECC 候选（地址非候选值也能逮到）。
     * 只在**没有候选应答**时才跑：已确定在 0x60 就没必要再花 ~2.5s 扫全地址。
     * （本档位已降到 2kHz，扫描很慢；这是判定电池里最贵的一臂。） */
    if (hit == 0u) {
        /* 快档 + 2 地址/唤醒：整批 ~0.3ms，稳稳落在唤醒窗口内（原用 500µs 识别档，
         * 8 个地址一批要 ~80ms 才探完，若芯片只在唤醒后短时可访问整批都会被漏掉）。 */
        nw  = atecc_scan_collect(woke, (uint8_t)sizeof(woke), 1u,
                                 DSO_ATECC_SCAN_FAST_US, 2u);
        nw8 = atecc_scan_collect(woke8, (uint8_t)sizeof(woke8), 1u,
                                 DSO_ATECC_SCAN_SLOW_US, 4u);
        atecc_diag_addrs("[ATECC-E ] woke@fast(2/wake)", woke, nw);
        atecc_diag_addrs("[ATECC-E ] woke@8kHz(4/wake)", woke8, nw8);
        nd  = atecc_diag_diff("[ATECC-E ] diff@fast(woke-cold)", cold, nc, woke, nw);
        nd8 = atecc_diag_diff("[ATECC-E ] diff@8kHz(woke-cold)", cold, nc, woke8, nw8);
    } else {
        DebugUartLock();
        printf("[ATECC-E ] skipped: dev already answered at 0x%02X\r\n", (unsigned)hit);
        DebugUartUnlock();
    }

#if DSO_ATECC_DIAG_ARM_F
    /* ---- Arm F：唤醒脉冲宽度扫描 + 总线恢复后重试 ---- */
    DebugUartLock();
    printf("[ATECC-F ] pulse-width sweep(0x%02X):", (unsigned)s_addr);
    DebugUartUnlock();
    for (i = 0u; i < 5u; i++) {
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakePulseMeasUs(pws[i]);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));   /* 本臂用更宽的等待 */
        a = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();
        if (a != 0u && hit == 0u) {
            hit = s_addr;
        }
        DebugUartLock();
        printf(" %uus=%s", (unsigned)pws[i], a ? "ACK" : "NACK");
        DebugUartUnlock();
    }
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
    BSP_I2C_UI_Lock();                       /* 恢复+唤醒+探针同一持锁窗口 */
    (void)BSP_I2C_UI_BusRecover(&sda2, &scl2);
    (void)BSP_I2C_UI_WakePulseMeasUs(100u);
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
    a = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    if (a != 0u && hit == 0u) {
        hit = s_addr;
    }
    DebugUartLock();
    printf(" | recover+100us=%s (SDA=%u SCL=%u)\r\n", a ? "ACK" : "NACK",
           (unsigned)sda2, (unsigned)scl2);
    DebugUartUnlock();
#endif

    /* ---- 结论行（5 个互斥结论，指向下一步动作） ---- */
    DebugUartLock();
    if (j00 != 0u) {
        printf("[ATECC-VERDICT] ATECC-like response at general-call addr 0x00 -> "
               "set DSO_ATECC608B_I2C_ADDR to 0x00 and re-run\r\n");
    } else if (hit != 0u) {
        printf("[ATECC-VERDICT] dev ACK at 0x%02X -> wake OK; go read-only Info/Config\r\n",
               (unsigned)hit);
    } else if ((nd + nd8) != 0u) {
        printf("[ATECC-VERDICT] dev maybe at diff addr (%u, see Arm E last 2 lines) -> "
               "set DSO_ATECC608B_I2C_ADDR to it and re-run\r\n",
               (unsigned)(nd + nd8));
    } else if (ctrl_ok != 0u) {
        printf("[ATECC-VERDICT] ctrl=OK dev=NONE -> firmware side CLOSED "
               "(addr 0x00+0x08-0x7F, 4 rates, 3 wake forms, spec timing tWLO/tWHI PASS, "
               "same-frame ctrl self-test OK): chip does not answer -> replace part / "
               "contact supplier\r\n");
    } else {
        printf("[ATECC-VERDICT] ctrl=NONE -> bus-level fault (pull-up / stuck low / "
               "bit timing), fix the bus first\r\n");
    }
    DebugUartUnlock();

    return hit;
}

/* Arm L1 —— **用户方案 1 逐字复刻**，且刻意安排在上电后**最早**时机执行
 * （POR 等待之后、1.2s 静默等待之前）：
 *   tPU 1ms → GPIO 唤醒 80µs（>tWLO 60µs）→ 等 tWHI 2ms → **立即**探 0x60；
 *   NACK 则间隔 5ms 重试，共 3 次（方案 4 的"唤醒后重试"也一并落在窗口内）。
 * 全程只读（仅地址探测）。@return 1=0x60 应答 */
static uint8_t atecc_diag_l1_early(void)
{
    uint8_t ack = 0u;
    uint8_t r;
    uint32_t meas;

    vTaskDelay(pdMS_TO_TICKS(1u));                        /* tPU 裕量（方案 1 要求 ≥1ms） */
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);   /* 16kHz，低于方案 3 的 50kHz 上限 */
    BSP_I2C_UI_Lock();
    meas = BSP_I2C_UI_WakePulseMeasUs(80u);               /* GPIO 直接控 SDA 拉低 80µs */
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));         /* tWHI 2ms（≥1.5ms） */
    for (r = 0u; r < 3u; r++) {
        if (BSP_I2C_UI_Probe(0x60u) == 0) {
            ack = 1u;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5u));                    /* 第一次可能 NACK → 5ms 后重试 */
    }
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();

    DebugUartLock();
    printf("[ATECC-L1] tPU=1ms wake=%uus(>60) tWHI=%ums retry3@5ms -> 0x60=%s (round %u/3)\r\n",
           (unsigned)meas, (unsigned)DSO_ATECC_TWHI_MS,
           ack ? "ACK" : "NACK", ack ? (unsigned)(r + 1u) : 3u);
    DebugUartUnlock();
    return ack;
}

#endif /* DSO_ATECC_DIAG_BATTERY */

/* ==================== 授权/设备身份（Stage 1，**非破坏**） ====================
 * 依据 Markdown/STM32H743 + ATECC608B-SSHDA-T + LCMXO2 FPGA 远程学生设备授权体系.md。
 * 铁律：**不写配置区、不写数据槽、不 Lock、不碰 OTP**。本区只有两类动作：
 *   ① 只读（Info / Read / 状态与 SlotConfig 解读）；
 *   ② 片内签名与验签（Nonce / Sign / Verify —— 私钥永不出片）。
 * 唯一例外：调用方显式 allow_genkey 时的 GenKey（数据区新建密钥，可反复重生成，
 * 非破坏），以及取公钥用的 GenKey Mode 0x00（只算公钥、不改私钥，见下）。
 * 编码约定见文件头：printf 字符串一律 ASCII。 */

static DSO_Atecc608b_Status s_status = { 0u, 0u, 0u, {0u,0u,0u,0u}, {0u,0u,0u,0u}, 0u, 0u };

/* ---- 小工具 ---- */

void DSO_Atecc608b_HexUpper(const uint8_t *b, uint16_t n, char *out)
{
    static const char *H = "0123456789ABCDEF";
    uint16_t i;
    for (i = 0u; i < n; i++) {
        out[2u * i] = H[(b[i] >> 4) & 0x0Fu];
        out[2u * i + 1u] = H[b[i] & 0x0Fu];
    }
    out[2u * n] = '\0';
}

/* STM32H743 96-bit UID（本工程 CMSIS 副本没有 UID_BASE，就地从地址读） */
#define DSO_UID_BASE   0x1FF1E800u
static void atecc_uid96(uint8_t out[12])
{
    const volatile uint32_t *p = (const volatile uint32_t *)DSO_UID_BASE;
    uint32_t w0 = p[0];
    uint32_t w1 = p[1];
    uint32_t w2 = p[2];
    out[0]  = (uint8_t)(w0 >> 24); out[1]  = (uint8_t)(w0 >> 16);
    out[2]  = (uint8_t)(w0 >> 8);  out[3]  = (uint8_t)w0;
    out[4]  = (uint8_t)(w1 >> 24); out[5]  = (uint8_t)(w1 >> 16);
    out[6]  = (uint8_t)(w1 >> 8);  out[7]  = (uint8_t)w1;
    out[8]  = (uint8_t)(w2 >> 24); out[9]  = (uint8_t)(w2 >> 16);
    out[10] = (uint8_t)(w2 >> 8);  out[11] = (uint8_t)w2;
}

/* CRC32（IEEE 802.3，无表）：把 96-bit UID 混成 8 个十六进制字符 */
static uint32_t atecc_crc32(const uint8_t *d, uint16_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint16_t i;
    uint8_t j;
    for (i = 0u; i < n; i++) {
        crc ^= (uint32_t)d[i];
        for (j = 0u; j < 8u; j++) {
            crc = ((crc & 1u) != 0u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
    }
    return ~crc;
}

int DSO_Atecc608b_UidHex(char out[25])
{
    uint8_t uid[12];
    if (out == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    atecc_uid96(uid);
    DSO_Atecc608b_HexUpper(uid, 12u, out);
    return 0;
}

int DSO_Atecc608b_DeviceId16(char out[DSO_DEVICE_ID_LEN + 1u])
{
    uint8_t uid[12];
    uint32_t mix;
    char rnd[9];
    uint8_t i;

    if (out == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    /* 前 8 字符 = 物理标签前 8 字符（不足则补 '0'） */
    for (i = 0u; i < 8u; i++) {
        char c = DSO_DEVICE_LABEL[i];
        out[i] = (c != '\0') ? c : '0';
    }
    /* 后 8 字符 = 96-bit UID 的 CRC32（跨重启稳定；Stage 2 改为 ATECC TRNG 随机段并写入 Slot1） */
    atecc_uid96(uid);
    mix = atecc_crc32(uid, 12u);
    snprintf(rnd, sizeof(rnd), "%08lX", (unsigned long)mix);
    for (i = 0u; i < 8u; i++) {
        out[8u + i] = rnd[i];
    }
    out[DSO_DEVICE_ID_LEN] = '\0';
    return 0;
}

void DSO_Atecc608b_GetStatus(DSO_Atecc608b_Status *st)
{
    if (st != NULL) {
        *st = s_status;
    }
}

/* 记录一次访问结果：更新 comm_ok / fail_cnt（供遥测判断 tamper）
 *
 * **语义分界（2026-10-11 修正）**：comm_ok 描述的是**通信**是否正常，不是"命令是否成功"。
 *   rc == 0                 → 成功
 *   ERR_EXEC(-4) / ERR_PARAM(-5)
 *                           → 芯片**已答复**，只是拒绝执行/参数不合法
 *                             （典型：`Sign` 回 st=0x0F 因为槽里没有密钥）⇒ 通信正常
 *   ERR_IO(-1) / TIMEOUT(-2) / CRC(-3)
 *                           → 真正的通信故障（写不进去、等不到响应、帧校验不过）
 * 旧写法把任何负 rc 都清零 comm_ok：一次"槽里没密钥"的正常结果会让遥测显示
 * "comm_ok:0"，现场会误判成芯片通信坏了。fail_cnt 仍然如实累计**操作失败**次数。 */
static void atecc_note(int rc)
{
    /* ERR_EXEC(-4)/ERR_PARAM(-5) 时芯片**已答复**（典型：Sign 回 st=0x0F 因为槽里没密钥）
     * → 通信是好的，不置 comm_ok=0、也不计入"连续通信失败"连击。 */
    if (rc == 0 || rc == DSO_ATECC_ERR_EXEC || rc == DSO_ATECC_ERR_PARAM) {
        s_status.comm_ok = 1u;
        s_status.fail_cnt = 0u;
    } else {
        s_status.comm_ok = 0u;
        if (s_status.fail_cnt < 0xFFu) {
            s_status.fail_cnt++;
        }
    }
}

/* ---- 授权命令（片内完成，私钥不出片） ---- */

/* Nonce Mode 0x03：32 字节挑战直接装入 TempKey（"挑战即摘要"，后端按摘要验签）。
 * Param2 取 0x0000（长度由 Mode 隐含）；若该片要求 NumIn 置于 Param2 高字节，
 * 用 0x2000 兜底重试一次 —— 两种写法都试，避免字节序假设卡死。 */
static int atecc_nonce_passthru(const uint8_t challenge[32])
{
    uint8_t tk[32];
    int rc = DSO_Atecc608b_Exec(ATECC_OP_NONCE, ATECC_NONCE_MODE_PASSTHRU, 0x0000u,
                                challenge, 32u, tk, (uint16_t)sizeof(tk));
    if (rc < 0) {
        rc = DSO_Atecc608b_Exec(ATECC_OP_NONCE, ATECC_NONCE_MODE_PASSTHRU, 0x2000u,
                                challenge, 32u, tk, (uint16_t)sizeof(tk));
    }
    atecc_note(rc);
    return (rc >= 0) ? 0 : rc;
}

/* Sign Mode 0x80：用 slot 内私钥对 TempKey 摘要签名，返回 64B r||s。
 * Param2 = KeyID，按 CryptoAuthLib 惯例置于高字节（slot=0 时与字节序无关）。 */
static int atecc_sign_slot(uint8_t slot, uint8_t sig[64])
{
    int rc = DSO_Atecc608b_Exec(ATECC_OP_SIGN, ATECC_SIGN_MODE_DIGEST,
                                (uint16_t)((uint16_t)slot << 8), NULL, 0u, sig, 64u);
    atecc_note(rc);
    return (rc == 64) ? 0 : ((rc >= 0) ? DSO_ATECC_ERR_EXEC : rc);
}

/* Verify Mode 0x00 Stored：用 slot 内公钥验签 64B 签名；响应 1 字节，0x00=通过。 */
static int atecc_verify_stored(uint8_t slot, const uint8_t sig[64], uint8_t *ok)
{
    uint8_t res = 0xFFu;
    int rc = DSO_Atecc608b_Exec(ATECC_OP_VERIFY, ATECC_VERIFY_MODE_STORED,
                                (uint16_t)((uint16_t)slot << 8), sig, 64u, &res, 1u);
    atecc_note(rc);
    if (rc < 0) {
        return rc;
    }
    if (rc != 1) {
        return DSO_ATECC_ERR_EXEC;
    }
    if (ok != NULL) {
        *ok = (res == 0x00u) ? 1u : 0u;
    }
    return 0;
}

/* GenKey：Mode 0x04 = 在槽内新建 ECC 私钥并返回 64B 公钥（数据区，可反复重生成）；
 *         Mode 0x00 = 只返回该槽现有私钥对应的公钥（**不改私钥**，与 CryptoAuthLib
 *         atcab_get_pubkey 同路径）。注意 GenKey 会使 TempKey 失效。 */
static int atecc_genkey(uint8_t mode, uint8_t slot, uint8_t pub[64])
{
    int rc = DSO_Atecc608b_Exec(ATECC_OP_GENKEY, mode,
                                (uint16_t)((uint16_t)slot << 8), NULL, 0u, pub, 64u);
    atecc_note(rc);
    return (rc == 64) ? 0 : ((rc >= 0) ? DSO_ATECC_ERR_EXEC : rc);
}

int DSO_Atecc608b_GetPubKey64(uint8_t slot, uint8_t out[64])
{
    if (out == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    /* Mode 0x00：芯片用槽内私钥算公钥后输出，私钥不参与传输也不改变。
     * 调用后请复核公钥与后端登记值一致（若不一致说明该片 Mode 0x00 语义不同）。 */
    return atecc_genkey(0x00u, slot, out);
}

int DSO_Atecc608b_AuthSelfTest(const uint8_t challenge[32], uint8_t sig_out[64],
                               uint8_t *verify_ok, uint8_t allow_genkey)
{
    uint8_t sig[64];
    uint8_t pub[64];
    uint8_t ok = 0u;
    int rc;

    if (challenge == NULL || sig_out == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    if (verify_ok != NULL) {
        *verify_ok = 0u;
    }

    rc = atecc_nonce_passthru(challenge);
    if (rc != 0) {
        return rc;
    }
    rc = atecc_sign_slot(DSO_ATECC_AUTH_SLOT, sig);
    if (rc != 0 && allow_genkey != 0u) {
        /* 槽内无可用 ECC 私钥 → 按调用方**明确要求**新建一次（数据区操作，非破坏）。
         * GenKey 会使 TempKey 失效，故重装挑战后再签。 */
        rc = atecc_genkey(ATECC_GENKEY_MODE_NEW_ECC, DSO_ATECC_AUTH_SLOT, pub);
        if (rc == 0) {
            rc = atecc_nonce_passthru(challenge);
            if (rc == 0) {
                rc = atecc_sign_slot(DSO_ATECC_AUTH_SLOT, sig);
            }
        }
    }
    if (rc != 0) {
        return rc;
    }
    rc = atecc_verify_stored(DSO_ATECC_AUTH_SLOT, sig, &ok);
    if (rc != 0) {
        return rc;
    }
    memcpy(sig_out, sig, 64u);
    if (verify_ok != NULL) {
        *verify_ok = ok;
    }
    return 0;
}

/* ---- 诊断：Info 响应读不出来时，把响应原样打印出来 ----
 * 用来区分三种可能：
 *   A) 单次事务读 10 字节能看到合法的 [07][rev×4][crc2] → 芯片正常，是我们的分次读错位
 *   B) 仍是 04 FF 01 42 …                              → 芯片输出被扰乱（或确实回了这串）
 *   C) poll 一直失败（读地址 NACK）                     → 芯片忙/未就绪
 * 注意：先只读 1 字节探就绪，再**单次事务**读 10 字节（与 Exec 的"分两次读"对照）。 */
static void atecc_diag_info_raw(void)
{
    uint8_t frame[8];
    uint8_t raw[10];
    uint16_t crc;
    int wrc = -99;
    int prc = -99;
    int rrc = -99;
    uint8_t i;
    uint8_t tries = 0u;
    char hx[3u * 10u + 1u];

    for (i = 0u; i < 10u; i++) {
        raw[i] = 0u;
    }
    /* 组 Info 帧（与 atecc_exec_body 同构） */
    frame[0] = DSO_ATECC608B_WORD_CMD;
    frame[1] = 7u;
    frame[2] = ATECC_OP_INFO;
    frame[3] = 0x00u;
    frame[4] = 0x00u;
    frame[5] = 0x00u;
    crc = atecc_crc16(&frame[1], 5u);
    frame[6] = (uint8_t)(crc & 0xFFu);
    frame[7] = (uint8_t)(crc >> 8);

    (void)DSO_Atecc608b_Wake(NULL, NULL);
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);
    BSP_I2C_UI_Lock();
    wrc = BSP_I2C_UI_WriteRaw(s_addr, frame, 8u);
    if (wrc == 0) {
        for (i = 0u; i < 30u; i++) {
            prc = BSP_I2C_UI_ReadRaw(s_addr, raw, 1u);   /* 探就绪（只读 1 字节） */
            tries++;
            if (prc == 0) {
                break;
            }
            bsp_DelayUS(1000u);
        }
        if (prc == 0) {
            rrc = BSP_I2C_UI_ReadRaw(s_addr, raw, 10u);  /* 单次事务读 10 字节 */
        }
    }
    BSP_I2C_UI_Unlock();
    BSP_I2C_UI_SetBitDelayUs(0u);

    for (i = 0u; i < 10u; i++) {
        (void)snprintf(&hx[3u * i], 4u, "%02X ", raw[i]);
    }
    hx[30] = '\0';
    DebugUartLock();
    printf("[ATECC] diag info: write=%d poll=%d tries=%u read10=%d raw=%s\r\n",
           wrc, prc, (unsigned)tries, rrc, hx);
    DebugUartUnlock();

    /* 追加对照：再试一条最简单的读命令（Read 配置区 block0，32 字节）。
     * 若它同样给不出合法 CRC → 芯片对**任何**命令的响应都不合规（不是 Info 被单独拒）。 */
    {
        uint8_t blk32[32];
        uint8_t j;
        char hx2[3u * 8u + 1u];
        int crc_rc = DSO_Atecc608b_ReadConfig32(0u, blk32);

        for (j = 0u; j < 8u; j++) {
            (void)snprintf(&hx2[3u * j], 4u, "%02X ", blk32[j]);
        }
        hx2[24] = '\0';
        DebugUartLock();
        printf("[ATECC] diag readcfg: rc=%d st=0x%02X first8=%s\r\n",
               crc_rc, (unsigned)DSO_Atecc608b_LastStatus(),
               (crc_rc == 0) ? hx2 : "(n/a)");
        DebugUartUnlock();
    }
}

/* ---- 诊断：**裸读实验**（不发送任何命令，纯只读）----
 * 唤醒 → 等 tWHI → 直接读 6 字节，重复 3 次。用来分开两种世界：
 *   ① 读地址 NACK（rc=-1）→ 设备地址模型**合规**（ATECC 无输出时对读地址 NACK）
 *      ⇒ 先前读到的字节**确实是命令响应** ⇒ 问题落在帧/CRC 生成端
 *   ② 能读到字节（尤其又是 04 FF 01 42）→ 设备**在没有命令时就准备输出**
 *      ⇒ 那些字节**不是命令响应** ⇒ 它不是合规 ATECC 的地址模型（更像带内部指针的存储类器件）
 * 注意：地址 ACK 不能证明"已正确唤醒"，本实验正是针对这一点。 */
static void atecc_diag_bare_read(void)
{
    uint8_t buf[6];
    uint8_t i;
    uint8_t k;
    char hx[3u * 6u + 1u];
    int rc;

    for (k = 0u; k < 3u; k++) {
        for (i = 0u; i < 6u; i++) {
            buf[i] = 0u;
        }
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);
        BSP_I2C_UI_Lock();
        BSP_I2C_UI_WakePulseUs(20000u);          /* 20ms SDA 低 + 内部 2ms 等待 */
        rc = BSP_I2C_UI_ReadRaw(s_addr, buf, 6u); /* 无命令，直接读 */
        BSP_I2C_UI_Unlock();
        BSP_I2C_UI_SetBitDelayUs(0u);
        for (i = 0u; i < 6u; i++) {
            (void)snprintf(&hx[3u * i], 4u, "%02X ", buf[i]);
        }
        hx[18] = '\0';
        DebugUartLock();
        printf("[ATECC] diag bare-read #%u (no command): rc=%d raw=%s\r\n",
               (unsigned)(k + 1u), rc, (rc == 0) ? hx : "(nack: no output)");
        DebugUartUnlock();
        bsp_DelayUS(2000u);
    }
}

/* ---- 诊断：**逐字节 ACK 掩码**（tHD;DAT 修正后新增）----
 * 明确"芯片从哪一个字节开始不应答"：
 *   addr 就 NACK        → 芯片没被唤醒 / 不在总线 / 地址不符
 *   0x03(word) NACK     → 它没把这当作合法 word 地址（协议/内容问题）
 *   组内某字节 NACK     → 帧内容或时序在该字节处出问题
 * （用 DiagTxBytes：即使某字节 NACK 也会把剩余字节发完并记录，故掩码完整。） */
static void atecc_diag_perbyte_ack(void)
{
    uint8_t frame[8];
    uint8_t ack[10];
    uint16_t crc;
    int rc;

    frame[0] = DSO_ATECC608B_WORD_CMD;
    frame[1] = 7u;
    frame[2] = ATECC_OP_INFO;
    frame[3] = 0x00u;
    frame[4] = 0x00u;
    frame[5] = 0x00u;
    crc = atecc_crc16(&frame[1], 5u);
    frame[6] = (uint8_t)(crc & 0xFFu);
    frame[7] = (uint8_t)(crc >> 8);

    (void)DSO_Atecc608b_Wake(NULL, NULL);
    BSP_I2C_UI_SetBitDelayUs(s_session_bit_us);
    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_DiagTxBytes(s_addr, frame, 8u, ack);
    BSP_I2C_UI_Unlock();
    BSP_I2C_UI_SetBitDelayUs(0u);

    DebugUartLock();
    printf("[ATECC] perbyte rc=%d ack=addr:%u 03:%u 07:%u 30:%u 00:%u 00:%u 00:%u BA:%u C0:%u\r\n",
           rc, (unsigned)ack[0], (unsigned)ack[1], (unsigned)ack[2], (unsigned)ack[3],
           (unsigned)ack[4], (unsigned)ack[5], (unsigned)ack[6], (unsigned)ack[7],
           (unsigned)ack[8]);
    DebugUartUnlock();
}

/* ---- 诊断：**会话速率扫描**（芯片对 I2C 速率是否敏感）----
 * 对每个档位跑一次完整 Info（Exec 会按 s_session_bit_us 设档并走重发逻辑），
 * 打印 rc/状态。放只在 Info 失败时调用——好片不必付这份代价。
 * 背景：2kHz 会话档只在 2026-09-22 那颗片上成功过；本片从未验证过速率。 */
static void atecc_rate_sweep(void)
{
    static const uint32_t rates[7] = { 500u, 250u, 100u, 50u, 30u, 10u, 5u };
    uint8_t i;

    DebugUartLock();
    printf("[ATECC] session rate sweep (us per half-period):\r\n");
    DebugUartUnlock();
    for (i = 0u; i < 7u; i++) {
        uint8_t rv[4] = { 0u, 0u, 0u, 0u };
        int rc;
        s_session_bit_us = rates[i];
        rc = DSO_Atecc608b_InfoRevision(rv);
        DebugUartLock();
        printf("[ATECC]   %4uus: rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
               (unsigned)rates[i], rc,
               (s_status_valid != 0u) ? (unsigned)s_last_status : 0xFFu,
               (unsigned)rv[0], (unsigned)rv[1], (unsigned)rv[2], (unsigned)rv[3],
               (rc == 0) ? "<ATECC OK>" : "(fail)");
        DebugUartUnlock();
        if (rc == 0) {
            break;                       /* 找到可用档位即停（并保留该档） */
        }
    }
    if (i >= 7u) {
        s_session_bit_us = DSO_ATECC_SESSION_BIT_US;   /* 全失败则还原默认 */
    }
}

/* ---- 诊断：**Read 路径分解**（镜像约定确认后跑）----
 * 现象：Info 在镜像约定下 rc=0，但 Read 配置区报 rc=-1（ERR_IO）。
 * ERR_IO 只可能来自"写失败 / 轮询后读 body 失败"（轮询超时是 -2），且 Info 与 Read 的
 * 写帧长度相同（8 字节）、响应也走"先读 1 字节再读余量"的同一路径 —— 差别只有
 * **响应长度**（Info 7 字节 vs Read 35 字节）。所以把两个变量拆开测：
 *   A) 走真实代码路径（Exec 两段式读），当前会话档 —— 复现
 *   B) 同上但会话档 20µs —— 判断是否速率相关（同档下 Info 是通的）
 *   C) **单次事务读 36 字节**（不做"先读 1 字节"的轮询）—— 验证怀疑：那段 1 字节轮询
 *      读完即 NACK，可能让长响应无法继续；单次事务能一次读全整包
 * 只发 Read(0x02)（只读）；word 恒 0x03；不写任何区域。 */
static void atecc_diag_read_cfg(void)
{
    uint8_t out[32];
    uint8_t frame[8];
    uint8_t raw[36];
    uint16_t crc;
    uint32_t saved = s_session_bit_us;
    int rc;
    int wrc = -99;
    int rrc = -99;
    uint8_t i;

    /* A) 真实路径 @ 当前会话档 */
    for (i = 0u; i < 32u; i++) {
        out[i] = 0u;
    }
    rc = DSO_Atecc608b_ReadConfig32(0u, out);
    DebugUartLock();
    printf("[ATECC-rd] A) Exec 2-phase @%uus: rc=%d st=0x%02X first4=%02X %02X %02X %02X\r\n",
           (unsigned)saved, rc, (unsigned)DSO_Atecc608b_LastStatus(),
           (unsigned)out[0], (unsigned)out[1], (unsigned)out[2], (unsigned)out[3]);
    DebugUartUnlock();

    /* B) 真实路径 @ 20µs（同档下 Info 已证明可通） */
    s_session_bit_us = DSO_ATECC_LA_SLOW_US;
    for (i = 0u; i < 32u; i++) {
        out[i] = 0u;
    }
    rc = DSO_Atecc608b_ReadConfig32(0u, out);
    DebugUartLock();
    printf("[ATECC-rd] B) Exec 2-phase @%uus: rc=%d st=0x%02X first4=%02X %02X %02X %02X %s\r\n",
           (unsigned)s_session_bit_us, rc, (unsigned)DSO_Atecc608b_LastStatus(),
           (unsigned)out[0], (unsigned)out[1], (unsigned)out[2], (unsigned)out[3],
           (rc == 0) ? "<OK>" : "(fail)");
    DebugUartUnlock();

    /* C) 单次事务读整包（不做 1 字节轮询） */
    for (i = 0u; i < 36u; i++) {
        raw[i] = 0u;
    }
    frame[0] = DSO_ATECC608B_WORD_CMD;
    frame[1] = 7u;                                  /* Count：Read32 无数据字节，同 Info */
    frame[2] = ATECC_OP_READ;
    frame[3] = ATECC_READ32_P1;                     /* Param1：0x80 = 32B 读 / zone=Config */
    frame[4] = 0x00u;
    frame[5] = 0x00u;                               /* Param2 = block0 的地址 0x0000 */
    crc = atecc_crc_wire(atecc_crc16(&frame[1], 5u));
    frame[6] = (uint8_t)(crc & 0xFFu);
    frame[7] = (uint8_t)(crc >> 8);

    (void)DSO_Atecc608b_Wake(NULL, NULL);
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_LA_SLOW_US);
    BSP_I2C_UI_Lock();
    wrc = BSP_I2C_UI_WriteRaw(s_addr, frame, 8u);
    if (wrc == 0) {
        for (i = 0u; i < 30u; i++) {
            rrc = BSP_I2C_UI_ReadRaw(s_addr, raw, 36u);   /* 一次读全整包 */
            if (rrc == 0) {
                break;
            }
            bsp_DelayUS(1000u);
        }
    }
    BSP_I2C_UI_Unlock();
    BSP_I2C_UI_SetBitDelayUs(0u);
    s_session_bit_us = saved;

    DebugUartLock();
    printf("[ATECC-rd] C) single-transaction 36B @%uus: write=%d read=%d count=%02X "
           "first8=%02X %02X %02X %02X %02X %02X %02X %02X %s\r\n",
           (unsigned)DSO_ATECC_LA_SLOW_US, wrc, rrc, (unsigned)raw[0],
           (unsigned)raw[0], (unsigned)raw[1], (unsigned)raw[2], (unsigned)raw[3],
           (unsigned)raw[4], (unsigned)raw[5], (unsigned)raw[6], (unsigned)raw[7],
           (rrc == 0 && raw[0] == 0x23u) ? "<count=0x23 = full 32B packet!>"
                                         : "(not the expected count)");
    DebugUartUnlock();
}

/* ---- 诊断：**CRC 约定判定臂**（一次性，4+1 帧，<2 s）----
 * 依据（2026-10-11 计算）：本器件的响应尾字节 = 我们标准 CRC-16/ARC 的**位序镜像**：
 *   {04,11}: 我们的 ARC=0xCCC2 → rev16=0x4333，它实测尾字节 33 43  ✔
 *   {04,FF}: 我们的 ARC=0x8042 → rev16=0x4201，它实测尾字节 01 42  ✔
 * 两条互相独立的 16 位匹配（偶然概率 ≈ 2^-32）⇒ 它的 CRC 引擎按镜像位序工作：
 * 我们的**正确**帧在它眼里"CRC 错" → 回 0xFF；它自己的帧我们也验不过。
 * 本臂把 Info 帧的尾两字节换成四种候选，一次看清它接受哪一种：
 *   ① 标准    BA C0 → 预期仍被拒（04 FF …）：对照组
 *   ② 镜像 lo 03 5D → **主预测：被接受**（count 变 07，含 00 00 60 03）
 *   ③ 镜像 hi 5D 03 → 备选（它读 CRC 的字节序未定）
 *   ④ 仅对调  C0 BA → 反证：只换字节序不够
 * ②/③ 被接受 ⇒ 自动切镜像约定 → **重跑 Info 复验**（不过则回退，绝不留坏状态）
 *              → 再用 Read 配置区 block0 泛化确认（第二个命令也通）。
 * 安全：只发 Info(0x30) 与 Read(0x02)（均只读）；word 恒 0x03；不写任何区域、不 Lock。
 * @return 1 = 镜像约定已确认并启用（通信打通）；0 = 未确认（调用方继续跑其余诊断臂）。 */
static uint8_t atecc_diag_crc_conv(void)
{
    /* 候选尾两字节（按发送次序）+ 标签 */
    static const uint8_t tails[4][2] = {
        { 0xBAu, 0xC0u },   /* ① 标准（当前发的） */
        { 0x03u, 0x5Du },   /* ② 镜像值 lo-first  <== 主预测 */
        { 0x5Du, 0x03u },   /* ③ 镜像值 hi-first  <== 备选 */
        { 0xC0u, 0xBAu }    /* ④ 仅字节对调 */
    };
    static const char *tags[4] = { "standard ", "mirror-lo", "mirror-hi", "swaponly " };
    uint8_t frame[8];
    uint8_t raw[6];
    uint8_t k;
    uint8_t i;
    int rc;
    int acc = -1;                    /* 被接受的候选下标；-1 = 全被拒 */

    DebugUartLock();
    printf("[ATECC-crc] convention test: is chip tail == rev16(our CRC-16/ARC)? "
           "4 candidate Info frames follow\r\n");
    DebugUartUnlock();

    for (k = 0u; k < 4u; k++) {
        for (i = 0u; i < 6u; i++) {
            raw[i] = 0u;
        }
        frame[0] = DSO_ATECC608B_WORD_CMD;
        frame[1] = 7u;                    /* Count：ATECC608 的 Info 固定 0x07 */
        frame[2] = ATECC_OP_INFO;
        frame[3] = 0x00u;                 /* Param1：0 = Revision */
        frame[4] = 0x00u;
        frame[5] = 0x00u;                 /* Param2 = 0x0000 */
        frame[6] = tails[k][0];
        frame[7] = tails[k][1];

        (void)DSO_Atecc608b_Wake(NULL, NULL);
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_LA_SLOW_US);   /* 20µs：解码友好且够快 */
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_WriteRaw(s_addr, frame, 8u);
        if (rc == 0) {
            for (i = 0u; i < 30u; i++) {
                rc = BSP_I2C_UI_ReadRaw(s_addr, raw, 6u);
                if (rc == 0) {
                    break;
                }
                bsp_DelayUS(1000u);
            }
        }
        BSP_I2C_UI_Unlock();
        BSP_I2C_UI_SetBitDelayUs(0u);

        /* 判据：首个应答字节不再是 4 字节错误组的 0x04（也不是 0xFF）⇒ 命令被接受 */
        if (rc == 0 && raw[0] != 0x04u && raw[0] != 0xFFu && acc < 0) {
            acc = (int)k;
        }
        DebugUartLock();
        printf("[ATECC-crc] #%u %s tail=%02X %02X -> rd=%d resp=%02X %02X %02X %02X %02X %02X %s\r\n",
               (unsigned)(k + 1u), tags[k],
               (unsigned)frame[6], (unsigned)frame[7], rc,
               (unsigned)raw[0], (unsigned)raw[1], (unsigned)raw[2],
               (unsigned)raw[3], (unsigned)raw[4], (unsigned)raw[5],
               (rc == 0 && raw[0] != 0x04u && raw[0] != 0xFFu)
                   ? "<ACCEPTED>" : "(rejected: 04 FF ..)");
        DebugUartUnlock();
        bsp_DelayUS(2000u);
    }

    if (acc < 0) {
        DebugUartLock();
        printf("[ATECC-crc] all 4 tails rejected -> mirror hypothesis FAILED. "
               "Next: bounded convention sweep, or an independent I2C master.\r\n");
        DebugUartUnlock();
        return 0u;
    }

    DebugUartLock();
    printf("[ATECC-crc] ACCEPTED by candidate #%u (%s) -> mirror convention CONFIRMED\r\n",
           (unsigned)(acc + 1), tags[acc]);
    DebugUartUnlock();

#if DSO_ATECC_CRC_MIRROR
    /* 切镜像约定并**立即复验**：复验不过就回退，绝不把坏状态留在 s_crc_mirror 上 */
    s_crc_mirror = 1u;
    s_crc_swap = (acc == 2) ? 1u : 0u;
    {
        uint8_t rv[4] = { 0u, 0u, 0u, 0u };
        int vrc = DSO_Atecc608b_InfoRevision(rv);

        DebugUartLock();
        printf("[ATECC-crc] mirror mode ON (swap=%u) -> verify Info rc=%d st=0x%02X "
               "rev=%02X%02X%02X%02X %s\r\n",
               (unsigned)s_crc_swap, vrc, (unsigned)DSO_Atecc608b_LastStatus(),
               (unsigned)rv[0], (unsigned)rv[1], (unsigned)rv[2], (unsigned)rv[3],
               (vrc == 0) ? "<MIRROR WORKS>" : "(verify FAILED -> revert)");
        DebugUartUnlock();

        if (vrc != 0) {
            s_crc_mirror = 0u;
            s_crc_swap = 0u;
            return 0u;
        } else {
            /* 通信已通：同步状态快照，使遥测模块帧立刻反映"在且通"
             * （Info/Read 不走 atecc_note，故这里显式置位） */
            s_status.present = 1u;
            s_status.comm_ok = 1u;
            s_status.rev[0] = rv[0];
            s_status.rev[1] = rv[1];
            s_status.rev[2] = rv[2];
            s_status.rev[3] = rv[3];
            /* 泛化确认第二个命令（Read 配置区），并把"Read 为何 ERR_IO"拆成三臂测 */
            atecc_diag_read_cfg();
            return 1u;              /* 通信已打通：其余诊断臂无需再跑 */
        }
    }
#else
    DebugUartLock();
    printf("[ATECC-crc] DSO_ATECC_CRC_MIRROR=0 -> confirmed but not switching convention\r\n");
    DebugUartUnlock();
    return 0u;
#endif
}

/* ---- 只读：SlotConfig 逐槽解读（"先读后写"的依据） ---- */

void DSO_Atecc608b_DumpSlotConfig(const uint8_t *cfg128)
{
    uint8_t local[128];
    const uint8_t *cfg;
    uint16_t w[16];
    uint8_t blk;
    uint8_t i;
    char line[160];
    int n;

    if (cfg128 != NULL) {
        cfg = cfg128;               /* 复用调用方已读到的配置区，省 4 次 32B 读 */
    } else {
        for (blk = 0u; blk < 4u; blk++) {
            if (DSO_Atecc608b_ReadConfig32(blk, &local[blk * 32u]) != 0) {
                DebugUartLock();
                printf("[ATECC] slotcfg: config read failed at blk=%u\r\n", (unsigned)blk);
                DebugUartUnlock();
                return;
            }
        }
        cfg = local;
    }
    /* SlotConfig[0..15] 在 cfg[0x20..0x3F]，每槽 16-bit（按小端组字） */
    n = sprintf(line, "[ATECC] slotcfg raw(0x20..0x3F):");
    for (i = 0u; i < 16u; i++) {
        n += sprintf(line + n, " %02X%02X", cfg[0x20u + 2u * i], cfg[0x20u + 2u * i + 1u]);
        w[i] = (uint16_t)cfg[0x20u + 2u * i] | ((uint16_t)cfg[0x20u + 2u * i + 1u] << 8);
    }
    sprintf(line + n, "\r\n");
    DebugUartLock();
    printf("%s", line);
    /* 文档指定的 5 个关键槽逐槽解读（ReadConfig=b3:0 WriteConfig=b7:4 IsSecret=b15 KeyType=b14:12） */
    for (i = 0u; i < 5u; i++) {
        printf("[ATECC] slot%u w=%04X Read=%u Write=%u IsSecret=%u KeyType=%u\r\n",
               (unsigned)i, (unsigned)w[i],
               (unsigned)(w[i] & 0x0Fu), (unsigned)((w[i] >> 4) & 0x0Fu),
               (unsigned)((w[i] >> 15) & 0x01u), (unsigned)((w[i] >> 12) & 0x07u));
    }
    printf("[ATECC] (bits per ATECC608 SlotConfig; if mismatch, trust the raw 0x20..0x3F bytes)\r\n");
    DebugUartUnlock();
    /* 填入只读状态快照（serial4 = SN[0..3]；0x56=0 已锁数据区，0x57=0 已锁配置区） */
    s_status.serial4[0] = cfg[0x00];
    s_status.serial4[1] = cfg[0x01];
    s_status.serial4[2] = cfg[0x02];
    s_status.serial4[3] = cfg[0x03];
    s_status.lock_config = (cfg[ATECC_CFG_LOCK_CONFIG_OFF] == 0x00u) ? 1u : 0u;
    s_status.lock_data = (cfg[ATECC_CFG_LOCK_VALUE_OFF] == 0u) ? 1u : 0u;
}

/* 已知器件（这些地址绝不送 ATECC 命令：EEPROM 会写坏、PCAL6524 寄存器就是 LED 输出） */
static int atecc_scan_known(uint8_t a)
{
    return (a == 0x22u || a == 0x26u || a == 0x27u ||
            a == 0x40u || a == 0x41u || a == 0x44u);
}

/* 上电一次性地址扫描 0x01-0x7F：列出全部 ACK 的 7-bit 地址，并单独标注 3 个候选。
 * 用途：确认 ATECC 究竟在哪个地址（0x60 / 0x35 / 0x36），以及总线上有没有新增器件。
 * 判定档位用快档（5µs）：实测新旧片在 93kHz/200kHz 都能 ACK 地址，扫得快。 */
static void atecc_boot_scan(void)
{
    uint8_t addr;
    uint8_t n = 0u;
    uint8_t a60 = 0u;
    uint8_t a35 = 0u;
    uint8_t a36 = 0u;
    uint32_t saved = BSP_I2C_UI_GetBitDelayUs();
    char list[192];
    int len = 0;

    BSP_I2C_UI_SetBitDelayUs(5u);
    BSP_I2C_UI_Lock();
    for (addr = 0x01u; addr <= 0x7Fu; addr++) {
        if (BSP_I2C_UI_Probe(addr) == 0) {
            if ((size_t)len + 6u < sizeof(list)) {
                len += snprintf(&list[len], sizeof(list) - (size_t)len,
                                " %02X", (unsigned)addr);
            }
            n++;
            if (addr == 0x60u) { a60 = 1u; }
            if (addr == 0x35u) { a35 = 1u; }
            if (addr == 0x36u) { a36 = 1u; }
        }
    }
    BSP_I2C_UI_Unlock();
    BSP_I2C_UI_SetBitDelayUs(saved);
    if (len < (int)sizeof(list)) {
        list[len] = '\0';
    } else {
        list[sizeof(list) - 1u] = '\0';
    }
    DebugUartLock();
    printf("[ATECC] addr scan 0x01-0x7F: n=%u ->%s\r\n", (unsigned)n, list);
    printf("[ATECC] scan cand: 0x60=%s 0x35=%s 0x36=%s\r\n",
           a60 ? "ACK" : "NACK", a35 ? "ACK" : "NACK", a36 ? "ACK" : "NACK");
    DebugUartUnlock();

    /* 对三个候选各跑一次**完整 Info**（组包 + 读响应 + CRC 校验）：
     * 地址 ACK 只说明"有东西在应答"，**只有响应帧 CRC 成立**才说明它是能正常工作的 ATECC。 */
    {
        static const uint8_t cand[3] = { 0x60u, 0x35u, 0x36u };
        uint8_t i;
        for (i = 0u; i < 3u; i++) {
            uint8_t rv[4] = { 0u, 0u, 0u, 0u };
            uint8_t old = s_addr;
            int rc;
            s_addr = cand[i];
            rc = DSO_Atecc608b_InfoRevision(rv);
            DebugUartLock();
            printf("[ATECC] info@0x%02X: rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
                   (unsigned)cand[i], rc, (unsigned)DSO_Atecc608b_LastStatus(),
                   (unsigned)rv[0], (unsigned)rv[1], (unsigned)rv[2], (unsigned)rv[3],
                   (rc == 0) ? "<ATECC OK>" : "(fail)");
            DebugUartUnlock();
            s_addr = old;
        }
    }

    /* 其余**未知地址**逐个"体检"（全程只读）：
     *   ① 地址不 ACK → 跳过
     *   ② 读 1 字节**能** ACK → 内存映射型（EEPROM 类）→ **跳过**（对它发命令等于写）
     *   ③ 地址 ACK 但读 NACK → 命令/状态型（密码芯片特征）→ 才送完整 Info 判定
     * 注意：早期"R=ok 内存映射型"的分类是在 ACK 采样还有 bug 的时期做的，不可信，故此步重做。
     * 候选清单里已含 0x60/0x35/0x36（上面已测），这里只补其它地址（如 0x64/0x50/0x7C）。 */
    {
        uint8_t a;
        BSP_I2C_UI_SetBitDelayUs(5u);
        for (a = 0x01u; a <= 0x7Fu; a++) {
            uint8_t b = 0u;
            uint8_t rv[4] = { 0u, 0u, 0u, 0u };
            int pr;
            int rc;
            if (atecc_scan_known(a) || a == 0x60u || a == 0x35u || a == 0x36u) {
                continue;
            }
            BSP_I2C_UI_Lock();
            pr = BSP_I2C_UI_Probe(a) == 0 ? 1 : 0;            /* ① 地址 ACK？ */
            if (pr != 0) {
                /* ② **连续两次**读 NACK 才算命令型（内存映射型会稳定 ACK 读）。
                 * 误判成命令型的代价是给未知器件写 8 字节命令（EEPROM 可能被改），故双重确认。 */
                if (BSP_I2C_UI_ReadRaw(a, &b, 1u) != 0 &&
                    BSP_I2C_UI_ReadRaw(a, &b, 1u) != 0) {
                    pr = 1;                                   /* 命令/状态型 */
                } else {
                    pr = 2;                                   /* 内存映射型 */
                }
            }
            BSP_I2C_UI_Unlock();
            if (pr == 0 || pr == 2) {
                continue;                                     /* 无器件 / 内存映射型：不碰 */
            }
            s_addr = a;
            rc = DSO_Atecc608b_InfoRevision(rv);
            DebugUartLock();
            printf("[ATECC] probe@0x%02X(cmd-type): rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
                   (unsigned)a, rc, (unsigned)DSO_Atecc608b_LastStatus(),
                   (unsigned)rv[0], (unsigned)rv[1], (unsigned)rv[2], (unsigned)rv[3],
                   (rc == 0) ? "<ATECC OK: use this address!>" : "(fail)");
            DebugUartUnlock();
            s_addr = DSO_ATECC608B_I2C_ADDR;
        }
        BSP_I2C_UI_SetBitDelayUs(saved);
    }
}

static void atecc608b_probe_task(void *pv)
{
    uint8_t cfg[128];
    uint8_t rev[4];
    uint8_t hit;     /* 判定电池命中的 ATECC 地址（0=未探到） */
    int rc;
    (void)pv;

    /* 规范：上电 ≥20ms 后才可动 I2C（实测值由 Arm 0 打印）；识别档位延迟见
     * DSO_ATECC_BIT_DELAY_US（当前 500µs≈2kHz，给 ACK 多点采样留窗口） */
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_POR_WAIT_MS));
    s_addr = DSO_ATECC608B_I2C_ADDR;

    /* 裸读实验必须**最先做**（在任何命令之前，甚至在地址扫描之前 —— 扫描里会对候选发 Info
     * 命令）。若在命令之后做，读到的可能是上一条命令遗留的输出 FIFO（真 ATECC 的响应会留在
     * 缓冲里，此时读地址 ACK 属正常）。只有"上电后从未发过命令却能读到数据"才能证明该设备
     * 不是合规 ATECC 的地址模型（合规 ATECC 无输出时对读地址 NACK）。 */
    atecc_diag_bare_read();

    /* 全地址扫描：确认芯片到底在哪个 7-bit 地址（0x60 / 0x35 / 0x36 / 其它），
     * 以及总线上有没有新增器件；并对候选与"命令型未知地址"各跑一次完整 Info。纯只读。 */
    atecc_boot_scan();

#if DSO_ATECC_DIAG_BATTERY
    /* 方案 1 逐字复刻（tPU→Wake→tWHI→立即探+重试）刻意放在**最早时机**执行，
     * 满足"上电后立即 Wake 并扫描"，然后再等启动打印出完跑其余各臂 */
    uint8_t l1 = atecc_diag_l1_early();
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_DIAG_SETTLE_MS));
    /* 分层判定实验（只读）：Arm 0 上电事实+冷态基线 / A GPIO脉冲窗口 / B 0x00字节法对照 /
     * H 自身地址字节唤醒 / G 信标+多采样 / I 时序合规 / J 0x00 / K 0x60·0x35 完整流 /
     * L 用户四方案 / C 等待档 / D 速率档 / E 冷态vs醒态差分扫描 / F 脉宽扫描，末行为结论 */
    hit = atecc608b_diag_battery();
    if (hit == 0u && l1 != 0u) {
        hit = 0x60u;
    }
#else
    /* 正式路径（判定电池关闭时）：整条识别链统一走**会话档 500µs≈2kHz** ——
     * 这是实测唯一能稳定"读到响应帧"的档位（见 DSO_ATECC_SESSION_BIT_US 注释：
     * 地址 ACK 可以更快，但读响应不行）。识别完成后在任务尾部还原为 0。 */
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);
    hit = (uint8_t)((DSO_Atecc608b_Wake(NULL, NULL) == 0) ? s_addr : 0u);
#endif

    if (hit != 0u) {
        s_addr = hit;
        rc = DSO_Atecc608b_InfoRevision(rev);
        DebugUartLock();
        printf("[ATECC] Info rc=%d st=0x%02X rev=%02X%02X%02X%02X crc=%s %s\r\n",
               rc, (unsigned)DSO_Atecc608b_LastStatus(),
               (unsigned)rev[0], (unsigned)rev[1],
               (unsigned)rev[2], (unsigned)rev[3],
               (s_crc_mirror != 0u) ? "mirror" : "std",
               (rc == 0 && rev[2] == 0x60u) ? "<ATECC608B>" : "(rev?)");
        DebugUartUnlock();
#if DSO_ATECC_CRC_MIRROR
        if (rc != 0 && s_crc_mirror == 0u) {
            /* **快路径判别 CRC 约定**：正品用标准 CRC；本批仿制件用镜像 CRC（见头文件）。
             * 各试一次即可定论，且**必须立刻复验**：只有镜像帧也被接受才切档，
             * 否则回退标准（绝不留坏状态）。判定成功后走下面的**正常启动流程**
             * （Config dump / SlotConfig / device_id / 自测），而不是掉进诊断臂。 */
            int mrc;
            DSO_Atecc608b_SetCrcMirror(1u);
            mrc = DSO_Atecc608b_InfoRevision(rev);
            DebugUartLock();
            printf("[ATECC] mirror-CRC retry: rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
                   mrc, (unsigned)DSO_Atecc608b_LastStatus(),
                   (unsigned)rev[0], (unsigned)rev[1],
                   (unsigned)rev[2], (unsigned)rev[3],
                   (mrc == 0) ? "<MIRROR CRC ACCEPTED: crc=mirror from here on>"
                              : "(mirror also rejected -> back to std)");
            DebugUartUnlock();
            if (mrc == 0) {
                rc = 0;              /* 已切镜像约定，继续正常启动流程 */
            } else {
                DSO_Atecc608b_SetCrcMirror(0u);
            }
        }
#endif
        if (rc == 0) {
            /* Config 区 4×32B 全区 dump（偏移语义按 ATECC608 存储器映射核对） */
            uint8_t blk;
            uint8_t i;
            char line[128];   /* "[ATECC] cfgXX:" + 32×" XX" + CRLF + NUL = 113 */
            int n;
            for (blk = 0u; blk < 4u; blk++) {
                if (DSO_Atecc608b_ReadConfig32(blk, &cfg[blk * 32u]) != 0) {
                    rc = -1;
                    break;
                }
            }
            if (rc != 0) {
                DebugUartLock();
                /* 打出**失败在哪一块**与芯片状态：blk 是关键信息（0/1/2/3 说明是第几块开始不行，
                 * 也可能是第一块就失败），st=0xFF 是"通信错"、其它值看 ATECC 状态码表。 */
                printf("[ATECC] config read failed at blk=%u (of 4) st=0x%02X crc=%s\r\n",
                       (unsigned)blk, (unsigned)DSO_Atecc608b_LastStatus(),
                       (s_crc_mirror != 0u) ? "mirror" : "std");
                DebugUartUnlock();
            } else {
                for (blk = 0u; blk < 4u; blk++) {
                    n = sprintf(line, "[ATECC] cfg%02X:", (unsigned)(blk * 32u));
                    for (i = 0u; i < 32u; i++) {
                        n += sprintf(line + n, " %02X", cfg[blk * 32u + i]);
                    }
                    sprintf(line + n, "\r\n");
                    DebugUartLock();
                    printf("%s", line);
                    DebugUartUnlock();
                }
                DebugUartLock();
                printf("[ATECC] sn9=%02X%02X%02X%02X%02X%02X%02X%02X%02X "
                       "rev@08=%02X lockval@56=%02X lockcfg@57=%02X slotlock@58=%02X%02X\r\n",
                       cfg[0x00], cfg[0x01], cfg[0x02], cfg[0x03],
                       cfg[0x04], cfg[0x05], cfg[0x06], cfg[0x07],
                       cfg[ATECC_CFG_SN8_OFF], cfg[ATECC_CFG_REV_OFF],
                       cfg[ATECC_CFG_LOCK_VALUE_OFF], cfg[ATECC_CFG_LOCK_CONFIG_OFF],
                       cfg[ATECC_CFG_SLOT_LOCKED_OFF],
                       cfg[ATECC_CFG_SLOT_LOCKED_OFF + 1u]);
                printf("[ATECC] present=1 addr=0x%02X\r\n", (unsigned)s_addr);
                DebugUartUnlock();
                /* 走到这里说明 Info 成功 ✓ 下面继续做只读识别与自测 */

                /* ---- 授权 Stage 1：状态快照 + SlotConfig 解读 + 设备身份 + 片内自测 ---- */
                s_status.present = 1u;
                s_status.comm_ok = 1u;
                s_status.rev[0] = rev[0];
                s_status.rev[1] = rev[1];
                s_status.rev[2] = rev[2];
                s_status.rev[3] = rev[3];
                DSO_Atecc608b_DumpSlotConfig(cfg);   /* 复用刚读到的 128B 配置区 */
                {
                    char uid[25];
                    char did[DSO_DEVICE_ID_LEN + 1u];
                    (void)DSO_Atecc608b_UidHex(uid);
                    (void)DSO_Atecc608b_DeviceId16(did);
                    DebugUartLock();
                    printf("[ATECC] uid96=%s device_id=%s\r\n", uid, did);
                    DebugUartUnlock();
                }

#if DSO_ATECC_AUTH_SELFTEST_AT_BOOT
                {
                    /* 固定自测挑战（仅 bring-up 自检；正式流程由服务器下发随机挑战） */
                    static const uint8_t chal[32] = {
                        0x00u,0x01u,0x02u,0x03u,0x04u,0x05u,0x06u,0x07u,
                        0x08u,0x09u,0x0Au,0x0Bu,0x0Cu,0x0Du,0x0Eu,0x0Fu,
                        0x10u,0x11u,0x12u,0x13u,0x14u,0x15u,0x16u,0x17u,
                        0x18u,0x19u,0x1Au,0x1Bu,0x1Cu,0x1Du,0x1Eu,0x1Fu };
                    static char sighex[129];
                    uint8_t sig[64];
                    uint8_t vok = 0u;
                    int arc = DSO_Atecc608b_AuthSelfTest(chal, sig, &vok, 0u);
                    DebugUartLock();
                    if (arc == 0) {
                        DSO_Atecc608b_HexUpper(sig, 64u, sighex);
                        /* 只打签名前 32 字符：长行会长时间占住串口发送路径；
                         * 完整 128 字符签名用命令 atecc.auth_start 取 */
                        printf("[ATECC] selftest ok self_verify=%u sig[0..15]=%.32s "
                               "(full sig via atecc.auth_start)\r\n",
                               (unsigned)vok, sighex);
                    } else {
                        printf("[ATECC] selftest FAIL rc=%d st=0x%02X -> Slot0 has no usable "
                               "ECC key or SlotConfig disallows Sign; GenKey/Config = Stage 2\r\n",
                               arc, (unsigned)DSO_Atecc608b_LastStatus());
                    }
                    DebugUartUnlock();
                }
#endif
            }
        }
        else {
            /* Info 失败 → 先做**长唤醒兜底**再诊断。
             * 依据：ATECC 的 VCC 在本板是常供电，MCU 复位/重烧都断不了它；若芯片曾被畸形
             * 时钟卡在命令中途（2026-09-23~10-1x 的 ACK 短脉冲 bug），只有**充分长的 SDA 低
             * 电平**（或整板断电）才能解开。这里给 20ms 低电平（≫tWLO 60µs）后重试一次。 */
            BSP_I2C_UI_WakePulseUs(20000u);           /* 20ms SDA 低 + 内部 2ms 等待 */
            BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);
            rc = DSO_Atecc608b_InfoRevision(rev);
            DebugUartLock();
            printf("[ATECC] long-wake retry: rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
                   rc, (unsigned)DSO_Atecc608b_LastStatus(),
                   (unsigned)rev[0], (unsigned)rev[1],
                   (unsigned)rev[2], (unsigned)rev[3],
                   (rc == 0) ? "<RECOVERED: chip was wedged, long wake cleared it>"
                             : "(still failing -> see diag)");
            DebugUartUnlock();
            if (rc != 0) {
#if DSO_ATECC_DIAG_CRC_CONV
                /* **先打决定性的一臂**：CRC 约定判定 —— 由器件自己发出的两条帧反推出
                 * "它的 CRC 是标准 CRC 的位序镜像"，用 4 个候选尾字节一次验证。
                 * 一旦确认，通信当场打通，后面的逐字节 ACK / 速率扫描就没意义了，
                 * 直接跳过（省约 8 s 启动时间，日志也干净）。 */
                if (atecc_diag_crc_conv() == 0u) {
#endif
                    /* 先定位"从哪个字节开始不应答"（最直接的线索） */
                    atecc_diag_perbyte_ack();
                    /* 再把响应原样 dump（区分"芯片回错帧"与"我们读错位"） */
                    atecc_diag_info_raw();
                    /* 再扫一遍会话速率：本片对速率是否敏感，一次就能看出 */
                    atecc_rate_sweep();
#if DSO_ATECC_DIAG_CRC_CONV
                }
#endif
            }
        }
    }

    BSP_I2C_UI_SetBitDelayUs(0u);   /* 恢复默认位延迟（同总线其它器件用） */
    vTaskDelete(NULL);
}

void DSO_Atecc608b_Init(void)
{
    /* 一次性探测任务（prio 3：防 MainTask GUI_Init 卡死 spin 饿死 prio-1）。
     * 栈 1024 字：本任务里嵌套较深（AuthSelfTest → Exec 各带 group/body/frame
     * 三个 155B 缓冲）且打印较多，512 字曾偏紧。任务跑完自删，堆即回收。 */
    if (xTaskCreate(atecc608b_probe_task, "AteccP", 1024, NULL, 3, NULL)
            != pdPASS) {
        printf("[BOOT] FAIL AteccP\r\n");
    }
}
