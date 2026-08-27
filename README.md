# relay-c

C 语言版继电器控制服务。继电器定义与板级行为分离，支持 IO 取反、固定 reset/upgrade 时序、自定义标签动作。

## 快速开始

```bat
build.bat
copy boards.json.example boards.json   REM 首次使用
run.bat
```

浏览器打开：**http://127.0.0.1:18053/**（控制页 `/` · 配置编辑 `/config` · API 文档 `/docs`：Markdown 预览 + 完整 curl，自动使用局域网 IP）

无硬件时用模拟模式：

```bat
output\build\relay-c.exe -c boards.json -n
```

## 配置文件 boards.json

### 顶层结构

| 字段 | 说明 |
|------|------|
| `server.port` | HTTP 端口（默认 18053） |
| `relays` | 继电器模块定义 |
| `boards` | 板级行为定义 |

### 继电器 relays

```json
"com18_test": {
  "enabled": true,
  "windows_port": "COM18",
  "linux_port": "/dev/ttyUSB0",
  "relay_channels": 2,
  "baudrate": 9600,
  "io_inverted": false,
  "channels": {
    "1": {
      "on":  { "hex": "A0 01 01 A2" },
      "off": { "hex": "A0 01 00 A1" }
    }
  }
}
```

| 字段 | 说明 |
|------|------|
| `enabled` | 是否启用 |
| `windows_port` / `linux_port` | 平台串口 |
| `relay_channels` | 通道数 |
| `baudrate` | 波特率（默认 9600） |
| `io_inverted` | **IO 接线取反**：逻辑 ON/OFF 与物理命令对调，解决继电器接线反了的问题 |
| `channels` | 可选，按通道自定义 hex/str 开关命令（不配置则用 A0 标准协议） |

### 板级 boards

每个 board 包含：

- `reset` — 复位（固定时序，见下文）
- `upgrade_mode` — 镜像升级模式（可选，存在且 `enabled=true` 才可用）
- `actions` — 自定义标签动作（可选）

```json
"demo_board": {
  "enabled": true,
  "reset": {
    "enabled": true,
    "relay": "com18_test",
    "channel": 1,
    "hold_seconds": 5,
    "polarity_inverted": false
  },
  "upgrade_mode": {
    "enabled": true,
    "relay": "com18_test",
    "channel": 2
  },
  "actions": {
    "fan_on": {
      "enabled": true,
      "label": "风扇开启",
      "relay": "com18_test",
      "channel": 2,
      "mode": "hold_on"
    }
  }
}
```

## 固定时序语义

### reset（复位脉冲）

由 `polarity_inverted` 决定空闲态与脉冲态（逻辑电平，再经 relay 的 `io_inverted` 转为物理命令）：

| polarity_inverted | 空闲态 | 脉冲态 | 时序 |
|-------------------|--------|--------|------|
| `false`（默认） | ON | OFF | **OFF → 保持 hold_seconds → ON** |
| `true` | OFF | ON | **ON → 保持 hold_seconds → OFF** |

即默认情况下：平时 IO 为 ON，复位时拉 OFF，保持若干秒后恢复 ON。

### upgrade_mode（镜像升级）

与 Python 版 maskrom 一致，使用 pulse/idle 语义：

1. reset 通道进入**脉冲态**（assert reset）
2. upgrade 通道进入**脉冲态**（enable upgrade）
3. 等待 2 秒
4. reset 恢复**空闲态**（de-assert）
5. 等待 2 秒
6. upgrade 恢复**空闲态**

约束：同一 relay 上 reset 与 upgrade 不能使用相同通道。

### 自定义 actions

`actions` 下每个键名为 API 动作名，`label` 为界面显示文字。

| mode | 行为 |
|------|------|
| `pulse` | 同 reset：脉冲态 → 保持 hold_seconds → 空闲态 |
| `hold_on` | 置逻辑 ON 并保持 |
| `hold_off` | 置逻辑 OFF 并保持 |

每个 action 可独立设置 `polarity_inverted`、`hold_seconds`（pulse 模式）。

## HTTP API（均为 GET）

### 读取

```
GET /api/relays
GET /api/boards
GET /api/boards/support
GET /api/relays/<relay>/state
GET /api/relays/<relay>/state?channel=1
GET /api/boards/<board>/state
```

### 控制

```
GET /api/relays/<relay>/relay?channel=1&state=true
GET /api/relays/<relay>/raw?format=hex&data=A0%2001%2001%20A2
GET /api/boards/<board>/action?action=reset
GET /api/boards/<board>/action?action=upgrade
GET /api/boards/<board>/action?action=<自定义动作名>
```

示例：

```bat
curl "http://127.0.0.1:18053/api/boards/demo_board/action?action=reset"
curl "http://127.0.0.1:18053/api/boards/demo_board/action?action=fan_on"
curl "http://127.0.0.1:18053/api/relays/com18_test/relay?channel=1&state=true"
```

## 架构分层

```
Layer 3  HTTP          http_server.c (页面) + http_api.c (health/config API)
Layer 2  服务编排      service.c (reset/upgrade/custom，动作互斥)
Layer 1  协议+执行     protocol.c + serial.c (A0/hex/str + io_inverted)
Layer 0  基础设施      relay_lock.c (串口/动作锁) + config.c + action.c
```

| 模块 | 职责 |
|------|------|
| `action.c` | 空闲态/脉冲态语义、io_inverted 逻辑↔物理映射 |
| `relay_lock.c` | 串口全局互斥 + 板级动作互斥（防并发 reset） |
| `relay_startup.c` | 启动时按 `startup_init` 初始化通道 |
| `http_api.c` | health、config CRUD、热重载 |
| `http_pages.c` | 共享样式 / 导航 |
| `http_control_page.c` | 控制页 `/` |
| `http_docs_page.c` | API 文档 `/docs`（Markdown 预览 / 源码） |
| `http_config_page.c` | Web 配置编辑页 `/config` |
| `http_server.c` | HTTP 路由与 relay/board 控制 API |

## 命令行参数

```
relay-c.exe [-c boards.json] [-p 18053] [-n] [-h]
```

| 参数 | 说明 |
|------|------|
| `-c` | 配置文件路径 |
| `-p` | HTTP 端口（配置文件未指定时生效） |
| `-n` | dry-run 模拟模式，不打开真实串口 |
| `-h` | 帮助 |

## v1.1 新增能力

### 启动初始化 `startup_init`

```json
"startup_init": "off"   // 启动时全部通道置 OFF
"startup_init": "on"     // 启动时全部通道置 ON
// 省略或 "none"：不操作
```

### 健康检查与配置管理

```
GET /api/health              # 服务状态、版本、配置路径
GET /api/health               # 状态 / 版本 / 配置路径
GET /api/serial/ports         # 探测本机串口（Windows: COM* / Linux: /dev/tty*）
GET /api/config/reload       # 热重载 boards.json（无需重启）
GET /api/config/relays       # 配置视图
GET /api/config/relay/create?...&relay/update?...&relay/delete?...
GET /api/config/board/create?...&board/update?...&board/delete?...
GET /api/config/server/set?port=18053
```

### 并发安全

- **串口锁**：同一时刻仅一个串口事务
- **动作锁**：reset/upgrade/custom 不可并发；忙时返回 `another board action is in progress`

## 测试

```bash
make test
```

## 设计文档

详见 [DESIGN.md](DESIGN.md)
