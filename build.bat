@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MINGW=C:\msys64\mingw64"
set "MSYS=C:\msys64"
set "PATH=%MINGW%\bin;%MSYS%\usr\bin;%PATH%"
if defined USERPROFILE set "HOME=%USERPROFILE%"

echo ========================================
echo  relay-c - build (Windows)
echo ========================================

if not exist "%MINGW%\bin\gcc.exe" (
  echo [ERROR] 未找到 %MINGW%\bin\gcc.exe
  echo 请安装 MSYS2 mingw64: pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-cjson
  exit /b 1
)

taskkill /IM relay-c.exe /F >nul 2>&1
taskkill /IM relay_desktop.exe /F >nul 2>&1
"%MINGW%\bin\mingw32-make.exe" clean all test package
if errorlevel 1 (
  echo [FAIL] build failed
  exit /b 1
)

echo.
echo OK:
echo   output\build\relay-c.exe
echo   output\relay-c\
echo 运行: output\relay-c\run.bat  或  仓库根目录 run.bat
exit /b 0
