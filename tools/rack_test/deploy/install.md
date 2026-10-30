# 机架服务器部署（Linux）

目标：服务器在板子同二层网段上拥有 `192.168.100.100/24`，接收器监听
`0.0.0.0:5005`，并能从服务器主动连板子 `192.168.100.50:5005`。

> **2026-10-29 实测**：板子已在线（`ping 192.168.100.50` 通）；服务器
> `eno2` 上**已经有** `192.168.100.100/24`，`eth0` **不存在**。请先按 §1 找到
> 实际网卡；已有地址就跳过添加。先做 §4 的无 sudo 快速试跑，确认遥测与控制
> 通道正常，再做 §6 的服务化。

## 1. 网络预检（按实际网卡名）

```bash
# 找到去往板子的网卡（本机应为 eno2）
IFACE=$(ip route get 192.168.100.50 | awk '{for(i=1;i<=NF;i++) if($i=="dev") print $(i+1)}')
echo "IFACE=$IFACE"
ip -4 addr show dev "$IFACE"

# 已有 192.168.100.100/24 就不要再加；没有才加
ip -4 addr show dev "$IFACE" | grep -q '192.168.100.100/24' \
  || sudo ip addr add 192.168.100.100/24 dev "$IFACE"

ping -c 1 192.168.100.50
# 控制通道（板子自己的 5005 监听器）是否可连
timeout 3 bash -c '</dev/tcp/192.168.100.50/5005' \
  && echo CONTROL_REACHABLE || echo CONTROL_UNREACHABLE
```

`CONTROL_UNREACHABLE` 时：确认板子跑的是**主固件**（不是 5 模块精简版），且
上电已超过约 8 s（监听器在 LwIP 初始化后再延迟 3+3+2 s 才 listen）。

持久化（仅在没有地址时需要）。Ubuntu netplan 示例，网卡名按 `$IFACE` 替换：

```yaml
# /etc/netplan/60-dso-rack.yaml
network:
  version: 2
  ethernets:
    eno2:
      dhcp4: true
      addresses: [192.168.100.100/24]
```

```bash
sudo netplan try && sudo netplan apply
```

NetworkManager：

```bash
nmcli -t -f NAME,DEVICE con show
sudo nmcli con mod "Wired connection 1" +ipv4.addresses 192.168.100.100/24
sudo nmcli con up "Wired connection 1"
```

不要把加地址写进 systemd 单元：地址是网络配置，不是应用启动的一部分。

## 2. 同步代码（开发机 → 服务器家目录，无 sudo）

已生成现成包 `tools/rack_test/dist/rack_test_bundle.tar.gz`（64 KB），可跳过下面的
tar 流，直接：

```powershell
scp D:\DSO_V8.5\tools\rack_test\dist\rack_test_bundle.tar.gz backend@192.168.1.139:~/
```

```bash
# 服务器侧
mkdir -p ~/dso_rack_test && tar xzf ~/rack_test_bundle.tar.gz -C ~/dso_rack_test
```

或者从**开发机**仓库根目录用 tar 流（无需先打包）：

```bash
cd /d/DSO_V8.5
tar --exclude='__pycache__' --exclude='state' --exclude='*.pyc' -czf - \
  tools/__init__.py tools/rack_test \
  tools/auth_server/__init__.py \
  tools/auth_server/device_client.py \
  tools/auth_server/proto.py \
  module_telemetry/python_ref \
  | ssh backend@192.168.1.139 \
      'mkdir -p ~/dso_rack_test && tar xzf - -C ~/dso_rack_test'
```

有 `rsync` 时等价写法：

```bash
rsync -avR --exclude '__pycache__' --exclude 'state' --exclude '*.pyc' \
  tools/__init__.py tools/rack_test \
  tools/auth_server/__init__.py \
  tools/auth_server/device_client.py \
  tools/auth_server/proto.py \
  module_telemetry/python_ref \
  backend@192.168.1.139:~/dso_rack_test/
```

服务器侧检查：

```bash
ls ~/dso_rack_test
ls ~/dso_rack_test/module_telemetry/python_ref/envelope.py
```

只有启用可选授权 flow 时才需要额外同步
`tools/auth_server/{flow,service,keystore,store}.py` 与 `.state/`。

## 3. 版本与导入自检

```bash
cd ~/dso_rack_test
python3 -V          # 需要 3.9+
PYTHONPATH=$PWD python3 -m compileall -q tools/rack_test
PYTHONPATH=$PWD python3 -c \
  'from tools.rack_test import decoders; decoders.install_decoder_path("'"$PWD"'/module_telemetry/python_ref"); print("imports ok")'
```

## 4. 快速试跑（无 sudo，确认数据面）

写一份家目录配置：

```bash
cat > ~/dso_rack_test/config.json <<EOF
{
  "bind_host": "0.0.0.0",
  "bind_port": 5005,
  "device_host": "192.168.100.50",
  "device_port": 5005,
  "decoder_dir": "$HOME/dso_rack_test/module_telemetry/python_ref",
  "state_dir": "$HOME/dso-runs",
  "baseline_s": 60.0,
  "soak_s": 600.0,
  "operator": "backend"
}
EOF
```

确认 5005 空闲（没有旧的采集器/仿真器）：

```bash
ss -ltnp | grep ':5005' || echo "port 5005 free"
```

启动接收器：

```bash
cd ~/dso_rack_test
PYTHONPATH=$PWD python3 -m tools.rack_test serve --config config.json
```

**期望**（5–10 s 内）：

```
[rack] receiver listening on 0.0.0.0:5005
[rack] up=5s frames=7.0/s a2=100(20.0/s) active=1 seq_gap=0 reboot=0 bad=0 mods=atecc608b:5,sht40:5,...
```

- `frames≈7/s`：板子 7 帧/秒在跑（含 watchdog，工具只核算 seq）。
- `a2≈20/s`：legacy 连接连断风暴，属已知固件行为；工具对空连接 RST 关闭。
- 如果 `frames=0` 且 `a2=0`：板子没连上来，检查板子服务器 IP 是否为
  `192.168.100.100`、服务器防火墙 5005、以及板子是否在跑主固件。
- 如果只有 **5 帧/s** 且没有 `atecc608b`/`watchdog`：板子跑的是 5 模块精简固件，
  需先用 ST-Link 烧主固件（精简版没有 ATECC、看门狗帧和 5005 命令通道）。
- 如果抓包看到板子向 `.100:5005` 发 **UDP**（而不是 TCP），且 TCP 5005
  被拒绝：板子跑的是 `ETH_STM32_FPGA-main` 参考工程（板 IP `.50`、服务器
  `.100:5005`、UDP），不是 DSO 模块遥测主固件。先烧
  `Project/output(mdk).hex`（约 2026-10-23 的主固件）再测。抓包命令：

  ```bash
  sudo timeout 5 tcpdump -ni eno2 'host 192.168.100.50 and udp port 5005' -c 4
  ```

  实测若 UDP 载荷是
  `{"type":"telemetry","experiment":"exp2-01_hex_counter_32","source":"fmc16",...}`
  这类 JSON，则板子跑的是**远程实验/FPGA 参考工程**，与 DSO 模块遥测无关；
  需要换成 DSO 过桥板或把主固件烧到该板，不能靠改服务器配置解决。

另开一个终端做控制通道检查：

```bash
cd ~/dso_rack_test
PYTHONPATH=$PWD python3 -m tools.rack_test cmd --config config.json get_switch
```

**期望**：`result: ok`，`data` 含 `sw/temp/temp_ok/eth_link/usb_active/buzzer_mute`。
失败时先看 §1 的 `CONTROL_REACHABLE`；板子监听器单客户端，先关掉 GUI/授权服务。

停止 `serve`（Ctrl-C），跑完整场景（`run` 内嵌接收器，独占 5005）：

```bash
cd ~/dso_rack_test
PYTHONPATH=$PWD python3 -m tools.rack_test run --config config.json \
  --baseline 60 --soak 600
```

若控制通道被板端 LwIP 资源上限（A2 风暴）间歇性拒绝，分两次跑：

```bash
# 1) 数据面（保留接收器，跳过控制）
PYTHONPATH=$PWD python3 -m tools.rack_test run --config config.json \
  --telemetry-only --baseline 60 --soak 600

# 2) 控制面（先 Ctrl-C 停掉 serve；本模式不启动接收器）
#    板子 5005 是单客户端：上一次客户端异常断开后监听器可能卡住。
#    先复位/断电重启 DSO 板，等 ~15 s。
#    复位后**不要再开任何 TCP 客户端做探测**（</dev/tcp、nc、GUI 都会占用
#    唯一的客户端位）；直接跑 control-only，让工具自己的短 probe 做第一次连接。
PYTHONPATH=$PWD python3 -m tools.rack_test run --config config.json \
  --control-only
```

若仍失败，工具会区分两种错误：

- `Connection refused`：没有监听器/accept 队列满 → 再复位一次。
- `timed out`：TCP 握手成功但应用无 ack → 监听任务没在 accept/回复。
  抓串口（1,152,000）看 `[ETH] listening :5005`、`[ETH] client connected`、
  `[ETH] acc errno=...`；没有 `client connected` 就是任务没在 accept。

`--control-only` 会在 5005 已有人监听时直接报错；它让板子的模块上报/A2
连不上服务器，从而释放 netconn/PCB，控制监听器独占连接池。开关用
ack + `get_switch` 回读验证，遥测交叉校验标 SKIP；ATECC 命令照常测。
两份报告分别给出数据面与控制面结论。

产物在 `~/dso-runs/runs/<run_id>/`：`report.md`、`summary.json`、
`telemetry_raw.jsonl`、`events.jsonl`、`human_todo.json`。
现场人工项填成 JSON 后：

```bash
PYTHONPATH=$PWD python3 -m tools.rack_test report \
  --run-dir ~/dso-runs/runs/<run_id> --human-answers answers.json
```

## 5. 防火墙

```bash
sudo ufw status verbose
sudo ufw allow from 192.168.100.0/24 to any port 5005 proto tcp
# firewalld:
sudo firewall-cmd --permanent --add-rich-rule='rule family=ipv4 source address=192.168.100.0/24 port port=5005 protocol=tcp accept'
sudo firewall-cmd --reload
```

## 6. 服务化（可选，快速试跑通过后）

```bash
sudo useradd -r -s /usr/sbin/nologin dso || true
sudo mkdir -p /opt/dso_rack_test /etc/dso_rack_test /var/lib/dso_rack_test
sudo cp -a ~/dso_rack_test/. /opt/dso_rack_test/
sudo cp ~/dso_rack_test/config.json /etc/dso_rack_test/config.json
sudo sed -i "s#\"/home/backend/dso_rack_test/module_telemetry/python_ref\"#\"/opt/dso_rack_test/module_telemetry/python_ref\"#; s#\"/home/backend/dso-runs\"#\"/var/lib/dso_rack_test\"#" /etc/dso_rack_test/config.json
sudo chown -R dso:dso /opt/dso_rack_test /var/lib/dso_rack_test
sudo chmod 644 /etc/dso_rack_test/config.json
sudo cp /opt/dso_rack_test/tools/rack_test/deploy/dso-rack-test.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now dso-rack-test
journalctl -u dso-rack-test -f
```

若不想建 `dso` 账户，把 service 里的 `User=`/`Group=` 改成登录用户（如
`backend`），并把 `/opt/dso_rack_test`、`/var/lib/dso_rack_test` 的属主改成同一用户。
`run`（带场景）以登录用户执行时，要么用 `sudo -u dso`，要么把 `--state-dir`
指到该用户可写的目录（如 `~/dso-runs`）。

## 7. 验证

```bash
ss -ltnp | grep ':5005'
cat /var/lib/dso_rack_test/runs/<run_id>/status.json
PYTHONPATH=/opt/dso_rack_test python3 -m tools.rack_test watch \
  --run-dir /var/lib/dso_rack_test/runs/<run_id>
```

> 注意：`serve` 与 `run` 都独占 5005。执行带场景的 `run` 前先
> `sudo systemctl stop dso-rack-test`；结束后再启动。
