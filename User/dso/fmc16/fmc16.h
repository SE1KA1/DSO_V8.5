/**
 * @file fmc16.h
 * @brief STM32H743 ↔ XO2-4000HC FMC16 数据包协议 —— 公共头文件。
 *
 * 定义所有模块共享的类型、常量、错误码和工具宏。
 * 本文件不依赖任何 HAL 或 RTOS 头文件，可被所有 fmc16 子模块安全包含。
 *
 * 规范来源: STM32H743_XO2_FMC16_PACKET_PROTOCOL.md
 *
 * 字序规则（协议 §3）:
 *   - 物理传输单元为 16-bit word，每次读/写一个 uint16_t
 *   - 32-bit 字段先传高 16 位，再传低 16 位
 *   - 禁止将 FMC 缓冲区强转为 C packed struct
 *   - 序号/事务号归各自 Source 节点所有，溢出后自然回绕
 */
#ifndef FMC16_H
#define FMC16_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 一、协议常量
 * ======================================================================== */

/** 帧起始标志（协议 §4） */
#define FMC16_SOF0              0xA55AU
#define FMC16_SOF1              0x5AA5U

/** 公共头版本（协议 §4）: Major=1, Minor=0, HeaderWords=13 */
#define FMC16_HEADER_VERSION    0x100DU
#define FMC16_HEADER_WORDS      13U

/** 包长限制（协议 §2） */
#define FMC16_MAX_PAYLOAD_WORDS 256U
#define FMC16_MIN_PACKET_WORDS  15U    /* 13 header + 0 payload + 2 CRC32 */
#define FMC16_MAX_PACKET_WORDS  (FMC16_HEADER_WORDS + FMC16_MAX_PAYLOAD_WORDS + 2U)  /* 271 */

/** 空闲读返回值（协议 §2） */
#define FMC16_IDLE_WORD         0xFFFFU

/** 协议版本 Major/Minor */
#define FMC16_PROTOCOL_MAJOR    1U
#define FMC16_PROTOCOL_MINOR    0U
#define FMC16_PROTOCOL_VERSION  0x0100U

/* ========================================================================
 * 二、节点地址（协议 §6.1）
 * ======================================================================== */

#define FMC16_NODE_STM32        0x00U   /**< STM32H743 */
#define FMC16_NODE_XO2          0x01U   /**< XO2 管理/网关 */
#define FMC16_NODE_XC7A         0x02U   /**< XC7A 状态源 */
#define FMC16_NODE_EXT_AGG      0x03U   /**< 扩展模块聚合器 */
#define FMC16_NODE_BROADCAST    0xFEU
#define FMC16_NODE_ILLEGAL      0xFFU

/* ========================================================================
 * 三、逻辑通道（协议 §6.2）
 * ======================================================================== */

#define FMC16_CH_LINK_MGMT      0x0000U  /**< Link management */
#define FMC16_CH_PERIODIC       0x0001U  /**< 周期状态 */
#define FMC16_CH_CONTROL        0x0002U  /**< 控制命令 */
#define FMC16_CH_EVENT          0x0003U  /**< 异步事件 */
#define FMC16_CH_BULK           0x0004U  /**< 分片/批量数据 */

/* ========================================================================
 * 四、消息类型（协议 §6.3）
 * ======================================================================== */

typedef enum {
    FMC16_MSG_LINK_HELLO       = 0x01,  /**< XO2→STM32 */
    FMC16_MSG_LINK_STATUS      = 0x02,  /**< XO2→STM32 */
    FMC16_MSG_ACK              = 0x03,  /**< 双向 */
    FMC16_MSG_NACK             = 0x04,  /**< 双向 */
    FMC16_MSG_DEVICE_INFO      = 0x05,  /**< 双向 */
    FMC16_MSG_STATUS_SNAPSHOT  = 0x10,  /**< XO2→STM32 */
    FMC16_MSG_IO_SNAPSHOT      = 0x11,  /**< XO2→STM32 */
    FMC16_MSG_MODE_SNAPSHOT    = 0x12,  /**< XO2→STM32 */
    FMC16_MSG_HOST_STATUS      = 0x13,  /**< STM32→XO2 */
    FMC16_MSG_EVENT            = 0x20,  /**< XO2→STM32 */
    FMC16_MSG_COMMAND          = 0x30,  /**< STM32→XO2 */
    FMC16_MSG_RESPONSE         = 0x31,  /**< XO2→STM32 */
    FMC16_MSG_DATA_BLOCK       = 0x40,  /**< 双向 */
    FMC16_MSG_RESET_NOTICE     = 0x7F,  /**< XO2→STM32 */
} Fmc16MsgType;

/* ========================================================================
 * 五、标志位（协议 §6.4）
 * ======================================================================== */

#define FMC16_FLAG_ACK_REQUIRED   (1U << 0)
#define FMC16_FLAG_RESPONSE       (1U << 1)
#define FMC16_FLAG_ERROR          (1U << 2)
#define FMC16_FLAG_FRAGMENTED     (1U << 3)
#define FMC16_FLAG_FIRST_FRAGMENT (1U << 4)
#define FMC16_FLAG_LAST_FRAGMENT  (1U << 5)
#define FMC16_FLAG_URGENT         (1U << 6)
#define FMC16_FLAG_RETRY          (1U << 7)

/* ========================================================================
 * 六、命令操作码（协议 §9.1）
 * ======================================================================== */

typedef enum {
    FMC16_CMD_GET_CAPABILITIES      = 0x0001,
    FMC16_CMD_GET_LINK_STATUS       = 0x0002,
    FMC16_CMD_GET_STATUS_SNAPSHOT   = 0x0003,
    FMC16_CMD_SET_MODE              = 0x0010,
    FMC16_CMD_SET_CLK_SELECT        = 0x0011,
    FMC16_CMD_SET_PUBLISH_PERIOD    = 0x0012,
    FMC16_CMD_SET_CONTROL_OWNER     = 0x0013,
    FMC16_CMD_SUBSCRIBE_EVENTS      = 0x0020,
    FMC16_CMD_UNSUBSCRIBE_EVENTS    = 0x0021,
    FMC16_CMD_LINK_RESYNC           = 0x00F0,
    FMC16_CMD_IO_CONTROL            = 0x0100,
} Fmc16CommandOpcode;

/* ========================================================================
 * 七、响应/错误码（协议 §9.2）
 * ======================================================================== */

typedef enum {
    FMC16_OK                    = 0x0000,
    FMC16_ACCEPTED              = 0x0001,
    FMC16_BAD_OPCODE            = 0x0002,
    FMC16_BAD_LENGTH            = 0x0003,
    FMC16_BAD_ARGUMENT          = 0x0004,
    FMC16_BUSY                  = 0x0005,
    FMC16_NO_CREDIT             = 0x0006,
    FMC16_TIMEOUT               = 0x0007,
    FMC16_CRC_ERROR             = 0x0008,
    FMC16_UNSUPPORTED_VERSION   = 0x0009,
    FMC16_INTERNAL_ERROR        = 0x000A,
    FMC16_READ_ONLY             = 0x000B,
    FMC16_OWNERSHIP_CONFLICT    = 0x000C,
    FMC16_STALE_GENERATION      = 0x000D,
} Fmc16StatusCode;

/* ========================================================================
 * 八、设备类型（协议 §7.1）
 * ======================================================================== */

#define FMC16_DEVICE_TYPE_STM32 0x4837U  /**< "H7" */
#define FMC16_DEVICE_TYPE_XO2   0x584FU  /**< "XO" */

/* 角色/Personality */
#define FMC16_ROLE_MASTER       0U
#define FMC16_ROLE_SLAVE        1U
#define FMC16_PERSONALITY_LOCAL 0U
#define FMC16_PERSONALITY_FMC16 1U

/* ========================================================================
 * 九、能力掩码位（协议 §7）
 * ======================================================================== */

#define FMC16_CAP_ASYNC_READ       (1U << 0)
#define FMC16_CAP_ASYNC_WRITE      (1U << 1)
#define FMC16_CAP_LEGACY_SNAPSHOT  (1U << 2)
#define FMC16_CAP_FRAGMENTATION    (1U << 3)
#define FMC16_CAP_CREDIT_FLOW      (1U << 4)
#define FMC16_CAP_EVENT_PUB        (1U << 5)
#define FMC16_CAP_DUP_CACHE        (1U << 6)
#define FMC16_CAP_ATOMIC_SNAPSHOT  (1U << 7)
#define FMC16_CAP_IO_SCHEMA_1_0    (1U << 8)
#define FMC16_CAP_PI_MIRROR        (1U << 9)
#define FMC16_CAP_PIO_PER_BIT_OE   (1U << 10)
#define FMC16_CAP_LOCAL_MODE       (1U << 11)
#define FMC16_CAP_REMOTE_MODE      (1U << 12)
#define FMC16_CAP_CLOCK_REPORTING  (1U << 13)

/* ========================================================================
 * 十、事件码（协议 §9B）
 * ======================================================================== */

typedef enum {
    FMC16_EVT_MODE_CHANGED        = 0x0001,
    FMC16_EVT_CLOCK_CHANGED       = 0x0002,
    FMC16_EVT_KEY_CHANGED         = 0x0003,
    FMC16_EVT_EXPERIMENT_CHANGED  = 0x0004,
    FMC16_EVT_PI_APPLIED          = 0x0005,
    FMC16_EVT_PO_CHANGED          = 0x0010,
    FMC16_EVT_PIO_CHANGED         = 0x0011,
} Fmc16EventCode;

/* ========================================================================
 * 十一、复位原因（协议 §10.1）
 * ======================================================================== */

typedef enum {
    FMC16_RESET_POWER_ON   = 0,
    FMC16_RESET_WATCHDOG   = 1,
    FMC16_RESET_SOFT       = 2,
    FMC16_RESET_CRC_STORM  = 3,
    FMC16_RESET_BUS_FAULT  = 4,
    FMC16_RESET_UNKNOWN    = 0xFF,
} Fmc16ResetReason;

/* ========================================================================
 * 十二、链路状态标志（协议 §10）
 * ======================================================================== */

#define FMC16_LINK_READY           (1U << 0)
#define FMC16_LINK_RX_ACCEPTING    (1U << 1)
#define FMC16_LINK_TX_AVAILABLE    (1U << 2)
#define FMC16_LINK_RESYNC_REQUIRED (1U << 3)

/* ========================================================================
 * 十三、状态快照有效性标志（协议 §8）
 * ======================================================================== */

#define FMC16_VALID_XO2_LOCAL  (1U << 0)
#define FMC16_VALID_PO_STABLE  (1U << 1)
#define FMC16_VALID_PIO_STABLE (1U << 2)

/* ========================================================================
 * 十四、Mode ID 定义（协议 §9A.1）
 * ======================================================================== */

typedef enum {
    FMC16_MODE_RESULT_DISPLAY   = 0,   /**< M0 */
    FMC16_MODE_INPUT_MAPPING    = 1,   /**< M1 */
    FMC16_MODE_DECODER_SELECT   = 2,   /**< M2 */
    FMC16_MODE_EVENT_TRIGGER    = 3,   /**< M3 */
    FMC16_MODE_PARALLEL_INPUT   = 4,   /**< M4 */
    FMC16_MODE_SCAN_DISPLAY     = 5,   /**< M5 */
    FMC16_MODE_SIGNAL_SHAPING   = 6,   /**< M6 */
    FMC16_MODE_MATRIX_KEYS      = 7,   /**< M7 */
    FMC16_MODE_ROTARY_CONTROL   = 8,   /**< M8 */
    FMC16_MODE_NAV_CONTROL      = 9,   /**< M9 */
    FMC16_MODE_CONTROL_PANEL    = 10,  /**< MA */
    FMC16_MODE_FULL_STATUS_VIEW = 11,  /**< MB */
    FMC16_MODE_REMOTE_CONTROL   = 12,  /**< MC */
    FMC16_MODE_COUNT            = 13,
} Fmc16Mode;

/* ========================================================================
 * 十五、Control Owner（协议 §9A.1）
 * ======================================================================== */

#define FMC16_OWNER_XO2_LOCAL    0U
#define FMC16_OWNER_STM32_REMOTE 1U

/* ========================================================================
 * 十六、完整包头结构（仅用于序列化参考，代码中逐 word 操作）
 * ======================================================================== */

typedef struct {
    uint16_t sof0;             /**< W0:  0xA55A */
    uint16_t sof1;             /**< W1:  0x5AA5 */
    uint16_t version_header;   /**< W2:  0x100D */
    uint16_t total_words;      /**< W3:  总 word 数 */
    uint16_t type_flags;       /**< W4:  [15:8]=MessageType, [7:0]=Flags */
    uint16_t source_dest;      /**< W5:  [15:8]=Source, [7:0]=Destination */
    uint16_t channel;          /**< W6:  逻辑通道 */
    uint16_t payload_words;    /**< W7:  Payload word 数 */
    uint16_t seq_h;            /**< W8:  32-bit 传输序号高 16 位 */
    uint16_t seq_l;            /**< W9:  传输序号低 16 位 */
    uint16_t trans_h;          /**< W10: 32-bit 事务号高 16 位 */
    uint16_t trans_l;          /**< W11: 事务号低 16 位 */
    uint16_t header_crc16;     /**< W12: W2~W11 的 CRC16 */
    /* W13… payload[0..N-1] */
    /* W(13+N)~W(14+N)  packet_crc32 [hi, lo] */
} Fmc16RawHeader;

/** 最大完整包缓冲区大小（字节） */
#define FMC16_MAX_PACKET_BYTES  (FMC16_MAX_PACKET_WORDS * 2U)

/** 静态断言：RawHeader 正好 13 words */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(Fmc16RawHeader) == FMC16_HEADER_WORDS * 2,
               "Fmc16RawHeader must be exactly 13 words (26 bytes)");
#endif

/* ========================================================================
 * 十七、系统级状态
 * ======================================================================== */

typedef enum {
    FMC16_STATE_UNINIT = 0,    /**< 未初始化 */
    FMC16_STATE_BUS_READY,     /**< FMC 总线就绪，等待 HELLO */
    FMC16_STATE_LINK_UP,       /**< 链路建立，可收发 */
    FMC16_STATE_ERROR,         /**< 错误状态 */
} Fmc16SystemState;

/* ========================================================================
 * 十八、工具宏
 * ======================================================================== */

/** 从两个 uint16_t 组合 uint32_t（高 16 位在前） */
#define FMC16_U32(hi, lo)  (((uint32_t)(uint16_t)(hi) << 16U) | (uint32_t)(uint16_t)(lo))

/** 拆分 uint32_t 为高/低 uint16_t */
#define FMC16_U32_HI(v)    ((uint16_t)((uint32_t)(v) >> 16U))
#define FMC16_U32_LO(v)    ((uint16_t)((uint32_t)(v) & 0xFFFFU))

/** 从 type + flags 组合 type_flags word */
#define FMC16_TYPE_FLAGS(type, flags) \
    ((uint16_t)(((uint16_t)(type) << 8U) | ((uint16_t)(flags) & 0xFFU)))

/** 从 source + dest 组合 source_dest word */
#define FMC16_SRC_DST(src, dst) \
    ((uint16_t)(((uint16_t)(src) << 8U) | ((uint16_t)(dst) & 0xFFU)))

/** 提取 MessageType */
#define FMC16_TYPE(type_flags)  ((uint16_t)(((uint16_t)(type_flags) >> 8U) & 0xFFU))

/** 提取 Flags */
#define FMC16_FLAGS(type_flags) ((uint16_t)((uint16_t)(type_flags) & 0xFFU))

/** 提取 Source */
#define FMC16_SRC(src_dst)      ((uint16_t)(((uint16_t)(src_dst) >> 8U) & 0xFFU))

/** 提取 Destination */
#define FMC16_DST(src_dst)      ((uint16_t)((uint16_t)(src_dst) & 0xFFU))

/** 最小/最大值 */
#define FMC16_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define FMC16_MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifdef __cplusplus
}
#endif
#endif /* FMC16_H */
