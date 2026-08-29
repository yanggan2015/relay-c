@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MINGW=C:\msys64\mingw64"
set "MSYS=C:\msys64"
set "PATH=%MINGW%\bin;%MSYS%\usr\bin;%PATH%"
REM Non-login MSYS bash often sets HOME=C:Users... (broken). Point it at the
REM real Windows profile so git can read %%USERPROFILE%%\.gitconfig for tags.
if defined USERPROFILE set "HOME=%USERPROFILE%"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=build"

echo ========================================
echo  relay-c - %MODE%
echo  (delegates to build.sh -^> output/)
echo ========================================
echo.

if not exist "%MINGW%\bin\gcc.exe" (
  echo [ERROR] 未找到 %MINGW%\bin\gcc.exe
  echo 请先安装 MSYS2，并执行: install_deps.bat
  exit /b 1
)

if not exist "%MSYS%\usr\bin\bash.exe" (
  echo [ERROR] 未找到 bash，请安装 MSYS2
  exit /b 1
)

REM Use bash -c (not -lc): login shells cd to $HOME and break the working dir.
"%MSYS%\usr\bin\bash.exe" -c "./build.sh %MODE%"
if errorlevel 1 (
  echo [FAIL] build.sh failed
  exit /b 1
)

echo.
echo 产物:
echo   output\build\
echo   output\relay-c\
echo   output\relay-c-^<version^>.zip
if /I "%MODE%"=="release" echo   GitHub Release uploaded (if gh logged in)
exit /b 0
