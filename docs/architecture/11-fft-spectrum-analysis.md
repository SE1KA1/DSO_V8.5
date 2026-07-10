# FFT 频谱分析实施方案

## 目标

在当前示波器工程中新增频谱分析能力，优先支持 CH1 的实时 FFT 频谱显示，并为后续 CH2、冻结帧、峰值/谐波/噪声指标扩展留出稳定接口。

## 官方依据

- Arm CMSIS-DSP 官方文档：`arm_rfft_fast_init_f32()` / `arm_rfft_fast_f32()`，支持 32..4096 点实数 FFT。
- Arm CMSIS-DSP 官方文档：`arm_cmplx_mag_f32()` 用于复数幅值计算。
- Arm CMSIS-DSP 官方文档：窗口函数建议上，普通窗适合检波/噪声观察，平顶窗更适合幅度估计。
- CMSIS-DSP 官方说明：库推荐按目标编译并使用较高优化等级；MDK/Keil 可直接链接 Cortex-M7 FPU 对应的静态库。

## 当前工程状态

- `User/dso/dso_config.h` 已预留 `DSO_FFT_SIZE = 2048u`。
- 已导入并链接 `Libraries/CMSIS/DSP/Lib/ARM/arm_cortexM7lfsp_math.lib`。
- 已导入 `Libraries/CMSIS/DSP/Include/` 的官方 DSP 头文件，供未来频谱模块直接包含。
- Keil 工程已增加 `ARM_MATH_CM7` 定义和 DSP 静态库链接路径。
- 现有 `Libraries/CMSIS/Include/arm_math.h` 已覆盖 FFT / magnitude 相关 API；窗口系数初版建议在 `User/dso/` 内自实现，不额外依赖更高版本 window API。

## 设计决策

1. 采用 `arm_rfft_fast_f32()` 作为唯一 FFT 路径。
2. 初版固定使用 `DSO_FFT_SIZE = 2048`，不做运行时可变长度。
3. 初版只处理 CH1，避免和 CH2 / roll / record 的复杂交叉耦合。
4. 初版频谱页优先面向 Y-T / frozen capture；roll 模式不作为首版主路径。
5. 初版使用单一 window 策略，建议 Hann 窗（可由 `User/dso/` 预计算系数实现）；若后续更重视幅度准确度，再切换平顶窗。

## 数据流

1. 采集后端产出当前帧样本。
2. 引擎侧取出连续的 `DSO_FFT_SIZE` 个样本。
3. 将 ADC 码值转换为以 0 为中心的 `float32_t` 序列。
4. 乘窗。
5. 通过 `arm_rfft_fast_f32()` 做实数 FFT。
6. 对 packed 结果做 bin 解包与幅值计算。
7. 计算峰值频点、基波频率、幅度刻度、噪声底。
8. 将结果交给 UI 频谱页渲染。

## 模块划分

- `User/dso/dso_spectrum.c/.h`：频谱分析核心，封装 CMSIS-DSP 调用。
- `User/dso/dso_engine.c`：决定何时采样一帧用于频谱分析，并向 UI 发布谱数据。
- `User/ui/`：新增频谱页面与导航入口。

## 接口建议

```c
typedef struct {
    uint32_t fft_size;
    float sample_rate_hz;
    float bin_hz;
    uint32_t peak_bin;
    float peak_hz;
    float peak_mag;
    float dc_mag;
    bool valid;
} DSO_SpectrumMeta;
```

建议再配一组只读输出：

```c
float spectrum_mag[DSO_FFT_SIZE / 2u + 1u];
```

## 实施阶段

### Phase 1: 库接入

- 确认 MDK 链接 `arm_cortexM7lfsp_math.lib`。
- 保持 `ARM_MATH_CM7` 定义。
- 先不改现有时域链路。

### Phase 2: 核心算法

- 新建 `dso_spectrum.c/.h`。
- 实现样本归一化、乘窗、RFFT、幅值、峰值搜索。
- 先只输出线性幅值，再补 dB 标尺。

### Phase 3: UI 页面

- 新增 `UI_SCR_SPECTRUM`。
- 画频率轴、幅值轴、峰值标记、游标。
- 复用现有导航框架，不改动采集模式。

### Phase 4: 校验

- 1 kHz / 10 kHz / 100 kHz 标准正弦输入验证 bin 位置。
- 0 dBFS 与满幅输入验证幅度标定。
- 噪声输入验证底噪与窗函数效果。
- 对比关闭/开启窗函数时的旁瓣表现。

## 风险点

- `arm_rfft_fast_f32()` 会修改源缓冲，必须先复制样本。
- packed RFFT 输出不能直接按普通复数数组处理，DC / Nyquist 需要单独处理。
- 若输入窗口选择不当，会造成泄漏和幅度偏差。
- 2048 点 FFT 需要额外 RAM，建议使用 D1/AXI SRAM 的对齐缓冲区。

## 结论

这个方案优先走单一路径：`CH1 -> 连续样本 -> Hann 窗 -> 2048 点 RFFT -> 幅值 -> 频谱 UI`。先把底层管线做稳，再考虑 CH2、冻结谱、谐波表和 THD/SFDR。
