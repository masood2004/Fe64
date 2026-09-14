@echo off
REM ============================================================
REM  Launch the Fe64 Lichess bot (Windows)
REM    - uses the project venv (built from Blender's Python 3.13,
REM      because Smart App Control blocks the standalone build)
REM    - unbuffered output so logs stream in real time
REM ============================================================
set PYTHONUNBUFFERED=1
set PYTHONNOUSERSITE=1
cd /d "%~dp0..\lichess-bot"
"%~dp0..\.venv\Scripts\python.exe" lichess-bot.py %*
