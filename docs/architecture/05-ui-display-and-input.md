# UI、显示与输入链路

本文描述 UI 帧桥、MainTask 绘制、PlotBuf/DMA2D、LCD 旋转、LTDC 切换以及旋钮/按键输入链路。

## UI 帧桥

`User/ui/UI_WaveformCtrl.c` 是 DSO 引擎和 UI 的桥。

### Frame double buffer

写入者：

- `DSO_Engine_Task`
- 或 `ADC_FakeData_Task`，当 `USE_DSO_ENGINE == 0`

入口：

```c
void DSO_UI_SendFrame(const DSO_Frame *frame);
```

读取者：

- `MainTask`

入口：

```c
bool UI_Waveform_CopyFrame(DSO_Frame *out);
```

同步方式：

- `DSO_UI_SendFrame()` 使用 critical section 写入 back buffer。
- 标记 `s_has_new_frame`。
- `MainTask` copy 后更新显示用 `s_display_frame`。

UI 层不直接读取 DMA ring，也不做触发/频率分析。它只消费 `DSO_Frame` 和调用 DSO 控制接口。

## UI 控制接口

`UI_WaveformCtrl` 暴露给页面/导航层的主要接口：

- RUN/STOP：
  - `UI_Waveform_IsRunning()`
  - `UI_Waveform_ToggleRunStop()`
- 显示模式：
  - `UI_Waveform_GetDisplayMode()`
  - `UI_Waveform_ToggleDisplayMode()`
- CH1/CH2 solo：
  - `UI_Waveform_GetSolo()`
  - `UI_Waveform_CycleSolo()`
- 垂直位移：
  - `UI_Waveform_CH1_MoveUp/Down()`
  - `UI_Waveform_CH2_MoveUp/Down()`
- V/div 与 time/div：
  - `UI_Waveform_StepVoltPerDiv()`
  - `UI_Waveform_StepTimePerDiv()`
  - `UI_Waveform_FormatScaleStr()`
- 采样率显示：
  - `UI_Waveform_GetEffectiveSampleRateHz()`
  - `UI_Waveform_GetCh2SampleRateHz()`
- 10-bit lane 校准：
  - `UI_Waveform_RequestInterleaveCalibration()`
  - `UI_Waveform_CancelInterleaveCalibration()`
- 触发：
  - `UI_Waveform_CycleTrigMode()`
  - `UI_Waveform_AdjustTrigLevel()`
- 游标：
  - time/volt cursor mode 和位置调整。

这些接口大多转发到 `DSO_Acq`、`DSO_Timebase`、`DSO_Vertical`、`DSO_AcquireCtrl` 或 `DSO_10bitLaneCal`。

V2 底层迁移时，RUN/STOP 必须采用逻辑暂停契约：

- `UI_Waveform_IsRunning()` 的状态来源应是 `DSO_AcquireCtrl_IsRunRequested()`。
- `UI_Waveform_ToggleRunStop()` 只提交 `DSO_AcquireCtrl_SetRunRequested(true/false)`。
- UI 手动 RUN/STOP 不直接调用 `DSO_Acq_Start()` / `DSO_Acq_Stop()`。
- STOP 时高速 ADC/DMA 继续运行，engine 停止发布新帧，UI 保持上一帧。
- RUN 边沿由 engine 同步 DMA/record/roll 游标，并清除旧 trigger anchor，防止恢复后仍显示旧窗口或空 record 直线。

这项修改属于控制桥接，不属于 UI 布局迁移；`MainTask` 的 header、plot 区、PlotBuf 和旋钮页面语义不应跟随 V2 回退。

触发页当前底层接入范围：

- source：CH1/CH2，经过 `dso_trigger_source_bridge.c` 写入 `DSO_AcquireCtrl_SetTriggerSource()`；CH2 不可用时自动回到 CH1。
- slope：rising/falling/both，both 写入 `DSO_TRIGGER_SLOPE_BOTH`。
- level：`+1.25V`、`0.00V`、`OFF`。前两项启用手动触发电平并写入电压；`OFF` 调用 `DSO_AcquireCtrl_SetTriggerLevelEnabled(false)`。
- mode：Auto/Normal/Single，Single 会 rearm 并请求 RUN。
- type：仍是 UI 展示/弱 hook，不参与当前 edge trigger 底层策略。

`MainTask` 每次收到新 `DSO_Frame` 后调用 `UI_Trigger_SyncFrameState(&s_frame)`，把 engine 规整后的触发源和 level enabled 状态同步回触发页镜像，避免 8-bit 下选择 CH2 后 UI 与底层不一致。

## MainTask 绘制链路

`MainTask` 在 `User/ui/MainTask.c`。

示波器页每帧大体顺序：

1. `UI_Waveform_CopyFrame(&s_frame)` 拉取新 `DSO_Frame`。
2. 同步触发页镜像：`UI_Trigger_SyncFrameState(&s_frame)`。
3. 更新 header/status。
4. 处理 Roll coherent prefill。
5. 绘制 header、测量区、trigger badge、side menu。
6. 判断是否使用 PlotBuf：
   - `DMA2D_PlotBuf_IsReady() == true`：走 PlotBuf。
   - 否则走 GUI fallback。
7. PlotBuf 路径：
   - `DMA2D_PlotBuf_RenderScopeLock()`
   - `DMA2D_PlotBuf_StartFrame()`
   - `DrawWaveformsToBuf()`
   - 绘制逻辑 buffer 中的 header/side/y-axis/trig/cursor 等。
   - `DMA2D_PlotBuf_EndFrame()`
   - `DMA2D_PlotBuf_PreDisplaySync()`
   - `ManualRotateToPhysical()`
   - `DMA2D_PlotBuf_RenderScopeUnlock()`
8. GUI fallback：
   - `DrawGridLines()`
   - `DrawWaveforms()`
   - `ManualRotateToPhysical()`

RUN 状态下默认 `MAINTASK_FRAME_PERIOD_MS = 25`，约 40 fps。STOP 状态下每帧前有 50 ms 延时。迁移 V2 RUN/STOP 修复后，STOP 下 UI 仍可按该节奏刷新状态栏或按键反馈，但波形数据帧由 engine 逻辑冻结，不再依赖物理停止 ADC/DMA。

Header 测量区现在还显示 PB15/TIM12 外部测频：

- `UI_FreqFormat_ExtMilliHz()` 将 `s_frame.ext_freq_millihz` 格式化为 Hz/kHz/MHz。
- `EXT:%s` 与当前 MODE/CAL 状态同排显示。
- 静态 header skip 的测量签名包含 `ext_freq_valid`、`ext_freq_state`、`ext_freq_mode` 和 `UI_FreqFormat_FoldMilliHz(ext_freq_millihz)`，外部频率变化会触发 header 重绘。

## PlotBuf

PlotBuf 在 `User/ui/dma2d_wave.c` 和 `User/ui/dma2d_wave.h`。

目标：避免每帧把波形区先画到逻辑横屏再整块旋转。PlotBuf 直接维护一块竖屏方向的波形 bitmap，然后在 LCDConf 中 DMA2D 贴到物理 framebuffer 的 plot 区。

### 主要资源

- PlotBuf 双缓冲：当前绘制 buffer 与显示 buffer 分离。
- 背景 buffer：包含固定背景、网格、CH1/CH2 标签。
- 可选 guard：检测 PlotBuf 越界。

### 每帧流程

```text
DMA2D_PlotBuf_StartFrame
  -> 等待前一轮 DMA2D
  -> 准备当前绘制 buffer
  -> DrawWaveformsToBuf 使用 DMA2D_PlotBuf_DrawLine
DMA2D_PlotBuf_EndFrame
  -> 切换当前 buffer
  -> 异步恢复下一帧背景
DMA2D_PlotBuf_PreDisplaySync
  -> 等待 DMA2D
  -> 对当前绘制块做 D-Cache 同步
ManualRotateToPhysical
  -> DMA2D 贴 PlotBuf 到物理 plot 区
```

### 波形线段绘制

`USE_DMA2D_WAVEFORM_DRAW` 默认启用时：

- Bresenham 仍用于走线。
- 连续水平段/竖直段合并。
- DMA2D R2M 填充线段。

禁用时退回 CPU 逐像素绘制，主要用于排查 DMA2D 嫌疑。

## framebuffer 和旋转模型

显存布局在 `User/bsp/inc/bsp_vram_layout.h`：

| 区域 | 地址 | 尺寸 | 用途 |
| --- | --- | --- | --- |
| logical | `0xC0000000` | 800 x 480 x RGB565 | emWin 逻辑横屏 |
| physical 0 | `0xC0200000` | 480 x 800 x RGB565 | LTDC 物理 ping |
| physical 1 | `0xC0300000` | 480 x 800 x RGB565 | LTDC 物理 pong |

实际 LCDConf 在 `emWin/Config/LCDConf_Lin_Template.c`。

关键配置：

- `NUM_BUFFERS = 1`：emWin 只使用一个逻辑 framebuffer。
- 物理 framebuffer ping/pong 由 `ManualRotateToPhysical()` 管理。
- `active_phys_buffer` 记录 LTDC 当前扫描哪块物理 buffer。

### `ManualRotateToPhysical()`

功能：

1. 选择 inactive 物理 framebuffer 作为目标。
2. 根据状态选择路径：
   - 开机前几帧或请求 fullscreen：整屏 CPU 旋转。
   - PlotBuf ready：拷贝上一物理前台到后台，按需旋转 header/body，DMA2D 贴 PlotBuf。
   - PlotBuf 不可用：旋转 header + plot。
3. 对需要的 SDRAM/cache 做同步。
4. 写 LTDC layer CFBAR shadow。
5. 等待 VFP line interrupt 触发 immediate reload。
6. 更新 `active_phys_buffer`。

`LCDConf_RequestHeaderRotate()`、`LCDConf_RequestBodyRotate()`、`LCDConf_RequestFullRotate()` 可让下一帧补旋对应区域。

## LTDC 链路

LTDC 初始化在 `User/bsp/src/bsp_lcd_rgb.c`：

- 配置 RGB GPIO。
- 配置 PLL3R 像素时钟。
- 配置 LTDC timing。
- Layer pixel format 为 RGB565。
- framebuffer 初始地址指向 `LCD_RGB_FB_ADDR`，随后 `main.c` 强制切到 `VRAM_PHYSICAL_0_ADDR`。
- 开启 `LTDC_IRQn`，优先级 2。

LCDConf 中的 LTDC line interrupt 用于 VFP 同步切换 framebuffer，避免撕裂。

## 输入链路

输入由 `User/ui/ui_ec_poll_task.c` 周期调用：

```text
UI_EcPollTask
  -> UI_Nav_Poll()
  -> 读取 MCP23017 / GPB
  -> 解码 EC1/EC2 AB 相
  -> 按键 debounce / long press
  -> 调用页面或 waveform 控制接口
```

`UI_Nav_Poll()` 位于 `User/ui/ui_nav.c`。

### 示波器页旋钮语义

当前主逻辑：

- EC1 旋转：
  - 按住 EC2：`UI_Waveform_CycleSolo()`。
  - scale 模式：调整 time/div。
  - position 模式：移动 CH1 offset。
- EC2 旋转：
  - 同时按住 EC1/EC2：循环 trigger mode。
  - 按住 EC1：切换 Y-T/Roll。
  - scale 模式：调整 V/div。
  - position 模式：移动 CH2 offset。
- EC1 短按：RUN/STOP；V2 迁移后这是 `run_requested` 逻辑暂停/恢复，不是直接启停 ADC/DMA。
- EC1 长按：进入/确认菜单。
- EC2 短按：退出菜单/欢迎页。
- EC2 长按：切换旋钮模式。

### 页面输入

非 scope 页面下，EC1/EC2 detent 分发到：

- `UI_Channel_Ec*Detent()`
- `UI_Trigger_Ec*Detent()`
- `UI_System_Ec*Detent()`

`UI_System_Ec2Detent()` 是 ADC format 切换入口。

`UI_Trigger_Ec*Detent()` 当前能接入：

- trigger source：CH1/CH2。
- trigger slope。
- trigger mode。
- trigger level preset/off。
- probe x1/x10。

trigger type 仍是 UI 状态或弱 hook，尚未形成 Pulse/Video 等 engine 策略。
