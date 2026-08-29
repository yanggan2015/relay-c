@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not exist "boards.json" (
  echo [INFO] 未找到 boards.json，从 example 复制...
  copy /Y "boards.json.example" "boards.json" >nul
)

if not exist "output\relay-c\relay-c.exe" (
  echo Building portable package...
  call build.bat
  if errorlevel 1 exit /b 1
)

echo Starting relay-c on http://127.0.0.1:18053/
echo Config: boards.json
start "" "http://127.0.0.1:18053/"
"output\relay-c\relay-c.exe" -c "%~dp0boards.json"
