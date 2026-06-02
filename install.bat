@echo off
setlocal

:: Force the script's working directory to the folder where the .bat file lives
cd /d "%~dp0"

set "SRC=build\Release\CustomPlugin.ofx"
set "DEST_DIR=C:\Program Files\Common Files\OFX\Plugins\CustomPlugin.ofx.bundle\Contents\Win64"
set "DEST_FILE=%DEST_DIR%\CustomPlugin.ofx"

:: 1. Admin Check
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Access Denied. Right-click and "Run as administrator".
    pause
    exit /b 1
)

:: 2. Source Check
if not exist "%SRC%" (
    echo [ERROR] Source file missing: %CD%\%SRC%
    echo Did the compiler fail, or are you using a Ninja generator?
    pause
    exit /b 1
)

:: 3. Bundle Construction
if not exist "%DEST_DIR%" (
    echo [INFO] Creating bundle directory structure...
    mkdir "%DEST_DIR%"
)

:: 4. Deployment
echo [INFO] Copying binary...
copy /Y "%SRC%" "%DEST_FILE%"

if %errorLevel% equ 0 (
    echo [SUCCESS] Plugin installed successfully.
) else (
    echo [ERROR] Copy failed. Ensure DaVinci Resolve is fully closed.
)

pause