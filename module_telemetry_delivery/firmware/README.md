# 五模块数据上报固件（精简可烧录 Keil 工程）

本目录是「module_telemetry_delivery」配套的**精简固件工程**：同事拿到后，在 Keil uVision5 编译 → ST-Link 下载到板子 → 上电即以 TCP 客户端连接服务器，**每秒推送 5 个模块的数据帧**（JSON，CRLF 分行）。

目标硬件与现 DSO 板相同：**STM32H743IITx + LAN8720A（RMII）+ PB7/PB8 软件 I2C 上的 PCAL6524(0x22)/SHT40(0x44)/INA226 U10(0x41)/U44(0x40)**。

## 目录结构（相对本文件）

```
firmware/
├─ README.md                      本文件
├─ Project/MDK-ARM(uV5)/
│  ├─ project.uvprojx             精简 Keil 工程（输出 Objects\telemetry.hex）
│  └─ stm32-v7.sct                分散加载（与主固件相同）
├─ User/
│  ├─ main.c                      精简入口
│  ├─ mod_tel_conf.h              ★网络配置（板 IP / 服务器 IP:端口 都在这改）
│  ├─ bsp/                        BSP/时钟/MPU/I2C/传感器底层（bsp.c 等）
│  └─ dso/                        五模块上报核心 + 驱动 + 桩
│     ├─ dso_module_upload.c      上报任务（裁剪副本：5 模块，不含 watchdog 帧）
│     ├─ module_5ch_stub.c/.h     桩（bgm 占位 + USB/静音占位）
│     ├─ dso_switch_panel.c       开关面板（PCAL 总线拉起 + 开关位图）
│     ├─ dso_sht40.c  dso_ina226.c  dso_ina226_u44.c  传感器驱动
│     └─ dso_watchdog.c           SP706T 喂狗任务（只喂狗，不上报 watchdog 帧）
├─ LWIP/  Drivers/  Libraries/  FreeRTOS/  Middlewares/  网络栈 + 中间件（自包含）
└─ packet_specs 见上层 ../packet_specs/（帧格式协议）
```

> 工程已**自包含**（含 HAL/CMSIS/LwIP/FreeRTOS/lan8742），不依赖主 DSO 工程。

## 编译

1. 安装 **Keil MDK-ARM uVision5** + **ARM Compiler 5**（AC5；本工程用 AC5，不支持 AC6）+ **STM32H7xx_DFP** 器件包（Keil Pack Installer 装）。
2. 打开 `Project\MDK-ARM(uV5)\project.uvprojx`（目标 Flash）。
3. `F7` 编译，0 Error；产物 `Objects\telemetry.hex`。

## 烧录

- ST-Link 接 SWD，Target→Options→Debug→Flash Download：算法 `STM32H7x_2048.FLM`，起始 `0x08000000`，大小 `0x200000`（已内嵌，一般自动选好）。
- `F8` 下载。BOOT0=0（Flash 启动），无需其它跳线。

## 接线与现象

- **以太网**：板网口（LAN8720A RMII）连到 `192.168.100.x/24` 网段（可直连 PC 网卡并把 PC 设为该段静态 IP）。
- **服务器**：数据接收端需监听 `192.168.100.100:5005`（TCP Server；NetAssist 或 python_ref 均可）。
- **串口（可选）**：USB-CH340 接 UART1，波特率 **1,152,000**（不是 115200）。
- **上电后**：
  - 串口依次出现 `[BOOT]`、传感器首采 `[SHT40]/[INA226]/[INA226U44]`、`[WDOG]`（喂狗在跑，板不会约 1.6s 周期复位）、约 3s 后 `[MUPL] connected to server`；
  - TCP 每秒收到 **5 行** `module.data`：`sht40`、`ina226_u10`、`ina226_u44`、`bgm_adc_dac`（`freq_valid:0` 占位）、`pcal6524`；`seq` 单调递增；
  - 正常态：**YDS2 绿**（电流正常）与 **YDS3 蓝**（温度正常）常亮；过流/超温 → **YDS1 红 / YDS4 橙**按 500/250/100ms 半周期闪；蜂鸣器（U10 ALERT）level0 静音、预警响 250ms/停 1750ms、告警 250/250、紧急 100/100（与 LED 同步）。

## 配置（改 `User\mod_tel_conf.h`）

| 宏 | 默认 | 含义 |
|---|---|---|
| `MOD_TEL_BOARD_IP*` | 192.168.100.50 | 板子静态 IP |
| `MOD_TEL_NETMASK*` | 255.255.255.0 | 掩码 |
| `MOD_TEL_GW*` | 192.168.100.1 | 网关 |
| `FMC_TEL_SERVER_IP*` / `FMC_TEL_SERVER_PORT` | 192.168.100.100 / 5005 | **服务器地址** |

改完重编即可。

## 模块与帧格式

见上层 `../packet_specs/`：
- `00_envelope_framework.md`：统一信封（先读）
- `02_pcal6524` `03_sht40` `04_ina226_u10` `05_ina226_u44` `06_bgm_adc_dac`：五模块 data schema / level 阈值 / 声光语义

校验/演示可直接跑 `../python_ref/` 里的 `module_*.py`（纯 stdlib）。

## 与主固件的差异（重要）

- 本工程 `User/dso/dso_module_upload.c` 是**裁剪副本**：仅 5 模块、无 watchdog 帧、bgm 走占位；与主 DSO 固件 `User/dso/dso_module_upload.c` 同名但相互独立。
- 不含模块 1（ATECC 密码芯片）、7（BGM_DDS）、8（watchdog 数据帧）。watchdog **硬件喂狗保留**（不喂 SP706T 会复位整板）。
- 无 LCD/GUI/FPGA/示波器采集/开关命令串口；bgm 帧是占位无效帧（未采集真实频率/采样）。
- 若需远程开关/静音下行命令，属主固件 + pc_switch_panel 通道，不在本精简工程范围。
