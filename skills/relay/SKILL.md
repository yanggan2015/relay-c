# EADK Relay — Agent Skill

调用本机或局域网 Agent 接口（不要使用 MCP）。路径前缀 **`/relay`**，对外经 Gateway **TCP 18081**。

1. `GET http://{host}:18081/relay/health`
2. `GET http://{host}:18081/relay/api/help`
3. `GET http://{host}:18081/relay/help`

直连本机：`http://127.0.0.1:18153/relay/...`

PowerShell 用 `curl.exe`。写操作均为 GET（query）。

## 示例

```bash
curl.exe -s http://127.0.0.1:18081/relay/health
curl.exe -s http://127.0.0.1:18081/relay/api/help
curl.exe -s http://127.0.0.1:18081/relay/api/boards
curl.exe -s "http://127.0.0.1:18081/relay/api/boards/BOARD/action?action=reset"
curl.exe -s "http://127.0.0.1:18081/relay/api/relays/RELAY/relay?channel=1&state=true"
```
