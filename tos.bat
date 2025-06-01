@echo off
setlocal

REM 获取脚本所在目录
set SCRIPT_DIR=%~dp0

REM 检查Python是否安装
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Python is not installed or not in PATH
    exit /b 1
)

REM 运行Python版本的tos工具
python "%SCRIPT_DIR%tos.py" %* 