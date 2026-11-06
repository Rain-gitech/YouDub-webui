@echo off
cd /d "%~dp0"
"%~dp0.venv-personal\Scripts\python.exe" "%~dp0scripts\start-personal.py"
if errorlevel 1 pause
