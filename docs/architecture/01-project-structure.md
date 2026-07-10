# 项目结构与源码分层

本文记录当前工程的目录职责和关键文件分工。后续新增模块时，优先保持这些边界稳定。

## 顶层目录

| 路径 | 作用 | 维护建议 |
| --- | --- | --- |
| `User/` | 产品代码主体 | 功能代码优先放这里 |
| `User/dso/` | 示波器核心链路 | 采集、处理、时基、记录、校准、串口诊断 |
| `User/ui/` | UI 和 emWin 对接层 | 主界面、波形绘制、导航、页面 |
| `User/bsp/` | 板级支持 | 时钟、MPU/cache、SDRAM、LCD、UART、IO |
| `User/segger/` | SEGGER HardFault | 仅故障处理相关 |
| `tests/` | host-side C 回归测试 | `test_*.c` 和对应 `.exe` runner |
| `Project/MDK-ARM(uV5)/` | Keil 工程 | 当前 checkout 存在并注册了主要源码 |
| `Project/EWARMv8/` | IAR 工程 | 仓库说明提到，但当前 checkout 未发现该目录 |
| `Doc/` | 历史中文说明 | 当前主要是显示旋转和波形绘制优化记录 |
| `docs/superpowers/` | 历史设计/实施计划 | 保留 8-bit/10-bit 架构演进背景 |
| `docs/architecture/` | 当前架构维护文档 | 本文档集 |
| `FreeRTOS/` | RTOS vendor 代码 | 不建议直接改 |
| `emWin/` | emWin vendor 与配置 | `emWin/Config` 是本项目实际显示配置 |
| `Libraries/` | STM32 HAL/CMSIS 等 vendor 代码 | 不建议直接改 |

## `User/dso/` 模块分层

### 采样格式和共享采集入口

| 文件 | 职责 |
| --- | --- |
| `dso_sample_spec.h/.c` | 定义产品采样格式规格。当前有 `10-bit` 和 `8-bit high-speed` 两类，默认上电为 `8-bit high-speed` 三路交替采样。 |
| `dso_acq.h/.c` | 采集 facade。维护当前/待切换 sample format、8-bit pending topology，并把统一 API 分发给 10-bit 或 8-bit 后端。 |
| `dso_acq_internal.h` | 采集内部共享定义。 |
| `dso_acq_model.h` | 根据芯片 revision、ADC 时钟、TIM 时钟、采样周期估算运行时采样率和 profile。字段名中仍有 `tim8` 历史命名，10-bit 后端也复用该模型保存 TIM1 时钟。 |
| `dso_acq_hal_stm32h7.c` | STM32H7 HAL IRQ/callback 桥接，把 DMA IRQ 和 ADC half/full callback 转到 `DSO_Acq_*` facade。 |
| `dso_cache_range.h/.c` | D-Cache range 对齐辅助。 |

### 10-bit 专属采集与增强

| 文件 | 职责 |
| --- | --- |
| `dso_acq_10bit.h/.c` | 10-bit HAL 后端。配置 PLL2 ADC 时钟、GPIO、ADC1/2/3、TIM1、DMA，并提供 start/stop、pos、copy、debug snapshot。 |
| `dso_acq10_stream.h/.c` | 10-bit 双 lane 合并和 ring-copy 纯 C helper，适合 host 测试。 |
| `dso_acq10_tim_trigger.h/.c` | 10-bit TIM trigger 模式选择。当前返回 `PWM1`。 |
| `dso_10bit_lane_cal.h/.c` | 10-bit CH1 双 lane 校准，求解 offset/gain/skew，支持 EEPROM 持久化。 |
| `dso_10bit_display_sync.h/.c` | CH1/CH2 触发位置按采样率互相映射；限制 HS10 只用于 CH1。 |
| `dso_hs10_render.h/.c` | 10-bit 高频重复信号增强重建，128 phase bins。 |

### 8-bit 专属采集与增强

| 文件 | 职责 |
| --- | --- |
| `dso_acq_8bit.h/.c` | 8-bit 后端。V2 移植目标是仅保留 tri interleaved（三路交替采样）产品路径。 |
| `dso_acq8_sample_unpack.h/.c` | 8-bit 样本解包、低字节/高字节检测、双/三 lane copy helper。 |
| `dso_acq8_tim_trigger.h/.c` | 8-bit TIM trigger 模式选择。 |
| `dso_acq8_fault_policy.h/.c` | 8-bit 故障策略。当前 `auto_fallback_to_12bit` 默认 false，是历史命名。 |
| `dso_acq8_refresh_policy.h/.c` | 8-bit preview refresh 策略。 |
| `dso_hs8_lane_cal.h/.c` | 8-bit lane 校准。 |
| `dso_hs8_render.h/.c` | 8-bit 高频重复信号增强重建。 |
| `dso_lane_merge.h/.c` | 多 lane common-delta 和对齐 merge primitive。 |

V2 移植后，8-bit 架构不再保留 single safe / dual interleaved 作为产品路径；相关文件只服务三路交替采样，不通过隐藏 fallback 或兼容分支继续维持旧路径。

### 引擎、处理、时基和记录

| 文件 | 职责 |
| --- | --- |
| `dso_engine.h/.c` | DSO 主引擎任务。串联采集、增量 ingest、触发/测量、记录、roll、时基渲染、垂直映射、UI frame 发布。 |
| `dso_acquire_control.h/.c` | 触发模式、触发电平、触发斜率、采集模式、平均、记录深度、single arm 状态。 |
| `dso_process.h/.c` | 原始样本分析、触发点、频率、重复性置信度、clip、vmin/vmax/vpp/vavg/vrms/duty。 |
| `dso_timebase.h/.c` | 时基表、窗口样本数、normal/fast render、ETS/HS8/HS10、roll render。 |
| `dso_vertical.h/.c` | V/div、offset、probe、coupling、ADC 样本到 plot Y 的映射。 |
| `dso_record.h/.c` | 深存储 ring，支持 CH1/CH2、single capture freeze、view window。 |
| `dso_trigger_source_bridge.c` | UI 触发源到 `DSO_AcquireCtrl_SetTriggerSource()` 的桥接；CH2 不可用时规整回 CH1。 |
| `dso_freq_meter.h/.c` | PB15/TIM12 外部频率计状态机；engine 默认初始化、启动并周期 service。 |
| `dso_freq_meter_math.h/.c` | 频率计纯数学层：period/gate 换算、16-bit overflow 扩展、低频 capture 与高频 gated count 模式选择。 |
| `dso_freq_meter_hw_tim12.h/.c` | TIM12_CH2/PB15 硬件层、input capture、external clock gated count、`TIM8_BRK_TIM12_IRQHandler()`。 |
| `dso_render_policy.h/.c` | 渲染重建策略决策。 |
| `dso_roll_physics.h/.c` | Roll 模式 samples-per-pixel 等物理计算。 |
| `dso_roll_line_builder.h/.c` | 从历史 ring 构造 roll decimated line。 |
| `roll_history_mem.h/.c` | SDRAM 末端 RollHistory 内存划分。 |
| `roll_history_ring.h/.c` | RollHistory CH1/CH2 ring 操作。 |

### 调试和数据输出

| 文件 | 职责 |
| --- | --- |
| `dso_serial_debug.h/.c` | 结构化串口调试，采集 snapshot、raw dump、frame log。 |
| `dso_serial_debug_format.h/.c` | 串口调试输出格式化。 |
| `dso_serial_debug_math.h` | 串口调试数学辅助。 |
| `dso_vofa_firewater.h/.c` | VoFA+ FireWater 波形输出。 |
| `dso_testgen.h/.c` | 内部测试信号源，默认关闭。 |

## `User/ui/` 模块分层

| 文件 | 职责 |
| --- | --- |
| `MainTask.h/.c` | emWin UI 主任务。绘制示波器主界面、测量值、触发 badge、菜单、波形。 |
| `UI_WaveformCtrl.h/.c` | UI 与 DSO 引擎的桥。接收 `DSO_Frame`，提供 RUN/STOP、档位、触发、电压/时间格式化等接口。 |
| `dma2d_wave.h/.c` | PlotBuf 双缓冲、背景恢复、DMA2D 绘制波形线段、PlotBuf cache 同步。 |
| `ui_nav.h/.c` | 双编码器/按键导航和屏幕状态机。 |
| `ui_ec_poll_task.h/.c` | FreeRTOS 轮询任务，周期调用 `UI_Nav_Poll()`。 |
| `ui_screens.h/.c` | 菜单/页面绘制调度。 |
| `ui_channel_ctrl.h/.c` | 通道 enable 状态。当前回调是弱符号，未深度接入采集。 |
| `ui_trigger_ui.h/.c` | 触发页 UI 状态。source/slope/mode/level 已接到底层；type 仍是 UI 展示/弱 hook，不进入 engine 主触发策略。 |
| `ui_freq_format.h/.c` | 外部测频显示格式化：频率、周期、测频模式和 64-bit mHz 折叠签名。 |
| `ui_system_setup.h/.c` | 系统设置页。ADC 格式切换顺序为 `8-bit (3L) -> 10-bit -> 8-bit (3L)`，反向同样只在两项之间切换。 |
| `roll_history_ui.h/.c` | Roll history UI 侧预填/连贯显示辅助。 |
| `lcd_rotate_request.h` | 请求 LCDConf 下帧旋转 header/body/fullscreen 的轻量接口。 |
| `lcd_rotate_profile.h/.c`、`MainTask_profile.h`、`profile_opts.h` | 显示旋转和 MainTask 分段耗时 profiling。 |
| `ui_theme.h`、`ui_app_config.h`、`ui_display_light_path.h`、`ui_perf_baseline.h` | UI 配置、主题和性能相关选项。 |

## `User/bsp/` 模块分层

| 文件 | 职责 |
| --- | --- |
| `bsp.c/.h` | `System_Init()`、MPU、Cache、HAL、400 MHz 系统时钟、基础 BSP 初始化。 |
| `src/bsp_fmc_sdram.c`、`inc/bsp_fmc_sdram.h` | FMC SDRAM 初始化、32 MB SDRAM 基址/大小、SDRAM 测试。 |
| `src/bsp_lcd_rgb.c`、`inc/bsp_lcd_rgb.h` | RGB LCD/LTDC 初始化、LTDC GPIO、layer 配置、LTDC IRQ。 |
| `inc/bsp_vram_layout.h` | 逻辑/物理 framebuffer 地址和尺寸的单一来源。 |
| `inc/bsp_sdram_layout.h` | GUI heap 与 record buffer 的 SDRAM 区域划分。 |
| `src/dma2d_wait.c`、`inc/dma2d_wait.h` | DMA2D 等待/同步辅助。 |
| `src/vram_sentinels.c`、`inc/vram_sentinels.h` | 显存越界哨兵检测。 |
| `src/bsp_uart_fifo.c`、`inc/bsp_uart_fifo.h` | COM/UART FIFO，用于 printf、串口调试、VoFA。 |
| `src/bsp_ui_io_expander.c`、`inc/bsp_ui_io_expander.h` | UI IO expander，供旋钮按键读取。 |
| `src/bsp_led.c` | LED1-3 仍按板级 GPIO 初始化；PB15/LED4 默认关闭，因为 PB15 被 TIM12_CH2 外部测频占用。 |

`User/bsp/inc/` 与 `User/bsp/src/` 还保留大量安富莱/板卡通用外设驱动。当前 DSO 主链路只依赖其中一部分，新增 DSO 功能不要无目的地扩大 BSP 依赖。
