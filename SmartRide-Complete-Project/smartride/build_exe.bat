@echo off
setlocal
cd /d "%~dp0"
if not exist .venv python -m venv .venv
call .venv\Scripts\activate.bat
python -m pip install --upgrade pip
pip install -r requirements.txt
pyinstaller --noconfirm --clean --name SmartRide --onefile ^
  --add-data "templates;templates" ^
  --add-data "static;static" ^
  --hidden-import engineio.async_drivers.threading ^
  app.py
echo.
echo Build complete: dist\SmartRide.exe

