#!/usr/bin/env python3
# Host bridge for MASTER running in serial mode (USE_BLE_KEYBOARD = 0).
# Auto-finds the master's serial port, reads "KEY:x" lines, presses the real key.
#
#   pip install pyserial pynput
#   python key_bridge.py            # auto-detect
#   python key_bridge.py COM7       # or force a specific port
#
# How auto-detect works:
#   1. Enumerate serial ports, prioritising known ESP32 USB-UART chips.
#   2. Open each candidate (this usually resets the ESP32 via DTR) and also send '?'.
#   3. The master replies with "MASTER:ESPNOWPAD MAC:..."; first match wins.
#   4. If the master is unplugged later, rescan automatically.
#
# Windows: ports look like "COM5"  |  Linux: "/dev/ttyUSB0" / "/dev/ttyACM0"
# macOS:   "/dev/cu.usbserial-XXXX" / "/dev/cu.usbmodem-XXXX"

import sys
import time

import serial
import serial.tools.list_ports
from pynput.keyboard import Controller

BAUD            = 115200
IDENTITY_TOKEN  = "MASTER:ESPNOWPAD"   # must match the master firmware
QUERY_BYTE      = b"?"

# Known USB-UART / USB-CDC chips used by ESP32 dev boards. PID None = any PID of that VID.
KNOWN_USB_IDS = [
    (0x10C4, 0xEA60),  # Silicon Labs CP2102 / CP2102N
    (0x10C4, 0xEA70),  # Silicon Labs CP2105
    (0x1A86, 0x7523),  # WCH CH340
    (0x1A86, 0x5523),  # WCH CH341
    (0x1A86, 0x55D4),  # WCH CH9102
    (0x0403, None),    # FTDI (any PID)
    (0x303A, None),    # Espressif native USB-CDC (S2/S3/C3 boards)
]


def _chip_matches(p):
    if p.vid is None:
        return False
    for vid, pid in KNOWN_USB_IDS:
        if p.vid == vid and (pid is None or p.pid == pid):
            return True
    return False


def candidate_ports():
    """Return (likely_esp32_ports, other_ports), likely ones first."""
    ports = list(serial.tools.list_ports.comports())
    likely = [p.device for p in ports if _chip_matches(p)]
    others = [p.device for p in ports if p.device not in likely]
    return likely, others


def probe(port, settle=1.8, read_window=2.5):
    """Open a port, confirm it's the master. Returns an open Serial on success, else None."""
    try:
        ser = serial.Serial(port, BAUD, timeout=0.25)
    except (serial.SerialException, OSError) as e:
        print(f"    {port}: cannot open ({e})")
        return None
    try:
        time.sleep(settle)            # let the board boot if opening reset it
        try:
            ser.reset_input_buffer()
        except Exception:
            pass
        ser.write(QUERY_BYTE)
        deadline = time.time() + read_window
        while time.time() < deadline:
            line = ser.readline().decode(errors="ignore").strip()
            if IDENTITY_TOKEN in line:
                return ser            # confirmed — keep this handle open
            if not line:
                ser.write(QUERY_BYTE) # nudge again if the board was idle
        ser.close()
        return None
    except (serial.SerialException, OSError):
        try:
            ser.close()
        except Exception:
            pass
        return None


def find_master(forced_port=None):
    if forced_port:
        print(f"using forced port {forced_port}")
        ser = probe(forced_port)
        if ser:
            return ser
        # Fall back to a raw open so a manual port still works even without the
        # identity responder (e.g. older master firmware).
        try:
            print(f"  no identity token on {forced_port}; opening it anyway")
            return serial.Serial(forced_port, BAUD, timeout=1)
        except (serial.SerialException, OSError) as e:
            print(f"  cannot open {forced_port}: {e}")
            return None

    likely, others = candidate_ports()
    order = likely + others
    print("scanning ports:", ", ".join(order) if order else "(none found)")
    for port in order:
        print(f"  probing {port} ...")
        ser = probe(port)
        if ser:
            print(f"  -> master confirmed on {port}")
            return ser
    return None


def run(ser, kb):
    print(f"bridge running on {ser.port}")
    ser.timeout = 1
    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("KEY:") and len(line) >= 5:
            k = line[4]
            kb.press(k)
            kb.release(k)
            print("pressed", k)


def main():
    forced_port = sys.argv[1] if len(sys.argv) > 1 else None
    kb = Controller()
    while True:
        ser = find_master(forced_port)
        if not ser:
            print("no master found; retrying in 3s "
                  "(check cable/driver, and that USE_BLE_KEYBOARD = 0)")
            time.sleep(3)
            continue
        try:
            run(ser, kb)
        except (serial.SerialException, OSError):
            print("master disconnected; rescanning ...")
            try:
                ser.close()
            except Exception:
                pass
            time.sleep(2)
        except KeyboardInterrupt:
            print("\nbye")
            try:
                ser.close()
            except Exception:
                pass
            return


if __name__ == "__main__":
    main()
