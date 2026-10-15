@echo off
REM ============================================================
REM  Build key_bridge.exe  -  standalone Windows executable
REM  Run this ON A WINDOWS PC that has Python 3 installed.
REM  Just double-click it, or run `build.bat` in a cmd window.
REM  Output:  dist\key_bridge.exe  (copy that one file anywhere)
REM ============================================================
setlocal

REM --- sanity check: is Python on PATH? ---
python --version >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Python not found on PATH.
  echo Install Python 3 from https://www.python.org/downloads/
  echo and tick "Add python.exe to PATH" during setup.
  pause
  exit /b 1
)

REM --- 1) clean virtual env so the exe stays small and predictable ---
if exist build_env rmdir /s /q build_env
python -m venv build_env
call build_env\Scripts\activate.bat

REM --- 2) install runtime deps + PyInstaller ---
python -m pip install --upgrade pip
pip install -r requirements.txt
pip install pyinstaller

REM --- 3) build single-file console exe.
REM     --noupx avoids extra antivirus false positives.
REM     hidden-imports force in modules PyInstaller can't see (dynamic imports).
pyinstaller --onefile --console --noupx ^
  --name key_bridge ^
  --hidden-import pynput.keyboard._win32 ^
  --hidden-import pynput.mouse._win32 ^
  --hidden-import serial.tools.list_ports_windows ^
  key_bridge.py

if errorlevel 1 (
  echo.
  echo [ERROR] Build failed. See messages above.
  pause
  exit /b 1
)

echo.
echo ============================================================
echo  DONE.  Your standalone executable is here:
echo      dist\key_bridge.exe
echo  Copy that single file to any Windows laptop and run it.
echo  (Cleanup: you can delete build\, build_env\, and key_bridge.spec)
echo ============================================================
pause
