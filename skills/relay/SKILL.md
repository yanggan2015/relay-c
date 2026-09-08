# EADK Relay — Agent Skill

调用本机或局域网 HTTP API（不要使用 MCP）。

1. `GET http://{host}:{port}/health` 或 `/api/health`
2. `GET http://{host}:{port}/api/help` — 仅使用帮助中列出的路径
3. 板级动作 / 继电器通道操作按 help 中的 example 调用

默认 HTTP 端口：**18053**。
