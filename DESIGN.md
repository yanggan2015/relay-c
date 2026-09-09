# relay-c 设计说明

## 0. 开源 / 发布策略（重要）

### 0.1 原则

| 内容 | 策略 | 说明 |
|------|------|------|
| 源代码 | **闭源 / 私有仓库** | 仅维护者可见；不进 Release 附件 |
| Windows 可执行包 | **公开发布** | zip 内含 exe + 运行时 DLL + 示例配置 |
| Linux 可执行包 | **预留，稍后开放** | 命名与流程与 Windows 对称 |
| Release 中的 “Source code” | **不得含业务源码** | 公开发布仓仅放 README，无工程源码 |

> GitHub 限制：若 Release 建在**私有**仓库上，外人无法下载。  
> 因此必须采用 **「私有源码仓 + 公开发布仓」** 双仓模型（与 ssh-bridge-c 相同）。

### 0.2 双仓模型

```text
yanggan2015/relay-c              ← 私有：完整源码、CI/构建脚本
        │
        │  ./build.sh release
        │  （只上传可执行 zip，不上传源码）
        ▼
yanggan2015/relay-c-releases     ← 公开：仅 README + GitHub Releases 附件
```

| 仓库 | 可见性 | 用途 |
|------|--------|------|
| `relay-c` | **Private** | 源码开发、`build.sh`、内部文档 |
| `relay-c-releases` | **Public** | 对外下载 Windows（及未来 Linux）二进制 |

用户下载入口：

- https://github.com/yanggan2015/relay-c-releases/releases
- 过期联系：`yanggan2015@foxmail.com`

### 0.3 版本号与有效期

- 格式：`YYYYMMDD-HHMMSS`（UTC），由 `tools/gen_version.py` 生成
- 每个构建自编译起 **90 天**有效；`relay-c.exe version` 可查看
- Release 资产名：`relay-c-windows-<VERSION>.zip`

### 0.4 构建命令

```bash
./build.sh              # 本地构建 + windows zip（不上传）
./build.sh release      # 构建 + 上传到公开 releases 仓
```

Windows：`build.bat` / `build.bat release`

---

## 1. 设计目标

在 Python 版 `relay` 项目基础上，用 C 语言实现同等能力的继电器控制服务，并做以下增强：

- **启用开关**：`relays`、`boards` 及其子动作（`reset`、`upgrade_mode`）均支持 `enabled`
- **波特率配置**：每个 relay 可独立设置 `baudrate`（默认 9600）
- **多协议支持**：内置 A0 标准协议 + 自定义 hex/str 原始指令，适配不同厂商继电器
- **镜像升级模式**：用 `upgrade_mode` 替代 `supports_maskrom`；仅当配置块存在且 `enabled=true` 时才支持升级动作
- **IO 取反**：relay 级 `io_inverted`，解决继电器接线反相问题
- **固定 reset/upgrade 时序**：语义写死在代码中，见 2.2
- **自定义 actions**：board 下可配置带 label 的自定义开关逻辑
- **跨平台串口**：Windows（CreateFile COMx）/ Linux（termios）
- **HTTP API**：全部 GET，与 Python 版接口对齐并扩展

## 2. 分层架构

```
┌─────────────────────────────────────────────────────────┐
│  Layer 3: HTTP + 配置层  (http_server.c, config.c)      │
│  GET API / Web UI / boards.json 持久化                   │
├─────────────────────────────────────────────────────────┤
│  Layer 2: 服务编排层  (service.c)                        │
│  reset / upgrade 序列、relay 直接控制、状态聚合           │
├─────────────────────────────────────────────────────────┤
│  Layer 1: 协议 + 执行层  (protocol.c, serial.c)         │
│  A0 帧编码 / hex·str 解析 / 串口读写                      │
└─────────────────────────────────────────────────────────┘
```

### 2.1 协议层

- **A0 标准协议**（默认）：
  - 设置：`A0 <ch> <00|01> <checksum>`
  - 查询：`A0 <ch> 05 <checksum>`
- **自定义 hex**：配置如 `"hex": "A0 01 01 A2"` 或 `"A0 01 01 A2"`
- **自定义 str**：配置如 `"str": "relay1 on\r\n"`，支持 `\r` `\n` `\xHH` 转义

通道级命令优先级：`channels.<N>.{on,off,query}` > relay 级 `protocol: a0` 默认行为。

### 2.2 服务层

**逻辑电平与 IO 取反**

- 板级动作使用**逻辑电平**（ON/OFF）
- relay 级 `io_inverted=true` 时，逻辑 ON/OFF 与物理串口命令对调
- `polarity_inverted` 决定空闲态/脉冲态映射

| polarity_inverted | 空闲态 | 脉冲态 |
|-------------------|--------|--------|
| false（默认） | ON | OFF |
| true | OFF | ON |

**reset 固定时序**（默认 polarity_inverted=false）：

空闲 ON → 脉冲 OFF → 保持 hold_seconds → 恢复 ON

**upgrade_mode 固定时序**（与 Python maskrom 一致，使用 pulse/idle）：

1. reset 通道进入脉冲态
2. upgrade 通道进入脉冲态
3. 等待 2s
4. reset 恢复空闲态
5. 等待 2s
6. upgrade 恢复空闲态

**自定义 actions**（`boards.<name>.actions.<action_name>`）：

| mode | 行为 |
|------|------|
| `pulse` | 脉冲态 → hold_seconds → 空闲态 |
| `hold_on` | 置逻辑 ON 并保持 |
| `hold_off` | 置逻辑 OFF 并保持 |

约束：同一 relay 上 reset 与 upgrade_mode 不能使用相同通道。

### 2.3 HTTP 层

基于 mongoose 实现轻量 HTTP 服务，提供控制页、文档页、配置页。

## 3. 数据模型

### 3.1 RelayConfig

| 字段 | 类型 | 说明 |
|------|------|------|
| `enabled` | bool | 是否启用，默认 true |
| `windows_port` | string | Windows 串口，如 COM18 |
| `linux_port` | string | Linux 设备，如 /dev/ttyUSB0 |
| `port` | string | 通用回退端口 |
| `relay_channels` | int | 通道数，≥1 |
| `baudrate` | int | 波特率，默认 9600 |
| `io_inverted` | bool | IO 接线取反，默认 false |
| `post_write_delay_ms` | float | 写后等待 ms，默认 100 |
| `channels` | object | 可选，按通道自定义 on/off/query 命令 |

**channels 子项示例**：

```json
"channels": {
  "1": {
    "on":  { "hex": "A0 01 01 A2" },
    "off": { "hex": "A0 01 00 A1" },
    "query": { "hex": "A0 01 05 A6" }
  },
  "2": {
    "on":  { "str": "AT+RELAY=2,ON\\r\\n" },
    "off": { "str": "AT+RELAY=2,OFF\\r\\n" }
  }
}
```

未配置 `channels.<N>` 时，该通道使用 A0 标准协议。

### 3.2 BoardConfig

| 字段 | 类型 | 说明 |
|------|------|------|
| `enabled` | bool | 板级总开关 |
| `reset` | object | 复位绑定，含 `enabled` |
| `upgrade_mode` | object | 可选；存在且 `enabled=true` 才支持 upgrade 动作 |

**reset / upgrade_mode 子项**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `enabled` | bool | 该动作是否启用 |
| `relay` | string | 绑定的 relay 名称 |
| `channel` | int | 通道号 |
| `polarity_inverted` | bool | 极性反转 |
| `hold_seconds` | float | 仅 reset：脉冲保持时间 |

> **命名说明**：`upgrade_mode` 表示「镜像升级/烧录模式」（原 maskrom）。不再需要单独的 `supports_maskrom` / `supports_burn` 布尔字段——配置块存在且 `enabled=true` 即表示支持。

## 4. 配置文件规范

**扁平套件**：首选文件名 `relay_config.json`（含 `"eadk":{"tool":"relay"}`）；查找顺序与遗留 `boards.json`/`config.json` 见套件根 `TOOL_STANDARD.md` §2.3。

```json
{
  "eadk": { "tool": "relay" },
  "server": { "port": 18053 },
  "relays": {
    "com18_test": {
      "enabled": true,
      "windows_port": "COM18",
      "relay_channels": 2,
      "baudrate": 9600,
      "channels": {
        "1": {
          "on":  { "hex": "A0 01 01 A2" },
          "off": { "hex": "A0 01 00 A1" }
        }
      }
    }
  },
  "boards": {
    "demo_board": {
      "enabled": true,
      "reset": {
        "enabled": true,
        "relay": "com18_test",
        "channel": 1,
        "hold_seconds": 5
      },
      "upgrade_mode": {
        "enabled": true,
        "relay": "com18_test",
        "channel": 2
      }
    }
  }
}
```

## 5. API 设计

### 5.1 读取

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/relays` | 列出所有 relay 配置 |
| GET | `/api/boards` | 列出所有 board 配置 |
| GET | `/api/boards/support` | 板级能力与解析端口 |
| GET | `/api/relays/<relay>/state` | 全部通道状态 |
| GET | `/api/relays/<relay>/state?channel=1` | 单通道状态 |
| GET | `/api/boards/<board>/state` | board 关联通道状态 |

### 5.2 控制

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/relays/<relay>/relay?channel=1&state=true\|false` | 通道开关 |
| GET | `/api/relays/<relay>/raw?data=...&format=hex\|str` | 发送原始数据 |
| GET | `/api/boards/<board>/action?action=reset\|upgrade` | 板级动作 |

### 5.3 配置 CRUD

与 Python 版相同模式：`/api/config/relay/{create,update,delete}`、`/api/config/board/{create,update,delete}`、`/api/config/server/set`。

新增 query 参数：`enabled`、`baudrate`、通道自定义命令等。

## 6. 校验规则

- relay 未 `enabled` → 拒绝控制，列表中仍可见但标记 disabled
- board 未 `enabled` → 拒绝所有板级动作
- reset/upgrade_mode 子项未 `enabled` → 拒绝对应动作
- 通道号必须在 `[1, relay_channels]` 内
- board 绑定的 relay 必须存在且 enabled
- upgrade_mode 与 reset 同 relay 同通道 → 拒绝
- 删除 relay 前检查 board 引用

## 7. 与 Python 版差异

| 项目 | Python relay | relay-c |
|------|-------------|---------|
| maskrom | `supports_maskrom` + action=maskrom | `upgrade_mode.enabled` + action=upgrade |
| 波特率 | 固定 9600 | 每 relay 可配 |
| 协议 | 仅 A0 | A0 + hex/str 自定义 |
| enable | 无 | relay/board/动作均有 |
| 原始发送 | 无 | `/raw` 接口 |
| 状态缓存 | 无 | 内存缓存最近状态 |

## 8. 目录结构

```
relay-c/
├── DESIGN.md
├── README.md
├── Makefile
├── build.bat / build.sh
├── install_deps.bat
├── boards.json.example
├── include/
├── src/
├── third_party/
├── tests/
├── tools/                  # gen_version / dist 打包文档
└── output/                 # build/ + relay-c/ 便携目录 + zip
```

## 9. 测试策略

- 单元测试 `test_relay.c`：InMemory 串口模拟，覆盖 reset/upgrade 序列、enable 校验、配置 CRUD
- 硬件测试：COM18 连接实际继电器模块
- 发版前：`./build.sh release`，确认公开仓可下载且附件无源码
