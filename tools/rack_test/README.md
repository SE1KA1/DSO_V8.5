# rack_test — 过桥板模块机架远程测试

用服务器侧工具接收板子的 `module.data` / `module.event` 遥测、按最新固件阈值判定，
并通过板子的 v0.9 命令通道做远程开关扫描与 ATECC 身份/授权检查。

**覆盖模块**：`atecc608b`、`sht40`、`ina226_u10`、`ina226_u44`、`bgm_adc_dac`、`pcal6524`。
**明确排除**：`watchdog`。它的帧仍在流里，只用于 `seq`/`ts_ms` 连续性核算，
不参与任何判定。

## 1. 数据路径

```
板子 192.168.100.50 ──(遥测 7 帧/s，TCP 客户端)──▶ 服务器 192.168.100.100:5005
服务器 ──(v0.9 命令，TCP 客户端)──▶ 板子 192.168.100.50:5005（单客户端）
```

- 遥测连接是**单向**的：板子不读服务器，服务器也不回包。
- 板子还有第二条 legacy 遥测连接，因固件 `MSG_DONTWAIT` 误判会 ~50 ms 连断一次。
  本工具对**没有发过任何字节**的连接用 RST 关闭，避免板子 4 个 LwIP PCB 被
  TIME_WAIT 占满；发过数据的连接只做正常 FIN。
- 板子命令通道**单客户端**：运行本工具时不要同时开 GUI/授权服务。

## 2. 快速开始（离线，无需板子）

```bash
cd D:/DSO_V8.5
python -m tools.rack_test dryrun --state-dir tools/rack_test/state
```

它会自动启动接收器（127.0.0.1 随机端口）、假设备（7 帧/s 精确模拟 + v0.9 命令）
和完整场景，最后在 `tools/rack_test/state/runs/<run_id>/` 生成
`report.md`、`summary.json`、`telemetry_raw.jsonl`、`events.jsonl`。

故障注入（应看到对应 FAIL）：

```bash
python -m tools.rack_test dryrun --inject seq_gap=3 --inject bad_json
python -m tools.rack_test dryrun --inject drop_module=sht40
python -m tools.rack_test dryrun --a2-storm 300 --a2-interval 0.02
```

分进程模式（便于观察）：

```bash
python -m tools.rack_test serve --bind 127.0.0.1:5005 --state-dir tools/rack_test/state
python -m tools.rack_test.fakedev --host 127.0.0.1 --port 5005 --control-port 15005
python -m tools.rack_test watch --state-dir tools/rack_test/state
python -m tools.rack_test cmd --device 127.0.0.1 --device-port 15005 get_switch
```

## 3. 真机架执行

### 3.1 预检

1. 服务器有 `192.168.100.100/24`，`ping 192.168.100.50` 通（见 `deploy/install.md`）。
2. 板子跑的是主固件（遥测含 `atecc608b` 与 `watchdog` 帧、5005 命令通道可用）。
3. **没有** GUI / 授权 HTTP 服务 / `service.py flow` 占用板子 5005。
4. 5005 在本机空闲；`serve` 与 `run` 不能同时跑。

### 3.2 运行

```bash
python -m tools.rack_test run \
  --device 192.168.100.50 --device-port 5005 \
  --bind 0.0.0.0:5005 \
  --state-dir /var/lib/dso_rack_test \
  --operator "<你的名字>" --firmware "<烧录版本/日期>" \
  --baseline 60 --soak 7200
```

- 只测数据面（跳过开关/ATECC/控制通道）加 `--telemetry-only`；在板端
  `MEMP_NUM_TCP_PCB=4` / `MEMP_NUM_NETCONN=4` 被 legacy A2 风暴占满、控制通道
  间歇性拒绝时，用这个模式先拿到干净的模块遥测报告。
- 只测控制面（开关扫描/ATECC）加 `--control-only`：**不启动遥测接收器**，
  板子的模块上报/A2 连不上服务器而释放 netconn/PCB，控制监听器独占连接池。
  要求 5005 上没有监听器（先停 `serve`）。开关用 ack + `get_switch` 回读验证，
  遥测交叉校验标 SKIP。
  - 板子 5005 是单客户端；上一次客户端异常断开后监听器可能卡住。跑控制面前
    先复位/断电重启 DSO 板，等 ~15 s。**复位后不要再开任何 TCP 客户端探测**
    （`</dev/tcp>`、nc、GUI 都会占用唯一客户端位）；直接跑 control-only，
    让工具自己的短 probe 做第一次连接。
- 默认不做重启；要测远程复位加 `--allow-reboot`。
- 要跑完整 ATECC 授权链：先停本进程控制连接，工具会自己关；加
  `--auth-user USER --auth-password PASS`（需要 `tools/auth_server` 的 `.state`
  与 `cryptography`）。`--allow-license-clear` 会清设备 License，默认关闭。
- 有信号源接 PB15 时加 `--freq-hz 1000`，频率检查会改为实测比对。

运行中另一个终端：

```bash
python -m tools.rack_test watch --run-dir /var/lib/dso_rack_test/runs/<run_id>
```

每 5 s 的标准输出摘要示例：

```
[rack] up=300s frames=7.0/s a2=6100(20.3/s) active=1 seq_gap=0 reboot=0 bad=0 mods=...
```

若遥测缺口与 A2 接受率同时上升，立即中止并保留 run 目录。

### 3.3 人工观察

运行目录里的 `human_todo.json` 是现场清单。两种填法等价：

1. 直接编辑该文件的 `answers_template`，把 `null` 改成 `true`/`false`，
   然后 `report --human-answers <run_dir>/human_todo.json`；
2. 另写一个扁平 `answers.json`（未提供的项保持 PENDING，不会自动 PASS）。

```json
{"HUM-01": true, "HUM-02": true, "HUM-03": true,
 "HUM-04": true, "HUM-05": true, "PCI-08": true}
```

```bash
python -m tools.rack_test report --run-dir <run_dir> --human-answers answers.json
```

清单内容：

1. 基线：YDS2 绿常亮、YDS3 蓝常亮、YDS1 红灭、YDS4 橙灭、蜂鸣器无声。
2. 开关扫描时量线（ON≈0V，OFF≈3.3V）：B1–B8→P0_0–P0_7，B9→P1_0，
   B11→P1_1，B12→P1_2，B14→P1_3，B13→P1_4；B10 无线；PH7（mux G#）≈0V。
3. 可选：串口 1,152,000 发 `{"cmd":"get_switch","seq":1}`，5 s 内 YDS5 应亮且
   遥测 `usb_active=1`。
4. 可选：允许重启时保存串口启动日志，检查 `[ATECC] selftest`、`[SHT40]`、
   `[INA226]`、`[SW] pcal init=0`、`[MUPL] connected`。

## 4. 判定口径（以最新代码为准）

| 模块 | 通过条件 | 备注 |
|---|---|---|
| sht40 | `temp_ok=1`、`level=0`、温度范围合法 | 靠近 58 ℃ 记 WARN；无加热手段，level 1/2/3 标 N/A |
| ina226_u10 | `level=0`、`over_cur=0`、`cur_ma≤59` | 56–59 记 WARN；无负载，过流阈值标 N/A |
| ina226_u44 | `level=0`、`over_cur=0`、`cur_ma≤136` | 128–136 记 WARN；固件不发行事件 |
| bgm_adc_dac | 无信号：`freq_valid=0`、`freq_state∈{0,1,2,4}` | 有信号源时用 `--freq-hz` |
| pcal6524 | `sw` 范围合法、bit9=0；扫描每步 ack/回读/遥测一致；恢复基线 | 回读是软件模型；物理线由人工确认 |
| atecc608b | `present=1`、`comm_ok=1`、身份字段合法、`fail_cnt=0`；`get_devid/get_pubkey/auth_start/get_license` 与遥测一致 | softkey 模式下签名用软件 P-256 |
| 完整性 | 每模块最大间隔 ≤3 s；seq 严格 +1；无坏帧；无计划外重启 | 看门狗帧计入 seq |

阈值全部可在 `config/*.json` 或 CLI 覆盖；报告会同时给出实测值与代码出处。

## 5. 已知固件问题（报告会自动列出）

1. `pcal6524.eth_link` 取的是 `yds` bit3（温度告警），不是网口链路；真实链路看
   `get_switch.eth_link`。
2. `ina226_u10/u44.valid` 恒为 1，遥测无法判通信失效。
3. `atecc608b.chip_info.rev` 是 hex 字符串（规格写 int），另有规格外的 `device_id`。
4. `ina226_u44` 不上送事件。
5. legacy 遥测连接连断风暴（本工具用 RST 缓解，不改固件）。
6. 开关回读是软件模型；B10 无硬件线；I2C 写失败被静默。
7. ATECC softkey 模式下片内 ECDSA 未参与认证；片内自检只在启动时跑，结果仅串口可见。
8. 板端 `MEMP_NUM_NETCONN=4`，而监听/控制/UDP-5006/两路遥测共 5 个 socket：
   控制通道可能瞬时被拒。工具会等/重试，仍不可用则相关项记 SKIP；跑之前关掉
   GUI/授权服务。要稳定测控制面，用 `--control-only`（停 `serve`，不启动接收器）。
9. 板子监听器没有空闲超时：控制连接若异常断掉（没有 FIN），唯一客户端位可能被
   占住；控制通道中途消失时，重启板子再跑。
   - `Connection refused` = 没有监听器（或 accept 队列满）：复位板子。
   - `timed out` = **TCP 握手成功但应用无 ack**：监听任务没有 accept/回复。
     复位后仍这样，抓 UART 1,152,000 看 `[ETH] listening :5005`、
     `[ETH] client connected`、`[ETH] acc errno`；没有 `client connected`
     就是任务没在 accept。

## 6. 单元测试

```bash
python -m unittest discover -s tools/rack_test -t . -p "test_*.py" -v
python3.9 -m compileall -q tools/rack_test      # 服务器版本下限
```

## 7. 部署

见 `deploy/install.md`：网卡别名、rsync 路径、systemd 单元、防火墙、验证命令。
