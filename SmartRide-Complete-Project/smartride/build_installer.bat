@echo off
setlocal
cd /d "%~dp0"
call build_exe.bat
if errorlevel 1 exit /b 1
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo Inno Setup 6 is required to create SmartRide-Setup.exe.
  echo Download it from https://jrsoftware.org/isdl.php and run this file again.
  pause
  exit /b 1
)
"%ISCC%" SmartRide.iss
echo.
echo Installer complete: installer-output\SmartRide-Setup.exe
pause

