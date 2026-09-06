# python_ref — 参考编解码（后端可直接使用）

纯 Python 标准库，无第三方依赖。对应规格见 `../packet_specs/`。

## 文件

| 文件 | 作用 |
|---|---|
| `envelope.py` | 统一信封编解码 + 公共校验（所有模块复用） |
| `module_atecc608b.py` | 模块 1（ATECC608B）帧构造/解析/schema 校验 |

后续每模块新增一个 `module_xxx.py`，模式相同。

## 用法

```python
import envelope
import module_atecc608b as atecc

# 1) 构造一条模块帧（module.data / module.event）
line = atecc.make_frame(
    "module.data",
    atecc.status_data(present=1, comm_ok=1,
                      info=atecc.chip_info(serial4="a2c4f16d")),
    seq=42, ts_ms=1234567,
)
print(line)

# 2) 服务端收到一行后解析 + 校验（非法帧抛 ValueError）
frame = atecc.decode(line)          # 含信封校验 + 模块 schema 校验
print(frame["seq"], frame["data"]["chip_info"]["lock_config"])
```

命令行自测（会打印示例帧并断言）：

```bash
python envelope.py
python module_atecc608b.py
```

## 事件示例

```python
atecc.make_frame("module.event",
                 atecc.event_data("tamper", "warning", "i2c read fail x5"),
                 seq=88, ts_ms=1290300)
```

## 与真实设备流对接

真实设备按本目录规格发同一格式的行帧；服务端按行读取后调用 `decode` 即可。`seq` 可用于丢帧检测，`ts_ms` 为设备单调 ms 时间戳（与墙上时钟的换算由后端按 `device.status` 心跳校准，见 00 规格 §6）。
