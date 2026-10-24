@echo off
title Antigravity Usage Studio Launcher (Native C++20 Direct2D)
cd /d "%~dp0"
if exist "antigravity-hud.exe" (
    start "" "%~dp0antigravity-hud.exe"
) else (
    start pythonw hud_app.py
)
