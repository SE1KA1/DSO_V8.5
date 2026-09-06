# python_ref — 参考编解码（后端可直接使用，纯 stdlib 零依赖）

对应规格见 `../packet_specs/`。每个 `module_*.py` 独立：构造帧、解析+校验、命令行自测。

## 文件

| 文件 | 模块 | data 帧字段 |
|---|---|---|
| `envelope.py` | 公共信封 | 所有模块复用（ver/type/module/seq/ts_ms/data） |
| `module_pcal6524.py` | 模块 2 | sw / yds / eth_link / usb_active |
| `module_sht40.py` | 模块 3 | temp_dezi / temp_ok / hi_alarm / level |
| `module_ina226_u10.py` | 模块 4 | cur_ma / over_cur / valid / level |
| `module_ina226_u44.py` | 模块 5 | cur_ma / over_cur / valid / level |
| `module_bgm_adc_dac.py` | 模块 6 | freq_* / sample_* / overrun_count |

## 用法

```python
import module_sht40 as sht40

# 1) 构造一条模块帧（module.data / module.event）
line = sht40.make_frame(
    "module.data",
    sht40.data(temp_dezi=245, temp_ok=1, hi_alarm=0, level=0),
    seq=10, ts_ms=1500000,
)
print(line)

# 2) 服务端收到一行后解析 + 校验（非法帧抛 ValueError）
frame = sht40.decode(line)
print(frame["seq"], frame["data"]["temp_dezi"])
```

命令行自测（打印示例帧并断言字段范围/坏帧拒绝）：

```bash
python envelope.py
python module_pcal6524.py
python module_sht40.py
python module_ina226_u10.py
python module_ina226_u44.py
python module_bgm_adc_dac.py
```

## 事件帧示例（module.event）

```python
module_sht40.make_frame("module.event",
                        sht40.event_data("sensor_lost", "warning", "10 consecutive read fails"),
                        seq=11, ts_ms=1600500)
```

## 与真实设备流对接

真实设备按本目录规格发同一格式的行帧（TCP CRLF 一行一帧）；服务端按行读取后调用对应 `module_xxx.decode(line)` 即可。`seq` 用于丢帧检测，`ts_ms` 为设备单调 ms（与墙上时钟换算由后端按 device.status 心跳校准，见 00 规格）。

> 校验只做结构/范围检查（`level` 0..3 等），**不做阈值判定**——阈值语义见各规格分级表，由展示/告警层使用。
