@echo off
chcp 65001 >nul
echo ============================================
echo  DSO_V8.5 Git Repository Setup
echo ============================================
echo.

REM Check if git is available
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git is not installed or not in PATH.
    echo.
    echo Please install Git first:
    echo   Option 1: https://git-scm.com/download/win
    echo   Option 2: https://mirrors.tuna.tsinghua.edu.cn/github-release/git-for-windows/git/
    echo   Option 3: winget install --id Git.Git -e
    echo.
    pause
    exit /b 1
)

echo [1/3] Initializing git repository...
git init
if %errorlevel% neq 0 (
    echo [ERROR] git init failed
    pause
    exit /b 1
)

echo [2/3] Adding all project files...
git add .
if %errorlevel% neq 0 (
    echo [ERROR] git add failed
    pause
    exit /b 1
)

echo [3/3] Creating initial commit...
git commit -m "Initial commit: DSO_V8.5 digital oscilloscope based on STM32H7

- STM32H743IITx + XC7A100 FPGA
- FreeRTOS V10.2.0 real-time OS
- emWin GUI (800x480 LCD, DMA2D, LTDC)
- LwIP TCP/IP stack + Ethernet upload
- 8-bit/10-bit dual ADC acquisition (DMA rings)
- ETS equivalent-time sampling, FFT spectrum
- Rolling waveform display, serial debug output
- Custom scatter-loading file with 992KB total SRAM usage
- Build toolchain: ARM Compiler 5 (Keil MDK-ARM uVision 5)"
if %errorlevel% neq 0 (
    echo [ERROR] git commit failed
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Repository setup complete!
echo  Run 'git log' to verify.
echo ============================================
pause
