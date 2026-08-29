@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not exist "relay-c.exe" (
  echo 未找到 relay-c.exe
  pause
  exit /b 1
)

if not exist "boards.json" (
  if exist "boards.json.example" (
    echo 未找到 boards.json，从示例复制...
    copy /Y "boards.json.example" "boards.json" >nul
    echo 已生成 boards.json，请按实际串口修改后再运行。
    echo.
    notepad "boards.json"
  )
)

echo 启动 relay-c ...
echo 浏览器: http://127.0.0.1:18053/
echo 帮助: relay-c.exe help
echo 版本: relay-c.exe version
echo.
relay-c.exe %*
echo.
echo 已退出，代码=%ERRORLEVEL%
pause
exit /b %ERRORLEVEL%
