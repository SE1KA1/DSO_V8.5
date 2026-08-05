# PC 客户端复刻范围与不改动清单

> 冻结原则：**DSO 现有采集内核不重写**。PC 复刻 = 将 LCD 上的"显示+交互"迁移到 PC，而非在 PC 上重新实现示波器。

---

## 一、不改动清单（DSO 固件端红线）

以下模块**禁止修改**，仅通过已发布的 API 读取数据或下发参数。

| 模块 | 涉及源文件 | 不可修改内容 | 接入方式 |
|------|-----------|-------------|---------|
| **ADC 采集后端** | `dso_acq_8bit.c`, `dso_acq_10bit.c` | ADC/TIM/DMA 配置、三交织时序、PLL2 时钟、DMA 环形缓冲 | 通过 `DSO_Frame` 读取结果 |
| **触发引擎** | `dso_acquire_control.h/c`, `dso_acq8_tim_trigger.c` | 触发比较器、TIM1 CC 相位、auto/normal/single 状态机 | 通过 `DSO_AcquireCtrl_Set*` API 下发参数 |
| **频率/幅度测量** | `dso_process.c`, `dso_freq_meter.c` | 软件过零检测、Vpp/Vrms 统计算法、外部频率计 | 通过 `DSO_Frame` 读取结果 |
| **FFT** | 未实现（仅 `DSO_FFT_SIZE 2048` 预留） | N/A | PC 端自行 FFT，DSO 只提供原始波形 |
| **LCD 渲染管线** | `MainTask.c`, `dma2d_wave.c`, `PlotBuf` | DMA2D 波形绘制、emWin 控件、LTDC 帧缓冲、旋转 | PC 端自行实现 Canvas 渲染 |
| **触摸/按键交互** | `UI_WaveformCtrl.c`, EC11 编码器, MCP23017 | 编码器扫描、长按/短按状态机、菜单导航 | PC 端自行实现鼠标/键盘交互 |
| **系统初始化** | `bsp.c`, `main.c:main()` | MPU 配置、时钟树、SDRAM/LCD 初始化、FreeRTOS 启动 | 不接触 |
| **FreeRTOS 调度** | `FreeRTOSConfig.h`, 各 Task | 任务优先级、堆栈分配、调度时序 | 不接触 |

---

## 二、DSO 固件端的最小接入（允许新增/修改）

以下**允许新增**，不修改内核逻辑：

| 新增模块 | 位置 | 功能 | 对内核的依赖 |
|---------|------|------|-------------|
| **适配层** | `User/dso/dso_adapter.c/h`（新建） | 将 `DSO_Frame` + 状态打包为 JSON 遥测帧；接收 JSON 控制指令并调用 `Set*` API | 只调用已发布 API |
| **遥测推送** | 适配层内部 | 每帧或每 N ms 通过 TCP 5005 推送遥测 JSON | 复用 `dso_eth_upload.c` 的 TCP 通道 |
| **控制接收** | 适配层内部 | 从 TCP 5005 接收控制指令，解析后调用对应 Set API | 只调用已发布 Set API |

---

## 三、PC 客户端复刻范围

### 3.1 复刻 = 显示迁移 + 交互迁移

PC 端**不实现**任何采集、触发、测量算法。所有数据来自 DSO 推送的遥测帧。

### 3.2 波形显示

| LCD 原功能 | PC 复刻方式 | 数据来源 |
|-----------|------------|---------|
| CH1 波形折线 | Canvas/SVG 折线，608 点 | `DSO_Frame.y_coords[608]` |
| CH2 波形折线 | Canvas/SVG 折线，608 点 | `DSO_Frame.y_coords_ch2[608]` |
| 密集模式列包络 | Canvas 填充区域 | `y_column_env_top/bot_ch1[608]` |
| 网格背景 (10×8) | 本地绘制，固定尺寸 | 无 |
| 通道颜色 (CH1=黄, CH2=青) | CSS 配置 | 无 |

### 3.3 测量读数

| LCD 原功能 | PC 复刻方式 | 数据来源 |
|-----------|------------|---------|
| 频率 (CH1/CH2) | 文本标签 | `freq_hz` + `freq_valid` |
| 峰峰值 Vpp | 文本标签 | `vpp × adc_lsb_v` |
| 有效值 Vrms | 文本标签 | `vrms` |
| 占空比 | 文本标签 | `duty_cycle` |
| 最大值/最小值 | 文本标签 | `vmin`, `vmax` |
| 外部频率计 | 文本标签 | `ext_freq_millihz` + `ext_freq_valid` |

### 3.4 时基与垂直档位

| LCD 原功能 | PC 复刻方式 | 数据来源 |
|-----------|------------|---------|
| 时基显示 (s/ms/μs/ns) | 下拉框或文本 | `tb_sec_div_ns` |
| 采样率显示 | 文本 | `sample_rate` / `effective_sample_rate_hz` |
| CH1 V/div | 下拉框 | `vert_mv_per_div` |
| CH2 V/div | 下拉框 | `ch2_vert_mv_per_div` |
| 耦合 (DC/AC/GND) | 下拉选择 → **控制指令** | 读: `DSO_Vertical_GetInfoForChannel()`; 写: `DSO_Vertical_SetCouplingForChannel()` |
| 探头倍率 (x1/x10) | 下拉选择 → **控制指令** | 读: `GetInfoForChannel`; 写: `SetProbeForChannel` |
| 垂直偏移 | 滑块/数值输入 → **控制指令** | 读: `GetInfoForChannel`; 写: `SetOffsetForChannel` |

### 3.5 触发控制

| LCD 原功能 | PC 复刻方式 | 数据来源 |
|-----------|------------|---------|
| 触发模式 (Auto/Normal/Single) | 按钮组 → **控制指令** | 读: `DSO_AcquireCtrl_GetInfo()`; 写: `SetTriggerMode()` |
| 触发边沿 (Rise/Fall/Both) | 下拉 → **控制指令** | 写: `SetTriggerSlope()` |
| 触发源 (CH1/CH2) | 下拉 → **控制指令** | 写: `SetTriggerSource()` |
| 触发电平 (V) | 数值输入 + 波形上标线 → **控制指令** | 写: `SetTriggerLevelVolts()` |
| 触发位置 (%) | 滑块 → **控制指令** | 写: `SetTriggerPositionPct()` |
| 触发状态指示灯 | LED 红/绿 | `triggered` |

### 3.6 RUN/STOP 与采集控制

| LCD 原功能 | PC 复刻方式 | 数据来源 |
|-----------|------------|---------|
| RUN/STOP 按钮 | 按钮 → **控制指令** | 写: `DSO_AcquireCtrl_SetRunRequested()` |
| 采集模式 (Sample/Peak/HiRes/Avg) | 下拉 → **控制指令** | 写: `SetAcquireMode()` |
| 平均深度 | 数值 → **控制指令** | 写: `average_depth` |

### 3.7 AWG/波形发生器

| LCD 原功能 | PC 复刻方式 | 数据来源 |
|-----------|------------|---------|
| 波形类型 (Sine/Tri/Sqr) | 下拉 → **控制指令** | 写: profile switch |
| 频率 | 数值输入 → **控制指令** | 写: profile switch |
| 启停 | 按钮 → **控制指令** | 写: `DSO_Wavegen_StartFromConfig()`/`Stop()` |
| 运行状态 | 状态文本 | `DSO_WavegenSnapshot.running` |
| DMA 欠载告警 | 告警指示 | `DSO_WavegenSnapshot.dma_underrun` |

### 3.8 FFT — PC 端实现

DSO 固件不提供 FFT 功能。PC 端从遥测帧中提取波形 Y 坐标（或请求原始 ADC 数据），自行执行 FFT 并绘制频谱图。这是 PC 端的**纯软件增值功能**，不涉及 DSO 固件修改。

### 3.9 校准与错误状态

| LCD 原功能 | PC 复刻方式 | 数据来源 |
|-----------|------------|---------|
| 校准状态/进度 | 进度条 + 文本 | `cal_state` + `cal_progress_pct` |
| 溢出 (OVR) 告警 | 计数器 | `overrun_count` |
| 相位同步错误 | 计数器 | `phase_sync_errors` |

---

## 四、不属 PC 复刻范围（明确排除）

以下功能在 DSO 固件中运行，**不在 PC 端重新实现**：

| 功能 | 原因 |
|------|------|
| ADC 三交织时序控制 | 硬件实时时序，PC 无法介入 |
| 触发比较器 (TIM1 CC 相位) | MCU 硬件定时器级操作 |
| DMA 环形缓冲管理 | 内存总线级操作 |
| 软件过零频率检测 | DSP 算法在 Engine 内闭环，结果已在 Frame 中 |
| LTDC/DMA2D 旋转显示 | 硬件显示管线 |
| 编码器/触摸扫描 | 硬件外设 |
| LAN8720A PHY 配置 | 已工作，无需改动 |
| FreeRTOS 任务调度 | 内核时序 |

---

## 五、数据流边界

```
┌────── DSO 固件 (不改) ──────────────────────────┐
│                                                    │
│  ADC/DMA → Engine → DSO_Frame → 环形缓冲          │
│                              ↓                     │
│                         ┌─────────┐                │
│                         │ 适配层   │ ← 唯一新增代码  │
│                         │ (JSON)  │                │
│                         └────┬────┘                │
│                              ↓                     │
│                         TCP 5005                   │
└──────────────────────────────┼──────────────────────┘
                               │
              ╔════════════════╧═══════════════════╗
              ║         PC 客户端 (复刻)            ║
              ║                                      ║
              ║  JSON 解析 → Canvas 波形             ║
              ║            → 测量读数文本             ║
              ║            → FFT 频谱 (PC 本地)      ║
              ║                                      ║
              ║  UI 控件 → JSON 控制指令 → TCP → DSO  ║
              ╚══════════════════════════════════════╝
```

**边界原则**：虚线以上是 DSO 固件（采集、触发、测量在内核中闭环），虚线以下是 PC 纯显示+交互。JSON 遥测帧是唯一的跨边界数据协议。

---

## 六、与三份 DSO 文档的一致性确认

| DSO 文档 | 关键声明 | 与本边界的一致性 |
|---------|---------|----------------|
| 用户手册 | "高速默认、精度可切" | PC 复刻不改变采集模式切换逻辑 |
| 用户手册 | "双通道、18432 点存储深度、33.3M/9.3M 采样率" | PC 端复用同一 DSO_Frame 数据 |
| 开发者文档 | "采集内核: ADC/TIM/DMA → Engine → Frame" | 适配层在 Frame 之后接入，不进入采集管线 |
| 开发者文档 | "修改采集应关注哪些测试、哪些日志" | PC 端不修改采集，此条款保留给固件开发者 |
| 工程文档 | 硬件拓扑: STM32H7 + XC7A100 + LCMXO2 | PC 端不涉及硬件配置 |
