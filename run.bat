@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "PATH=C:\msys64\mingw64\bin;%PATH%"
set "PORT=18153"
set "PKG=%~dp0output\relay"

if not exist "boards.json" (
  echo [INFO] boards.json missing — copy from example
  if exist "boards.json.example" copy /Y "boards.json.example" "boards.json" >nul
)

if not exist "%PKG%\relay.exe" (
  echo Building relay...
  call build.bat
  if errorlevel 1 exit /b 1
)

if exist "boards.json" copy /Y "boards.json" "%PKG%\boards.json" >nul

call "%PKG%\run.bat"
exit /b %ERRORLEVEL%
