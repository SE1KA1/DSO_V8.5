# LCD→PC→Web 功能映射表

> 将 DSO LCD 上的每个 UI 元素映射到 PC/Web 端可复用的数据接口。
> 所有映射通过《DSO复用接口说明》中的适配层接入，DSO 内核无修改。

## 映射总览

| # | LCD 功能 | LCD 渲染位置 | 数据源结构/API | 数据路径 | PC/Web 映射方式 |
|---|---------|-------------|---------------|---------|---------------|
| 1 | **CH1 波形** | 主绘图区 | `DSO_Frame.y_coords[608]` | Engine→UI_WaveformCtrl→MainTask | 608点 int16 数组 → Canvas/SVG 折线 |
| 2 | **CH2 波形** | 主绘图区 | `DSO_Frame.y_coords_ch2[608]` | 同上 | 同上 |
| 3 | **CH1 列包络** | 主绘图区(密集模式) | `DSO_Frame.y_column_env_top/bot_ch1[608]` | 同上 | Canvas 填充区域 |
| 4 | **CH2 列包络** | 同上 | `DSO_Frame.y_column_env_top/bot_ch2[608]` | 同上 | 同上 |
| 5 | **频率读数 CH1** | 顶部状态栏 | `DSO_Frame.freq_hz` + `freq_valid` | Engine→Frame | 文本 "12.34 kHz" |
| 6 | **频率读数 CH2** | 顶部状态栏 | `DSO_Frame.ch2_freq_hz` + `ch2_freq_valid` | 同上 | 文本 |
| 7 | **Vpp 读数 CH1** | 顶部状态栏 | `DSO_Frame.vpp` × `adc_lsb_v` | Engine→Frame | 文本 "3.30 V" |
| 8 | **Vpp 读数 CH2** | 顶部状态栏 | `DSO_Frame.ch2_vpp` × `adc_lsb_v` | 同上 | 文本 |
| 9 | **Vrms CH1/CH2** | 状态栏 | `DSO_Frame.vrms` / `ch2_vrms` | 同上 | 文本 |
| 10 | **占空比 CH1/CH2** | 状态栏 | `DSO_Frame.duty_cycle` / `ch2_duty_cycle` | 同上 | 文本 "50.0%" |
| 11 | **时基档位** | 顶部 | `DSO_Frame.tb_sec_div_ns` | Engine→Frame | 下拉选项 / 文本 |
| 12 | **采样率** | 顶部状态栏 | `DSO_Frame.sample_rate` | Engine→Frame | 文本 "33.3 MS/s" |
| 13 | **CH1 V/div** | CH1 通道标签 | `DSO_Frame.vert_mv_per_div` | Engine→Frame | 下拉选项 |
| 14 | **CH2 V/div** | CH2 通道标签 | `DSO_Frame.ch2_vert_mv_per_div` | Engine→Frame | 下拉选项 |
| 15 | **触发模式** | 触发指示区 | `DSO_Frame.trig_mode` | Engine→Frame | 按钮组 Auto/Normal/Single |
| 16 | **触发源** | 触发指示区 | `DSO_Frame.trig_source` | Engine→Frame | 下拉 CH1/CH2 |
| 17 | **触发电平** | 主绘图区触发电平线 | `DSO_Frame.trig_level_v` + `trig_level_enabled` | Engine→Frame | 数值输入 + 波形上标线 |
| 18 | **触发状态** | 触发指示区 | `DSO_Frame.triggered` | Engine→Frame | LED 指示灯 绿/红 |
| 19 | **RUN/STOP** | 顶部工具栏 | `DSO_AcquireCtrlInfo.run_requested` | AcquireCtrl API | 按钮 Run/Stop |
| 20 | **SINGLE** | 触发模式区 | `DSO_AcquireCtrlInfo.single_armed/captured` | AcquireCtrl API | 按钮 + 状态 |
| 21 | **耦合 CH1/CH2** | 通道菜单 | `DSO_VerticalInfo.coupling` (DC/AC/GND) | Vertical API | 下拉 DC/AC/GND |
| 22 | **探头倍率** | 通道菜单 | `DSO_VerticalInfo.probe` (X1/X10) | Vertical API | 下拉 |
| 23 | **垂直偏移** | 通道菜单 | `DSO_VerticalInfo.offset_v` | Vertical API | 数值滑块 |
| 24 | **水平位置(触发位)** | 水平菜单 | `DSO_AcquireCtrlInfo.trigger_position_pct` | AcquireCtrl API | 滑块 0-100% |
| 25 | **采集模式** | 采集菜单 | `DSO_AcquireCtrlInfo.acquire_mode` | AcquireCtrl API | 下拉 Sample/Peak/HiRes/Avg |
| 26 | **平均深度** | 采集菜单 | `DSO_AcquireCtrlInfo.average_depth` | AcquireCtrl API | 数值 2/4/8/16/32/64 |
| 27 | **AWG 波形类型** | AWG 面板 | `DSO_WavegenProfile.wave` | Wavegen API | 下拉 Sine/Triangle/Square |
| 28 | **AWG 频率** | AWG 面板 | `DSO_WavegenProfile.wave_hz` | Wavegen API | 数值输入 |
| 29 | **AWG 占空比** | AWG 面板 | `DSO_WavegenProfile.duty_permille` | Wavegen API | 滑块 |
| 30 | **AWG 启停** | AWG 面板 | `DSO_WavegenSnapshot.running` | Wavegen API | 按钮 On/Off |
| 31 | **AWG 欠载告警** | AWG 面板 | `DSO_WavegenSnapshot.dma_underrun` | Wavegen API | 告警指示 |
| 32 | **校准状态** | 不可见/调试 | `DSO_Frame.cal_state` + `cal_progress_pct` | Engine→Frame | 进度条 / 状态文本 |
| 33 | **溢出(OVR)告警** | 触发指示区 | `DSO_AcqDebugSnapshot.overrun_count` | Acq Debug API | 告警计数 |
| 34 | **相位同步错误** | 不可见/调试 | `DSO_AcqDebugSnapshot.phase_sync_errors` | Acq Debug API | 错误计数 |
| 35 | **外部频率计** | 频率计面板 | `DSO_Frame.ext_freq_valid` + `ext_freq_millihz` | Engine→Frame | 文本显示 |
| 36 | **外部频率计状态** | 频率计面板 | `DSO_Frame.ext_freq_state` / `ext_freq_mode` | Engine→Frame | 状态指示灯 |
| 37 | **网格背景** | 主绘图区 | 固定参数 (10×8 格) | 本地生成 | Canvas SVG 网格 |
| 38 | **CH1/CH2 颜色** | 波形线 | 固定 (黄 CH1, 青 CH2) | 本地配置 | CSS 颜色 |
| 39 | **LCD 背光亮度** | 硬件 PWM | BSP 直接控制 | 本地 | 范围滑块 |
| 40 | **Roll 模式滚动** | 主绘图区 | `DSO_Frame.roll_*` + `tb_mode==ROLL` | Engine→Frame | 自动滚动动画 |
| 41 | **ETS 指示** | 不可见/调试 | `DSO_Frame.ets_active` / `ets_building` | Engine→Frame | ETS 状态标签 |
| 42 | **等效采样率** | 状态栏 | `DSO_Frame.effective_sample_rate_hz` | Engine→Frame | 文本 |
| 43 | **采集原始 DMA 数据** | 不可见 | `DSO_Acq_GetBuf()` + `DSO_Acq_GetPos()` | 直接 API | 原始波形导出 CSV |

## 数据流架构

```
┌─────────────────────────────────────────────────────────────┐
│                       DSO 内核 (不改)                        │
│  ┌──────────┐   ┌──────────────┐   ┌──────────────────────┐ │
│  │ ADC/DMA  │──▶│ DSO_Engine   │──▶│ DSO_Frame            │ │
│  │ 原始数据  │   │ (采集+处理)   │   │ (608点 Y坐标 + 测量)  │ │
│  └──────────┘   └──────┬───────┘   └──────────┬───────────┘ │
│                        │                       │             │
│  ┌────────────────────┐│  ┌──────────────────┐│             │
│  │ AcquireCtrl API    ││  │ Vertical API     ││             │
│  │ (触发/RUN/STOP/模式)││  │ (耦合/偏移/探头)  ││             │
│  └────────────────────┘│  └──────────────────┘│             │
│                        │                       │             │
│  ┌────────────────────┐│                       │             │
│  │ Wavegen API        ││                       │             │
│  │ (AWG 配置/状态)     ││                       │             │
│  └────────────────────┘│                       │             │
└────────────────────────┼───────────────────────┼─────────────┘
                         │                       │
              ╔══════════╧═══════════════════════╧═══════════╗
              ║           适配层 (新增，不改内核)             ║
              ║  dso_adapter.c/h                              ║
              ║  - DSO_Adapter_GetFrame()   → JSON/二进制     ║
              ║  - DSO_Adapter_SetTrigger() ← JSON            ║
              ║  - DSO_Adapter_SetChannel() ← JSON            ║
              ║  - DSO_Adapter_GetAWG()     → JSON            ║
              ║  - DSO_Adapter_GetErrors()  → JSON            ║
              ╚═══════════════════════════════════════════════╝
                         │
              ┌──────────┴──────────┐
              ▼                     ▼
        ┌──────────┐         ┌──────────┐
        │  TCP 5005 │         │ WebSocket│
        │  (已有)   │         │  (新增)  │
        └─────┬─────┘         └─────┬─────┘
              │                     │
        ┌─────┴─────┐         ┌─────┴─────┐
        │ PC 客户端  │         │  Web 前端 │
        │ (NetAssist)│         │ (Canvas)  │
        └───────────┘         └───────────┘
```

## 控制参数双向映射表

| 参数 | Set API | JSON Key | 类型 | 范围 |
|------|---------|----------|------|------|
| 触发模式 | `DSO_AcquireCtrl_SetTriggerMode()` | `trig_mode` | int | 0=Auto, 1=Normal, 2=Single |
| 触发边沿 | `DSO_AcquireCtrl_SetTriggerSlope()` | `trig_slope` | int | 0=Rise, 1=Fall, 2=Both |
| 触发源 | `DSO_AcquireCtrl_SetTriggerSource()` | `trig_source` | int | 0=CH1, 1=CH2 |
| 触发电平 | `DSO_AcquireCtrl_SetTriggerLevelVolts()` | `trig_level_v` | float | -Vref..+Vref |
| 触发位置 | `DSO_AcquireCtrl_SetTriggerPositionPct()` | `trig_pos_pct` | int | 0..100 |
| RUN/STOP | `DSO_AcquireCtrl_SetRunRequested()` | `running` | bool | true/false |
| 采集模式 | `DSO_AcquireCtrl_SetAcquireMode()` | `acq_mode` | int | 0=Sample,1=Peak,2=HiRes,3=Avg |
| CHx V/div | `DSO_Vertical_SetIndexForChannel()` | `ch1_vdiv_idx` | int | 0..N (档位索引) |
| CHx 偏移 | `DSO_Vertical_SetOffsetForChannel()` | `ch1_offset_v` | float | -10..+10 |
| CHx 耦合 | `DSO_Vertical_SetCouplingForChannel()` | `ch1_coupling` | int | 0=DC, 1=AC, 2=GND |
| CHx 探头 | `DSO_Vertical_SetProbeForChannel()` | `ch1_probe` | int | 1=x1, 10=x10 |
| AWG 启停 | `DSO_Wavegen_StartFromConfig()` / `Stop()` | `awg_running` | bool | true/false |
| AWG 波形 | 编译期配置或 profile switch | `awg_wave` | int | 0=Sine,1=Tri,2=Sqr |
| AWG 频率 | 编译期 profile 或动态配置 | `awg_hz` | int | 32..250000 |

## 遥测帧格式（只读推送，适配层→PC/Web）

```json
{
  "seq": 12345,
  "ts_ms": 987654,
  "ch1": {
    "wave_y": [128, 130, 132, ...],     // 608 x int16, CH1 Y坐标
    "env_top": null,                      // 列包络上边界 (密集模式)
    "env_bot": null,                      // 列包络下边界
    "freq_hz": 12345.6,                  // 频率
    "freq_valid": true,
    "vpp_v": 3.30,                       // Vpp (V)
    "vrms_v": 1.17,                      // Vrms (V)
    "duty_pct": 50.0,                    // 占空比 %
    "vdiv_mv": 1000,                     // mV/div
    "coupling": "DC",
    "probe": "X1"
  },
  "ch2": { ... },                        // 同 CH1 结构
  "timebase": {
    "sec_div_ns": 1000000,               // 时基 ns/div
    "sample_rate_hz": 33333333,          // 采样率
    "eff_sample_rate_hz": 33333333,      // 等效采样率
    "mode": "NORMAL"
  },
  "trigger": {
    "mode": "AUTO",
    "source": "CH1",
    "slope": "RISE",
    "level_v": 1.50,
    "position_pct": 50,
    "triggered": true
  },
  "awg": {
    "running": false,
    "wave": "SINE",
    "hz": 10000,
    "duty_permille": 500,
    "dma_underrun": false
  },
  "status": {
    "running": true,
    "overrun": 0,
    "phase_sync_errors": 0,
    "cal_state": "READY",
    "cal_progress_pct": 100,
    "ets_active": false
  }
}
```
