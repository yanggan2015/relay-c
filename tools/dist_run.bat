@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "PORT=18053"
if not exist "boards.json" (
  if exist "boards.json.example" copy /Y "boards.json.example" "boards.json" >nul
)

if not exist "relay-c.exe" (
  echo [ERROR] relay-c.exe not found in this folder
  echo 请先在仓库根目录执行 build.bat
  exit /b 1
)

start /MIN "" /D "%~dp0" relay-c.exe -c boards.json

echo Started background. Web: http://127.0.0.1:%PORT%/
echo Desktop: relay_desktop.exe
exit /b 0
