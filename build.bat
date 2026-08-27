@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MINGW=C:\msys64\mingw64"
set "MSYS=C:\msys64"
set "PATH=%MINGW%\bin;%MSYS%\usr\bin;%PATH%"
set "APP=output\build\relay-c.exe"

echo ========================================
echo  relay-c build
echo ========================================

if not exist "%MINGW%\bin\gcc.exe" (
  echo [ERROR] 未找到 %MINGW%\bin\gcc.exe
  echo 请先安装 MSYS2，并执行: pacman -S mingw-w64-x86_64-gcc
  exit /b 1
)

if exist "%MINGW%\bin\mingw32-make.exe" (
  set "MAKE=%MINGW%\bin\mingw32-make.exe"
) else if exist "%MSYS%\usr\bin\make.exe" (
  set "MAKE=%MSYS%\usr\bin\make.exe"
) else (
  echo [ERROR] 未找到 make / mingw32-make
  echo 请执行: pacman -S mingw-w64-x86_64-make
  exit /b 1
)

tasklist /FI "IMAGENAME eq relay-c.exe" 2>nul | find /I "relay-c.exe" >nul
if not errorlevel 1 (
  echo [INFO] 检测到 relay-c.exe 正在运行，正在停止...
  taskkill /IM relay-c.exe /F >nul 2>&1
  timeout /t 1 /nobreak >nul
)

"%MAKE%" all
if errorlevel 1 (
  if exist "%APP%" (
    echo.
    echo [HINT] 若报 Permission denied，请先关闭正在运行的 relay-c.exe：
    echo        taskkill /IM relay-c.exe /F
    echo        或在任务管理器中结束 relay-c.exe 后重试
  )
  echo [FAIL] make failed
  exit /b 1
)

echo.
echo OK: %APP%
exit /b 0
