@echo off
title Antigravity Usage Studio Launcher
cd /d "%~dp0"
if exist "%LocalAppData%\Programs\Python\Python311\pythonw.exe" (
    start "" "%LocalAppData%\Programs\Python\Python311\pythonw.exe" "hud_app.py"
) else (
    start pythonw hud_app.py
)
