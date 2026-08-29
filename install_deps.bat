@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MSYS=C:\msys64"
if not exist "%MSYS%\usr\bin\bash.exe" (
  echo [ERROR] 未找到 MSYS2: %MSYS%
  echo 请先安装: winget install -e --id MSYS2.MSYS2
  echo 默认安装路径应为 C:\msys64
  exit /b 1
)

echo ========================================
echo  安装 mingw64 编译依赖 (relay-c)
echo ========================================
"%MSYS%\usr\bin\bash.exe" -lc "pacman -Sy --noconfirm && pacman -S --needed --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-pkgconf mingw-w64-x86_64-cjson"
if errorlevel 1 (
  echo [FAIL] pacman 安装失败，可打开 MSYS2 MinGW 64-bit 终端手动执行 pacman 命令
  exit /b 1
)

echo.
echo [OK] 依赖已安装。接下来运行: build.bat
exit /b 0
