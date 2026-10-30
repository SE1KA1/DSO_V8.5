# 测试固件方案（让 5005 控制面可靠）

> **已按 1+2+3 落地（2026-10-29）**：
> - `User/dso/dso_config.h`：`DSO_RACK_TEST_BUILD = 1`
> - `LWIP/Target/lwipopts.h`：`LWIP_RACK_TEST_POOLS = 1`（TCP PCB 8 / UDP PCB 4 / NETCONN 8）
> - `User/main.c`：ETH_Up 优先级 4；关闭 `DSO_FmcTelemetry_Init()` 与 `DSO_ETH_UDP_RX_Init()`
> - 未改监听器循环和 UART 波特率（4/5 留作 Stage B）。
> 本机无 Keil，未编译；请按下面“构建与烧录”执行。

## 目的

机架板数据面（TCP `module.data`，7 帧/s）已通过；控制面（TCP 5005）在真机上
表现为 **TCP 握手成功但应用层无 ack（timed out）**，复位后依旧。当前固件不适合
继续做开关/ATECC 测试。

本方案出一个**测试构建**：不改模块逻辑，只做运行配置与监听器健壮性调整，
目标是让 5005 命令通道稳定，同时保留模块遥测与看门狗。

## 根因假设（按可能性排序）

1. **监听任务被饿死/阻塞**：`DSO_ETH_Upload_Task` 优先级只有 2，而模块上报
   （prio 3）、legacy FMC 遥测（prio 3）、YDS/蜂鸣（prio 3）都在跑；
   `dso_switch_cmd.c` 里串口任务正是因为 “MainTask 卡死 GUI_Init 并 spin，
   饿死低优先级任务” 才被提优先级。监听任务可能同样被饿死：LwIP 的 listening
   PCB 能完成三次握手（所以 `CONTROL_REACHABLE`），但应用任务不去 `accept`/
   回 ack，表现正是 `timed out`。
2. **LwIP 资源上限**：`MEMP_NUM_TCP_PCB=4`、`MEMP_NUM_NETCONN=4`；监听器 +
   UDP-5006 + 模块上报 + legacy 遥测已经 4 个，accept 新客户端需要第 5 个
   netconn，失败时连接只能排队/被 abort。
3. **单客户端无空闲回收**：`dso_eth_upload.c` 的客户端循环是阻塞 `lwip_recv`；
   客户端异常断开时监听任务可能长期卡在旧连接，后续连接无法 accept。

## 测试构建改动（按影响从大到小）

### 1. ETH 监听任务提优先级

`User/main.c` 约 589 行：

```c
/* 原：prio 2 */
if (xTaskCreate(DSO_ETH_Upload_Task, "ETH_Up", 2560, NULL, 2, NULL) != pdPASS)
```

测试构建改为 **4**（与串口命令任务同级，高于模块/legacy 的 3）：

```c
#define DSO_ETH_UP_TASK_PRIO   4
if (xTaskCreate(DSO_ETH_Upload_Task, "ETH_Up", 2560, NULL,
                DSO_ETH_UP_TASK_PRIO, NULL) != pdPASS)
```

任务在 `lwip_accept`/`lwip_recv` 上阻塞时会主动让出 CPU，不会反过来饿死别的任务。

### 2. 扩大 LwIP 连接池

`LWIP/Target/lwipopts.h` 55-60 行：

```c
#define MEMP_NUM_TCP_PCB        8   /* 原 4 */
#define MEMP_NUM_UDP_PCB        4   /* 原 2，可选 */
#define MEMP_NUM_NETCONN        8   /* 原 4 */
```

H743 的 RAM 足够；这是静态池（BSS），不占 FreeRTOS heap。测试构建建议直接改；
若要保留正式固件的 4，可用 Keil 预定义宏覆盖。

### 3. 测试构建关闭 legacy FMC 遥测与 UDP-5006

`User/dso/dso_config.h` 增加：

```c
/* 机架控制面测试构建：1 = 关闭 legacy 遥测/UDP 测试接收，释放 netconn/PCB。
 * 默认 0，正式固件不变。 */
#ifndef DSO_RACK_TEST_BUILD
#define DSO_RACK_TEST_BUILD 0
#endif
```

`User/main.c`：

```c
#if !DSO_RACK_TEST_BUILD
    DSO_FmcTelemetry_Init();      /* 已知 EWOULDBLOCK 误判 → 连断风暴 */
#else
    printf("[BOOT] legacy FMC telemetry disabled (rack test)\r\n");
#endif
...
#if !DSO_RACK_TEST_BUILD
    DSO_ETH_UDP_RX_Init();        /* 5006 吞吐测试接收 */
#endif
```

模块上报 `DSO_ModuleUpload_Init()`、看门狗 `DSO_Watchdog_Init()`、FMC16/DSO
引擎均保留。关闭 legacy 后服务器侧 `a2=0`，工具会记 “A2 client not observed”，
不影响模块判定。

### 4. 监听客户端非阻塞 + 空闲回收

`User/dso/dso_eth_upload.c` 的客户端循环（155 行起）由阻塞 `lwip_recv` 改为：

```c
unsigned long nb = 1;
(void)lwip_ioctl(s_cli, FIONBIO, &nb);
uint32_t last_rx = xTaskGetTickCount();
for (;;) {
    n = lwip_recv(s_cli, buf, sizeof(buf), MSG_DONTWAIT);
    if (n == 0) break;                       /* peer FIN */
    if (n < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if ((xTaskGetTickCount() - last_rx) > pdMS_TO_TICKS(60000)) {
                printf("[ETH] client idle timeout\r\n");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        break;                               /* reset/error */
    }
    last_rx = xTaskGetTickCount();
    /* ... 原有逐字节行式/回显处理不变 ... */
}
printf("[ETH] client disconnected\r\n");
lwip_close(s_cli); s_cli = -1; vTaskDelay(pdMS_TO_TICKS(100));
```

效果：死连接最多占用 60 s；监听任务始终能回到 `accept`。

### 5. 串口诊断改 115,200（测试构建）

服务器 `/dev/ttyUSB1` 在 1,152,000 下没有输出，可能是 USB-CH340/驱动不支持该
非标准速率。测试构建把 UART1 改为标准速率：

`User/bsp/inc/bsp_uart_fifo.h:93`：

```c
#define UART1_BAUD   115200    /* 测试构建；正式固件恢复 1152000 */
```

`DSO_SERIAL_DEBUG_ENABLE=0`，`dso_engine.c:2100` 不会覆盖该值（VOFA 关闭时）。
服务器抓串口：

```bash
stty -F /dev/ttyUSB1 115200 raw -echo
timeout 60 cat /dev/ttyUSB1
```

若仍无输出，用 `dmesg | grep -i ch340` / `udevadm info /dev/ttyUSB1` 确认端口，
或试 `/dev/ttyACM0`/`ttyACM1`。

### 6. 串口/监听诊断打印（可选）

在 `dso_eth_upload.c` 的 accept 前后加：

```c
printf("[ETH] accept wait\r\n");
int r = lwip_accept(...);
printf("[ETH] accepted fd=%d\r\n", r);
```

命令分发处打印 `[ETH] cmd: %.48s`。用于判断卡在 accept 还是命令处理。

## 构建与烧录

1. **先备份正式固件**（当前 `Objects/output.hex` 是 2026-10-23 的正常构建）：
   `Project/MDK-ARM(uV5)/Objects/output.hex` → 另存 `output-normal-20261023.hex`；
   `Project/output(mdk).hex` 也一并备份。
2. Keil `Project/MDK-ARM(uV5)/project.uvprojx`，AC5，`F7`；确认 **0 Error**。
   若链接报 RAM 不足，把 `LWIP_RACK_TEST_POOLS` 改回 0（保留优先级与关闭
   legacy 的两项）再试。
3. 产物 `Objects/output.hex` 另存为 `output-racktest.hex`。
4. ST-Link SWD：`0x08000000`，大小 `0x200000`，BOOT0=0，`F8`。
5. 复位后若串口可接（115,200 需 Stage B；当前仍是 1,152,000）应见：
   `[BOOT] legacy FMC telemetry disabled (rack test)` → `[ETH] bind :5005 OK` →
   `[ETH] listening :5005` → 模块首采/`[MUPL] connected to server`；
   不再有 `[FTEL]`/`[UDP]` 行。

## 验证顺序

1. 服务器停 `serve`，确认 5005 无监听。
2. 复位板子，等 ~15 s；**不要**先做 TCP 探测。
3. `run --control-only`：
   - 期望 8 PASS（PCI-04..07、ATECC-02..05），`progress` 中 probe 一次成功。
   - 失败时看串口：`[ETH] accepted fd=...` 是否出现、`[ETH] cmd:` 是否收到。
4. 起 `serve` / `run --telemetry-only`：期望 7 帧/s；`a2=0`（legacy 已关），
   `INT-07` 记 “A2 client not observed”。
5. 若控制面通过，可直接跑一次完整 `run`（数据+控制），不再需要分两次。

## 回滚

`DSO_RACK_TEST_BUILD` 改回 0；`UART1_BAUD` 恢复 1152000；`lwipopts` 的池可保留
8（更稳）或恢复 4；重新编译烧正式固件。

## 风险

- ETH_Up 提到 4 会抢占其他任务；它主要在 accept/recv 阻塞或 10 ms 轮询，
  风险低。
- 池从 4→8 多占几 KB 静态 RAM；看 map 确认余量。
- 关闭 legacy 遥测后没有 `device.status`/`telemetry.io` 帧；模块测试不需要。
- UART 改 115,200 后串口命令通道也变 115,200；测试用 TCP，不影响。
- 测试固件不是量产固件，验证完回滚。

## 本轮范围与后续

本轮按用户选择落地 **1+2+3**（优先级、池、关闭 legacy/UDP）。重编烧录后：

1. 服务器停 `serve`；复位板子，等 ~15 s；不要先做 TCP 探测。
2. `run --control-only`：期望 8 PASS。
3. 若通过，起 `serve` / `run --telemetry-only`：期望 7 帧/s、`a2=0`。
4. 若仍 `timed out`，再做 **4（监听器非阻塞 + 60 s 空闲回收）** 和
   **5（UART1 115,200 + accept/cmd 诊断打印）**，用串口定位。

回滚：`DSO_RACK_TEST_BUILD` 改回 0；`LWIP_RACK_TEST_POOLS` 改回 0（或保留 1，
8/8 池更稳）；重编烧 `output-normal-20261023.hex`。
