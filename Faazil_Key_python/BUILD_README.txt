============================================================
 key_bridge  ->  standalone Windows executable (build + run)
============================================================

WHAT THIS IS
  key_bridge listens to the ESP-NOW MASTER over USB serial and turns its
  "KEY:x" output into real keypresses on the PC. This kit builds it into a
  single .exe so target laptops do NOT need Python installed.

------------------------------------------------------------
FILES IN THIS FOLDER
------------------------------------------------------------
  key_bridge.py      the program
  requirements.txt   python dependencies (pyserial, pynput)
  build.bat          double-click to build -> dist\key_bridge.exe
  BUILD_README.txt   this file

------------------------------------------------------------
BUILD (do this once, on a Windows PC with Python)
------------------------------------------------------------
  Build machine needs:
    - Windows 10 or 11, 64-bit
    - Python 3 on PATH
      (python.org installer -> tick "Add python.exe to PATH")

  Steps:
    1. Put all files in one folder.
    2. Double-click build.bat  (or run it in a cmd window).
    3. Result:  dist\key_bridge.exe
    4. Copy that ONE file to any Windows laptop. Done.

  Cleanup (optional): delete build\, build_env\, key_bridge.spec.

------------------------------------------------------------
RUN ON ANY WINDOWS LAPTOP
------------------------------------------------------------
  - Plug in the ESP32 master (USB).
  - Double-click key_bridge.exe. A console opens, scans ports, finds the
    master, and presses keys on each "strip full" event.
  - Force a port:   key_bridge.exe COM7
  - Stop: Ctrl+C or close the window.

------------------------------------------------------------
THINGS THAT ACTUALLY MATTER ON A WINDOWS LAPTOP
------------------------------------------------------------
1. USB-UART DRIVER REQUIRED ON EACH LAPTOP.
   The exe does not include drivers. The ESP32 must show up as a COM port.
   - CP210x (Silicon Labs): usually auto-installs on Win10/11.
   - CH340 (WCH): may need a manual driver install once.
   If you see "no master found", install the driver for your board's chip.

2. ANTIVIRUS / SMARTSCREEN.
   The exe types keystrokes and is unsigned, so Defender/SmartScreen may
   warn or quarantine it.
   - First run: "Windows protected your PC" -> More info -> Run anyway.
   - If Defender removes it: add an exclusion, or code-sign the exe.
   - Fewer flags: build --onedir instead of --onefile (edit build.bat).

3. ADMIN / UAC.
   If the app receiving the keypresses runs AS ADMINISTRATOR, Windows blocks
   keystrokes from a non-admin sender (UIPI). If keys don't register there,
   right-click key_bridge.exe -> Run as administrator.

4. STARTUP DELAY.
   --onefile unpacks to a temp folder each launch (~1-3 s). Normal.

5. AUTO-START (optional).
   Win+R -> shell:startup -> Enter -> drop a shortcut to key_bridge.exe there.

------------------------------------------------------------
TROUBLESHOOTING
------------------------------------------------------------
  "no master found"          -> driver missing, bad cable, or the master is
                                flashed with USE_BLE_KEYBOARD = 1 (must be 0).
  exe closes instantly       -> run it from a cmd window to read the error.
  keys do nothing in one app -> that app is elevated; run exe as admin (#3).
  ModuleNotFoundError at run  -> rebuild; keep the --hidden-import flags
                                in build.bat.
============================================================
