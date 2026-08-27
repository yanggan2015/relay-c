@echo off

setlocal EnableExtensions

cd /d "%~dp0"



set "MINGW=C:\msys64\mingw64"

set "MSYS=C:\msys64"

set "PATH=%MINGW%\bin;%PATH%"

set "APP=output\build\relay-c.exe"



echo ========================================

echo  relay-c build

echo ========================================



if not exist "%MINGW%\bin\gcc.exe" (

  echo [ERROR] gcc not found: %MINGW%\bin\gcc.exe

  echo Install MSYS2: pacman -S mingw-w64-x86_64-gcc

  exit /b 1

)



if exist "%MINGW%\bin\mingw32-make.exe" (

  set "MAKE=%MINGW%\bin\mingw32-make.exe"

) else if exist "%MSYS%\usr\bin\make.exe" (

  set "MAKE=%MSYS%\usr\bin\make.exe"

) else (

  echo [ERROR] make / mingw32-make not found

  echo Install MSYS2: pacman -S mingw-w64-x86_64-make

  exit /b 1

)



taskkill /IM relay-c.exe /F >nul 2>&1

if not exist "output\build\src" mkdir "output\build\src"
if not exist "output\build\third_party" mkdir "output\build\third_party"
if not exist "output" mkdir "output"

"%MAKE%" all

if errorlevel 1 (

  echo.

  echo [HINT] If Permission denied, stop relay-c.exe and retry:

  echo        taskkill /IM relay-c.exe /F

  echo [FAIL] make failed

  exit /b 1

)



echo.

echo OK: %APP%

exit /b 0

