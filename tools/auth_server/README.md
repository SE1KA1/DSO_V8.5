# DEV 授权服务（PC 侧）

> **仅用于开发联调**。这不是安全产品：CA 私钥在本机文件里，传输是明文 TCP，
> 设备端目前还无法验签（ATECC 未锁定，见 `Markdown/ATECC608B_结论_配置区未锁定.md`）。

协议格式与安全边界见 **`Markdown/设备授权协议_v0.md`**。

**部署 / 备份 / CA 轮换 / 故障排查 / 已知限制**：见 **`Markdown/授权服务部署与运维.md`**。

## 运行环境（先看这条）

- **必须在仓库根目录执行**（`cd D:\DSO_V8.5`）：`python -m tools.…` 要求根目录在 `sys.path` 上。
  在 `tools/` 里执行会报 `No module named 'tools'`。
- **用系统 Python，不要用仓库的 `.venv`**：`.venv` 里没有 `cryptography`（只有 `cryptoauthlib`）。
  系统 Python（`tools/atecc_verify_signature.py` 能跑的那个）已装 `cryptography`。
- 若想从任意目录跑测试，也可以直接执行脚本：`python D:\DSO_V8.5\tools\auth_server\e2e_test.py`。

## 快速开始

```bash
# 1) 初始化：生成本机 CA 密钥与台账（都在 .state/，已 gitignore）
python -m tools.auth_server.service init

# 2) 建一个学生账号
python -m tools.auth_server.service add-user stu001

# 3) 登记设备公钥 + 命令地址（只能带外/管理员做，服务不提供在线注册）
#    --host 是设备 TCP 5005 的地址；HTTP API 的 /api/authorize 靠它去驱动设备
python -m tools.auth_server.service register Eep21025E269FA2E <128位hex公钥> --host 192.168.100.50

# 4) 绑定：允许 stu001 使用该设备 30 天
python -m tools.auth_server.service bind stu001 Eep21025E269FA2E --days 30

# 5) 只读探测真机能力（不改任何东西）
python -m tools.auth_server.service probe --host 192.168.1.50
#   provisioned    = 能签名（已预置且锁定的器件）
#   identity_only  = 只能读身份，设备证明不可用（当前这颗未锁定的片子）

# 6) 校验一个 License token
python -m tools.auth_server.service verify-license <token> --device Eep21025E269FA2E

# 7) 把 CA 公钥生成为固件头文件（Stage 2C 设备侧验签要用）
python -m tools.auth_server.service show-ca --c > User/dso/dso_auth_ca.h

# 8) 一条命令跑完整闭环（真机或 dsosim）
python -m tools.auth_server.service flow --host 192.168.100.50 --user stu001

# 9) 反向验证：证明设备真的会「拒绝」坏 License（7 个用例）
python -m tools.auth_server.service negtest --host 192.168.100.50 --user stu001

# 10) 只读查询设备当前持有的 License（登录状态 + lic/user/nb/na/perm）
#     restored=yes 表示这条记录是重启后从 EEPROM 恢复的（设备无时钟，不判过期）
python -m tools.auth_server.service license --host 192.168.100.50

# 11) 续期：设备证明 -> 新 License -> 推给设备
python -m tools.auth_server.service renew --host 192.168.100.50 --user stu001

# 12) 吊销：单张 License 或整台设备（之后不再签发）
python -m tools.auth_server.service revoke --jti <jti>
python -m tools.auth_server.service revoke --device Eep21025E269FA2E
```

> `dso_auth_ca.h` 是**生成文件**：本机 CA 换掉后必须重新生成，否则设备侧会拒绝所有 License。

## 测试

```bash
python -m unittest tools.auth_server.e2e_test -v
```

50 条用例：正向闭环、重放与新鲜度、token 形状的逐项拒绝、签名规则
（License 要求 low-s / 设备证明允许 high-s）、策略拒绝、命令行长度上限、
以及一次完整的 TCP 往返（对接 `dsosim`）。登录失败 5 次后账号锁定 5 分钟。

> 测试用系统 Python（需要 `cryptography`）。`.venv` 里没有这个包。

## 文件

| 文件 | 说明 |
|---|---|
| `proto.py` | token 格式、规范化 JSON、严格解析（stdlib） |
| `keystore.py` | CA 密钥、low-s 归一化、raw↔DER（需要 `cryptography`） |
| `store.py` | JSON 台账：用户（PBKDF2）、设备、绑定、挑战、License |
| `service.py` | 服务库 + CLI |
| `device_client.py` | v0.9 JSON 客户端（处理遥测干扰、未知命令回显、512 字节上限） |
| `e2e_test.py` | 测试 |
| `.state/` | 运行时生成：`ca_key.pem`、`store.json` —— **不要提交** |

## HTTP API（常驻服务）

```bash
python -m tools.auth_server.http_api --port 8080
# 首次启动生成 .state/admin_token（0600）；审计写 .state/audit.jsonl
```

启动后直接在浏览器打开：

- **`http://127.0.0.1:8080/`** —— 学生端：登录 → 选设备 → 授权/续期 → 查看设备状态
- **`http://127.0.0.1:8080/admin`** —— 管理端：令牌 → 设备台账 / 建用户 / 登记设备 / 绑定 / 吊销 / 审计

界面是纯静态的（`web/` 目录，无框架无 CDN），由同一个服务托管。

端点与安全模型见 `Markdown/设备授权协议_v0.md` §12。快速示例：

```bash
TOKEN=$(cat tools/auth_server/.state/admin_token)

# 管理员：建用户 / 登记设备（host/port 只在这里给）/ 绑定
curl -s -X POST http://127.0.0.1:8080/api/admin/users \
  -H "X-Admin-Token: $TOKEN" -H 'Content-Type: application/json' \
  -d '{"user":"stu001","password":"12345678"}'

curl -s -X POST http://127.0.0.1:8080/api/admin/devices \
  -H "X-Admin-Token: $TOKEN" -H 'Content-Type: application/json' \
  -d '{"device":"Eep21025E269FA2E","pubkey":"<128-hex>","host":"192.168.100.50","port":5005}'

curl -s -X POST http://127.0.0.1:8080/api/admin/bindings \
  -H "X-Admin-Token: $TOKEN" -H 'Content-Type: application/json' \
  -d '{"user":"stu001","device":"Eep21025E269FA2E","days":30}'

# 学生：登录 -> 授权（服务端自己去驱动设备）
SID=$(curl -s -X POST http://127.0.0.1:8080/api/session \
  -H 'Content-Type: application/json' \
  -d '{"user":"stu001","password":"12345678"}' \
  | python -c "import sys,json;print(json.load(sys.stdin)['sid'])")

curl -s -X POST http://127.0.0.1:8080/api/authorize \
  -H 'Content-Type: application/json' \
  -d "{\"sid\":\"$SID\",\"device\":\"Eep21025E269FA2E\"}"

# 设备当前持有什么
curl -s http://127.0.0.1:8080/api/device/Eep21025E269FA2E/license \
  -H "X-Session: $SID"
```

> 默认只监听 `127.0.0.1`。要对外开放必须加 `--unsafe-lan`——**明文 HTTP，
> 会话与凭据会在局域网上裸奔**，仅限实验环境。

审计与运维：

- 审计按事件/设备过滤：`/api/admin/audit?limit=100&event=authorize_ok&device=Eep…`
- 导出 CSV：`/api/admin/audit/export`（需 admin token，界面上的"导出 CSV"走同一个端点）
- 日志超过 2 MB 自动轮转为 `audit.jsonl.1`（只保留一代）

管理端小抄：

- 吊销支持 **License 号**（`{"lic":"L00005"}`）或 **jti**（`{"jti":"..."}`）或整机（`{"device":"..."}`）；
  界面上 "License 台账" 每行都有吊销按钮。
- 同一账号 + 同一设备只允许一条**有效**绑定：重复提交会回 `409 binding_exists`；
  改期限用 `{"replace":true}`（CLI 用 `bind … --force`）。
- `provisioned` = 能完成设备证明（可授权）；`identity_only` = 只能报设备号（拿不到 License）。
  详见协议文档 §5.1。

测试：

```bash
python -m unittest tools.auth_server.http_test -v     # 29 条
```

## 模拟设备

`dsosim/atecc.py` 提供"已预置"设备模型（软件 P-256 密钥），挂在 `dsosim` 的
`atecc.*` 命令上：

```python
from dsosim.device import DSOSimulator
dev = DSOSimulator()
dev.atecc.provisioned = False     # 模仿真机未锁定态：rc=-4 st=0x0F
```

`cryptography` 只在 `dsosim/atecc.py` 内部惰性导入，dsosim 其余部分仍是 stdlib。
