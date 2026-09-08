# windows_desktop — Windows 桌面壳（`relay_desktop.exe`）

与 `relay-c` 服务端解耦：本目录仅含 WebView2 外壳，拉起同目录 `relay-c.exe` 并嵌入 WebUI。

无 icon/rc；webview 依赖共享自 `../../ssh-bridge-c/third_party`。

## 构建

```bat
cd windows_desktop
mingw32-make
```

或仓库根：`mingw32-make desktop`

产物：`output/build/relay_desktop.exe`（并复制到 `output/relay_desktop.exe`）。

## 依赖

- `ssh-bridge-c/third_party/webview` + `webview2`
- 本机 WebView2 Runtime
- 同目录 `relay-c.exe` + `boards.json`
