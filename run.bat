@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "PATH=C:\msys64\mingw64\bin;%PATH%"
if not exist "boards.json" (
  echo [INFO] 未找到 boards.json，从 example 复制...
  copy /Y "boards.json.example" "boards.json" >nul
)
if not exist "output\build\relay-c.exe" (
  echo Building...
  call build.bat
  if errorlevel 1 exit /b 1
)

copy /Y "C:\msys64\mingw64\bin\libcjson-1.dll" "output\build\" >nul 2>&1

echo Starting relay-c on http://127.0.0.1:18053/
echo Config: boards.json  COM18 test relay
start "" "http://127.0.0.1:18053/"
"output\build\relay-c.exe" -c boards.json
