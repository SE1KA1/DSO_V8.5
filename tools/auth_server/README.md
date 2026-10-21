# DEV 授权服务（PC 侧）

> **仅用于开发联调**。这不是安全产品：CA 私钥在本机文件里，传输是明文 TCP，
> 设备端目前还无法验签（ATECC 未锁定，见 `Markdown/ATECC608B_结论_配置区未锁定.md`）。

协议格式与安全边界见 **`Markdown/设备授权协议_v0.md`**。

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

# 3) 登记设备公钥（只能带外/管理员做，服务不提供在线注册）
python -m tools.auth_server.service register Eep21025E269FA2E <128位hex公钥>

# 4) 绑定：允许 stu001 使用该设备 30 天
python -m tools.auth_server.service bind stu001 Eep21025E269FA2E --days 30

# 5) 只读探测真机能力（不改任何东西）
python -m tools.auth_server.service probe --host 192.168.1.50
#   provisioned    = 能签名（已预置且锁定的器件）
#   identity_only  = 只能读身份，设备证明不可用（当前这颗未锁定的片子）

# 6) 校验一个 License token
python -m tools.auth_server.service verify-license <token> --device Eep21025E269FA2E
```

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

## 模拟设备

`dsosim/atecc.py` 提供"已预置"设备模型（软件 P-256 密钥），挂在 `dsosim` 的
`atecc.*` 命令上：

```python
from dsosim.device import DSOSimulator
dev = DSOSimulator()
dev.atecc.provisioned = False     # 模仿真机未锁定态：rc=-4 st=0x0F
```

`cryptography` 只在 `dsosim/atecc.py` 内部惰性导入，dsosim 其余部分仍是 stdlib。
