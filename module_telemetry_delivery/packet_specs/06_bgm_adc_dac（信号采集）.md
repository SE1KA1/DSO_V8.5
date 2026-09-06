# 模块 6：BGM_ADC_DAC（DSO 采样读数 / 模拟通道监测）

## 1. 硬件与用途

平台 BGM_ADC_DAC 模拟通道的控制与监测：DSO 侧已实现采样读取。本模块上报其**测量读数与采样运行态**（频率计快照 + 采集状态），供后端监控模拟通道工作状态。

数据源（User/dso/）：
- 频率计快照：`DSO_FreqMeter_GetSnapshot()`（dso_freq_meter.h）
- 采样运行态：`DSO_Acq_GetSampleFormat()/GetSampleRate()/GetOverrunCount()`（dso_acq.h）

## 2. 周期上报 `type: module.data`（1s 周期）

```json
"data": {
  "freq_valid": 1,          // int 0/1  快照有效
  "freq_state": 3,          // int      0=STOPPED 1=WAIT_EDGE 2=MEASURING 3=VALID
                            //          4=NO_SIGNAL 5=OVERRANGE 6=FAULT
  "freq_millihz": 1000000,  // int      频率（mHz；1 000 000 = 1 kHz；1e12 = 1 GHz 量级）
  "gate_us": 100000,        // int      门控时间（µs）
  "edge_count": 100000,     // int      边沿计数
  "ref_ticks": 200000,      // int      参考 tick 计数
  "overflow_count": 0,      // int      计数溢出次数
  "last_edge_age_ms": 5,    // int      最近边沿距今（ms）
  "overrange": 0,           // int 0/1  超量程
  "sample_format": 3,       // int      DSO 采样格式枚举（dso_acq_model.h；与 dsosim 的
                            //          SAMPLE/PEAK/HIRES/AVERAGE 序对应，序号以固件为准）
  "sample_rate": 100000000, // int      采样率（Hz）
  "overrun_count": 0        // int      采集 overrun 累计
}
```

## 3. 事件
暂无（采样读数周期性足够；后续如需阈值告警再补）。

## 4. 示例完整帧

```json
{"ver":"1.0","type":"module.data","module":"bgm_adc_dac","seq":50,"ts_ms":5100000,"data":{"freq_valid":1,"freq_state":3,"freq_millihz":1000000,"gate_us":100000,"edge_count":100000,"ref_ticks":200000,"overflow_count":0,"last_edge_age_ms":5,"overrange":0,"sample_format":3,"sample_rate":100000000,"overrun_count":0}}
```

## 5. 后端解析要点
- `freq_state` 用整数并对照上表字符串；`freq_millihz` 转 Hz 除以 1000（后端避免浮点可用整数运算）。
- `sample_format` 序号含义以固件 `dso_acq_model.h` 为准（与 dsosim 模拟器字段可对拍验证）。
