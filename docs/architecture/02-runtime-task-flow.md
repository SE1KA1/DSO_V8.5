# 启动流程与 FreeRTOS 任务链路

本文描述从上电到示波器稳定运行的主链路。关键入口在 `User/main.c`、`User/bsp/bsp.c`、`User/dso/dso_engine.c` 和 `User/ui/MainTask.c`。

## 上电启动顺序

`main()` 位于 `User/main.c`，当前主要顺序如下：

1. `System_Init()`
   - 位于 `User/bsp/bsp.c`。
   - 配置 MPU。
   - 启用 I-Cache / D-Cache。
   - 调用 `HAL_Init()`。
   - 配置系统时钟，`configCPU_CLOCK_HZ` 为 400 MHz。
2. 暂停 tick 并屏蔽中断，用于启动关键外设阶段降低干扰。
3. `bsp_Init()`
   - 初始化 DWT。
   - 初始化基础 UART/LED 等。
   - 当前 LCD/SDRAM 工程避免初始化会占用相同 IO 的板载 key/FMC 扩展 IO。
4. `bsp_InitExtSDRAM()`
   - 初始化 32 MB SDRAM，基址 `0xC0000000`。
5. `SDRAM_QuickCheck()`
   - 对 SDRAM 基址做快速写读检查。
6. `VRAM_Sentinels_Init()`
   - 初始化物理 framebuffer 后方哨兵，辅助发现显示/DMA2D 越界。
7. `Framebuffer_Init()`
   - 清逻辑 framebuffer 与物理 ping/pong framebuffer。
   - 逻辑 framebuffer 地址来自 `bsp_vram_layout.h`。
8. `LCD_RGB_Init()`
   - 初始化 LTDC 和 LCD RGB。
9. 强制 `LTDC_Layer1->CFBAR = VRAM_PHYSICAL_0_ADDR`
   - 保证上电初始扫描物理 framebuffer 0。
10. 使能 DMA2D clock 并初始化 DMA2D wait helper。
11. `UI_Waveform_Init()`
   - 先让 UI 侧有可用的默认显示状态。
12. `RollHistory_Init()`
   - 初始化 Roll 历史 ring。
13. 创建 FreeRTOS 任务。
14. 调度器启动前执行一次 `SCB_CleanInvalidateDCache()`。
15. `vTaskStartScheduler()`。

## 任务列表

| 任务 | 创建位置 | 优先级 | 栈 | 职责 |
| --- | --- | --- | --- | --- |
| `MainTask` / `"emWin"` | `User/main.c` | 2 | 14336 words | UI 主循环，emWin 绘制，PlotBuf，ManualRotate，页面调度 |
| `DSO_Engine_Task` / `"DSOEng"` | `User/main.c` | 1 | 10240 words | 采集、处理、时基、记录、垂直映射、发布 `DSO_Frame` |
| `UI_EcPollTask` | `MainTask` 内启动 | 由 `ui_app_config.h` 控制 | 由配置控制 | 周期读取旋钮/按键并更新 UI 状态 |
| `ADC_FakeData_Task` | `USE_DSO_ENGINE == 0` 时 | 1 | 2048 words | 非真实 DSO 引擎模式下的假数据任务 |

`USE_DSO_ENGINE` 默认在 `User/dso_integration.h` 中开启，所以正常固件创建 `DSO_Engine_Task`。

## `DSO_Engine_Task` 初始化

`DSO_Engine_Task()` 位于 `User/dso/dso_engine.c`。进入任务后先初始化 DSO 子系统：

1. `DSO_Acq_Init()`
   - 恢复默认 sample format：当前为 `8-bit high-speed` 三路交替采样。
   - 初始化当前活动采集后端。
   - 若 spec 要求立即启动，则启动 ADC/DMA。
2. `DSO_Process_Init()`
3. `DSO_Timebase_Init()`
   - 默认 timebase index = 12，即 `1 ms/div`。
4. `DSO_AcquireCtrl_Init()`
   - 默认 trigger auto、rising、level = `DSO_VREF * 0.5`。
   - 默认 acquire mode = sample。
5. `DSO_Record_Init()`
6. `DSO_Vertical_Init()`
   - CH1 offset 默认 -0.4 V，CH2 offset 默认 +0.4 V。
7. `DSO_10bitLaneCal_Init()`
   - 尝试从 EEPROM 加载 10-bit lane 校准。
   - 初始化并启动 PB15/TIM12 外部测频。
8. `DSO_HS8LaneCal_Reset()`
9. `DSO_Acq_SetTimArr(DSO_Timebase_GetTimArr())`
   - 用当前时基表的 ARR 配置采样 timer。
10. `DSO_Vertical_UpdateViewTime()`
11. `DSO_Record_Reset()`
12. 可选启动 `DSO_TestGen`、VoFA、串口诊断。
13. 延时 200 ms 等待 ADC 稳定。

## 引擎主循环

引擎主循环服务周期为 `DSO_ENGINE_SERVICE_INTERVAL_MS = 1 ms`，UI 帧发布间隔为 `DSO_ENGINE_FRAME_INTERVAL_MS = 30 ms`，除非发生强制刷新。

每轮循环的高层步骤：

1. `DSO_Acq_Service()`
   - 转发给当前后端。8-bit 可执行故障策略，10-bit 当前为空服务。
2. `DSO_Acq_ApplyPendingSampleFormat()`
   - 处理 UI 请求的 10-bit/8-bit 模式切换。
3. `DSO_Timebase_ApplyPending()`
4. `DSO_AcquireCtrl_ApplyPending()`
   - V2 RUN/STOP 修复迁移后，同时应用 `run_requested` 逻辑状态。
   - 调用 `DSO_FreqMeter_Service(now)`，维护 PB15/TIM12 外部测频状态。
5. 获取当前采集状态：
   - CH1/CH2 DMA pos。
   - CH1/CH2 buffer。
   - 校准 offset。
   - CH1/CH2 sample rate。
   - overrun。
   - CH2 是否可用。
6. 处理 format/timebase/control 变化：
   - 需要时重置 roll ring feed。
   - 非 gentle roll step 时重置 fast/average/record。
   - 调整 TIM ARR。
   - 若硬件采集因模式切换或 SINGLE 完成等原因处于 stopped，且触发模式允许，再由 engine 统一重新启动采集；手动 STOP 不走这条物理停启链路。
7. 对 RollHistory ring 做增量 feed。
8. 非 Roll 模式下，如果处于 RUN 请求状态：
   - 增量拷贝新样本进入 record ring。
   - CH2 可用时同步 ingest CH2。
9. 如果处于 STOP 请求状态，engine 保持最后一帧，不生成和发布新的波形帧；高速 ADC/DMA 不因手动 STOP 被物理停止。
10. 如果未到发布间隔且没有强制刷新，则延时 1 ms 继续。
11. 根据当前时基分支：
    - Roll：使用 `DSO_Acq_CopyNewSamples()` + `DSO_Timebase_RollRender()`。
    - Y-T/Fast/Normal：使用 recent 或 record view window + `DSO_Timebase_RenderEx()`。
12. 执行触发、频率和测量分析。
13. 应用 10-bit lane 校准或 8-bit HS8 校准策略。
14. 使用 `DSO_Vertical_MapY*()` 将 ADC 样本映射到 UI plot Y。
15. 填充 `DSO_Frame` 元数据。
16. 可选输出 VoFA/串口诊断。
17. `DSO_UI_SendFrame(&frame)` 发布给 UI。

## `MainTask` 初始化

`MainTask()` 位于 `User/ui/MainTask.c`。当前主初始化流程：

1. `GUI_Init()`
2. `UI_Waveform_Init()`
3. `UI_EcPollTask_Start()`
4. 绘制或恢复示波器页面布局。
5. 尝试初始化 `DMA2D_PlotBuf_Init()`。
6. 进入 UI 主循环。

同一个文件中有历史/条件路径，但实际主线是 FreeRTOS 下的 emWin UI 循环。

## UI 主循环

UI 主循环根据当前屏幕 ID 分支：

- 示波器页或测量页：
  - `UI_Waveform_SetStreamActive(1)`。
  - 如果 STOP，前置 `vTaskDelay(50 ms)`。
  - 调用 `MainTask_Scope_RenderFrame()`。
  - RUN 状态下用 `vTaskDelayUntil(..., MAINTASK_FRAME_PERIOD_MS)` 控帧。
  - `MAINTASK_FRAME_PERIOD_MS` 默认 25 ms，约 40 fps。
- 其他页面：
  - `UI_Screen_Draw()` 绘制页面。
  - 调用 `ManualRotateToPhysical()`。
  - 以 `UI_DEMO_IDLE_POLL_MS` 延时。

## 任务间数据所有权

| 数据 | 写入者 | 读取者 | 同步方式 |
| --- | --- | --- | --- |
| ADC DMA buffer | DMA/ADC | `DSO_Engine_Task` | half/full callback 中按半区 invalidate D-Cache |
| `DSO_Frame` | `DSO_Engine_Task` | `MainTask` | `UI_WaveformCtrl.c` 双缓冲 + critical section |
| UI 控制 pending 状态 | `UI_EcPollTask`/MainTask | `DSO_Engine_Task` | pending/apply 模型，无复杂锁 |
| PlotBuf | `MainTask` | LCDConf/DMA2D | `DMA2D_PlotBuf_RenderScopeLock()` 与 DMA2D wait/cache sync |
| LTDC physical framebuffer | LCDConf/ManualRotate | LTDC | CFBAR shadow 更新 + VFP line interrupt reload |

## 运行时注意事项

- `DSO_Acq_SetSampleFormat()` 只写 pending；真正切后端发生在引擎循环中的 `DSO_Acq_ApplyPendingSampleFormat()`。
- `DSO_Timebase_SetIndex()` 只写 pending；真正切换在 `DSO_Timebase_ApplyPending()`。
- `DSO_AcquireCtrl_Set*()` 同样是 pending/apply 模型。
- V2 RUN/STOP 修复迁移后，UI 不应直接调用 `DSO_Acq_Start()`/`Stop()` 实现手动 RUN/STOP；UI 只提交 `DSO_AcquireCtrl_SetRunRequested()`，由 engine 逻辑冻结或恢复帧发布。
- Normal/Single trigger 下，如果未触发，引擎可能不发布 frame；Auto 模式会持续刷新。
