"""
============================================================
 24-RELAY KEYPRESS CONTROLLER
 Python serial controller for Arduino Mega relay firmware
============================================================
 Usage:
   python relay_keypress.py              (will prompt for COM port)
   python relay_keypress.py COM3         (direct COM port)
   python relay_keypress.py --port COM3  (explicit flag)

 Key Mapping (24 relays):
   Keys [a] through [x] → Toggle relay 1–24
   Shift + [a-x]        → Force relay ON
   Ctrl  + [a-x]        → Force relay OFF
   [F5]                 → All relays ON
   [F6]                 → All relays OFF
   [F7]                 → Request & display state
   [Esc]                → Quit

 Requirements:
   pip install pyserial keyboard colorama

 Packaging as .exe:
   pip install pyinstaller
   pyinstaller --onefile --name RelayController relay_keypress.py
   (note: 'keyboard' library may need admin privileges on Windows)
============================================================
"""

import sys
import time
import threading
import argparse

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[ERROR] pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

try:
    import keyboard
except ImportError:
    print("[ERROR] keyboard library not installed. Run: pip install keyboard")
    sys.exit(1)

try:
    from colorama import init as colorama_init, Fore, Style
    colorama_init()
except ImportError:
    # Fallback: no colors
    class Fore:
        RED = GREEN = YELLOW = CYAN = WHITE = MAGENTA = RESET = ""
    class Style:
        BRIGHT = RESET_ALL = ""

# ─── Configuration ──────────────────────────────────────────

SERIAL_BAUD = 115200
SERIAL_TIMEOUT = 1  # seconds
HEARTBEAT_INTERVAL = 4  # seconds (must be < Arduino's 10s timeout)
RECONNECT_DELAY = 3  # seconds between reconnect attempts
NUM_RELAYS = 24

# Key-to-relay mapping: keys 'a' through 'x' → relay 1–24
RELAY_KEYS = [chr(ord('a') + i) for i in range(NUM_RELAYS)]

# ─── State ──────────────────────────────────────────────────

relay_state = [False] * NUM_RELAYS  # True = ON
ser = None  # serial.Serial instance
ser_lock = threading.Lock()
running = True
connected = False


# ─── Display ────────────────────────────────────────────────

def clear_screen():
    """Cross-platform clear."""
    import os
    os.system('cls' if os.name == 'nt' else 'clear')


def print_banner():
    """Print the application header."""
    print(f"\n{Fore.CYAN}{Style.BRIGHT}{'=' * 60}")
    print(f"  24-RELAY KEYPRESS CONTROLLER")
    print(f"{'=' * 60}{Style.RESET_ALL}")


def print_relay_grid():
    """Print current relay states in a grid layout."""
    print(f"\n  {Fore.WHITE}{Style.BRIGHT}RELAY STATUS:{Style.RESET_ALL}\n")

    for i in range(NUM_RELAYS):
        num = i + 1
        key = RELAY_KEYS[i].upper()
        if relay_state[i]:
            tag = f"{Fore.GREEN}{Style.BRIGHT}[{key}] R{num:02d}:ON {Style.RESET_ALL}"
        else:
            tag = f"{Fore.RED}[{key}] R{num:02d}:OFF{Style.RESET_ALL}"
        print(f"  {tag}", end="")

        # 8 relays per row
        if (i + 1) % 8 == 0:
            print()

    print()


def print_controls():
    """Print control instructions."""
    print(f"  {Fore.YELLOW}Controls:{Style.RESET_ALL}")
    print(f"    [A-X]       Toggle relay 1–24")
    print(f"    Shift+[A-X] Force ON")
    print(f"    Ctrl+[A-X]  Force OFF")
    print(f"    [F5] All ON  [F6] All OFF  [F7] Status  [Esc] Quit")
    print(f"  {'─' * 50}")


def refresh_display():
    """Redraw the full console UI."""
    clear_screen()
    print_banner()
    if connected:
        print(f"  {Fore.GREEN}● Connected: {ser.port}{Style.RESET_ALL}")
    else:
        print(f"  {Fore.RED}● Disconnected{Style.RESET_ALL}")
    print_relay_grid()
    print_controls()


def log(level, msg):
    """Print a log message below the controls area."""
    colors = {
        "INFO": Fore.CYAN,
        "OK": Fore.GREEN,
        "WARN": Fore.YELLOW,
        "ERROR": Fore.RED,
    }
    color = colors.get(level, Fore.WHITE)
    print(f"  {color}[{level}] {msg}{Style.RESET_ALL}")


# ─── Serial Communication ───────────────────────────────────

def send_command(cmd):
    """Send a command string to Arduino. Returns True on success."""
    global connected
    with ser_lock:
        if ser is None or not ser.is_open:
            connected = False
            return False
        try:
            ser.write(f"{cmd}\n".encode('ascii'))
            ser.flush()
            return True
        except (serial.SerialException, OSError) as e:
            log("ERROR", f"Send failed: {e}")
            connected = False
            return False


def read_serial_loop():
    """Background thread: read and process Arduino responses."""
    global connected, running
    while running:
        if not connected or ser is None:
            time.sleep(0.5)
            continue
        try:
            with ser_lock:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('ascii', errors='replace').strip()
                else:
                    line = None

            if line:
                process_response(line)

        except (serial.SerialException, OSError):
            connected = False
            log("ERROR", "Serial connection lost")

        time.sleep(0.02)  # 20 ms poll


def process_response(line):
    """Parse and act on a response from Arduino."""
    if line.startswith("OK:"):
        # OK:NN:S
        parts = line.split(':')
        if len(parts) == 3:
            try:
                idx = int(parts[1]) - 1
                state = parts[2] == '1'
                if 0 <= idx < NUM_RELAYS:
                    relay_state[idx] = state
                    status = f"{Fore.GREEN}ON{Style.RESET_ALL}" if state else f"{Fore.RED}OFF{Style.RESET_ALL}"
                    key = RELAY_KEYS[idx].upper()
                    log("OK", f"Relay {idx+1:02d} [{key}] → {status}")
            except ValueError:
                pass

    elif line.startswith("ST:"):
        # Full state: ST:000000000000000000000000
        bits = line[3:]
        if len(bits) >= NUM_RELAYS:
            for i in range(NUM_RELAYS):
                relay_state[i] = (bits[i] == '1')
            refresh_display()
            log("INFO", "State synchronized from Arduino")

    elif line.startswith("HB:OK"):
        pass  # Silent heartbeat ack

    elif line.startswith("ERR:"):
        log("ERROR", f"Arduino: {line[4:]}")

    elif line.startswith("["):
        # Arduino diagnostic log — show it
        log("INFO", f"FW: {line}")


# ─── Heartbeat ──────────────────────────────────────────────

def heartbeat_loop():
    """Background thread: send periodic heartbeat to prevent watchdog trip."""
    global running
    while running:
        time.sleep(HEARTBEAT_INTERVAL)
        if connected:
            send_command("H")


# ─── Keypress Handling ──────────────────────────────────────

def on_key_event(event):
    """Handle a keyboard event."""
    if event.event_type != 'down':
        return

    name = event.name

    # ── Escape → Quit ──
    if name == 'esc':
        global running
        running = False
        return

    # ── F5 = All ON ──
    if name == 'f5':
        if send_command("A1"):
            log("INFO", "Command: All relays ON")
        return

    # ── F6 = All OFF ──
    if name == 'f6':
        if send_command("A0"):
            log("INFO", "Command: All relays OFF")
        return

    # ── F7 = Status ──
    if name == 'f7':
        if send_command("S"):
            log("INFO", "Requesting state...")
        return

    # ── Relay key (a-x) ──
    if len(name) == 1 and name.lower() in RELAY_KEYS:
        idx = RELAY_KEYS.index(name.lower())
        relay_num = f"{idx + 1:02d}"

        # Check modifiers
        if keyboard.is_pressed('ctrl'):
            # Force OFF
            if send_command(f"F{relay_num}"):
                log("INFO", f"Command: Relay {relay_num} → FORCE OFF")
        elif keyboard.is_pressed('shift'):
            # Force ON
            if send_command(f"N{relay_num}"):
                log("INFO", f"Command: Relay {relay_num} → FORCE ON")
        else:
            # Toggle
            if send_command(f"T{relay_num}"):
                log("INFO", f"Command: Relay {relay_num} → TOGGLE")


# ─── Serial Connection ──────────────────────────────────────

def list_com_ports():
    """List available COM ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print(f"  {Fore.YELLOW}No COM ports found.{Style.RESET_ALL}")
        return []
    print(f"\n  {Fore.CYAN}Available COM Ports:{Style.RESET_ALL}")
    for i, p in enumerate(ports):
        print(f"    {i + 1}. {p.device}  —  {p.description}")
    return ports


def connect_serial(port):
    """Open serial connection and verify handshake. Returns True on success."""
    global ser, connected
    try:
        ser = serial.Serial(
            port=port,
            baudrate=SERIAL_BAUD,
            timeout=SERIAL_TIMEOUT,
            write_timeout=SERIAL_TIMEOUT
        )
        time.sleep(2)  # Arduino resets on serial open — wait for boot

        # Flush boot messages
        while ser.in_waiting:
            line = ser.readline().decode('ascii', errors='replace').strip()
            if line:
                log("INFO", f"FW: {line}")

        # Handshake: request status
        ser.write(b"S\n")
        ser.flush()
        time.sleep(0.5)

        # Read status response
        got_state = False
        while ser.in_waiting:
            line = ser.readline().decode('ascii', errors='replace').strip()
            if line.startswith("ST:"):
                bits = line[3:]
                if len(bits) >= NUM_RELAYS:
                    for i in range(NUM_RELAYS):
                        relay_state[i] = (bits[i] == '1')
                    got_state = True
            elif line:
                log("INFO", f"FW: {line}")

        connected = True
        if got_state:
            log("OK", f"Connected to {port} — state synced")
        else:
            log("WARN", f"Connected to {port} — no state response (check firmware)")
        return True

    except serial.SerialException as e:
        log("ERROR", f"Cannot open {port}: {e}")
        connected = False
        return False


def get_com_port():
    """Get COM port from args, interactive prompt, or auto-detect."""
    parser = argparse.ArgumentParser(description="24-Relay Keypress Controller")
    parser.add_argument('port', nargs='?', help='COM port (e.g. COM3, /dev/ttyUSB0)')
    parser.add_argument('--port', '-p', dest='port_flag', help='COM port (alternate flag)')
    args = parser.parse_args()

    port = args.port or args.port_flag

    if port:
        return port

    # Interactive selection
    ports = list_com_ports()
    print(f"\n  {len(ports) + 1}. [Enter COM port manually]")
    if not ports:
        print(f"\n  {Fore.RED}No ports available. CABCDEFGHIJKLMNOPQRSTUVWXYAaonnect Arduino and retry.{Style.RESET_ALL}")
        sys.exit(1)

    while True:
        try:
            choice = input(f"\n  Enter COM port name or number (1-{len(ports)}): ").strip()
            if not choice:
                continue

            # Check if it's a number (list index)
            if choice.isdigit():
                idx = int(choice) - 1
                if 0 <= idx < len(ports):
                    return ports[idx].device
                else:
                    print(f"  {Fore.YELLOW}Choose between 1 and {len(ports)}{Style.RESET_ALL}")
            else:
                # Treat as raw port name
                return choice

        except (KeyboardInterrupt, EOFError):
            print("\n  Exiting.")
            sys.exit(0)


# ─── Main ───────────────────────────────────────────────────

def main():
    global running

    print_banner()
    port = get_com_port()

    if not connect_serial(port):
        sys.exit(1)

    # Start background threads
    reader_thread = threading.Thread(target=read_serial_loop, daemon=True)
    reader_thread.start()

    hb_thread = threading.Thread(target=heartbeat_loop, daemon=True)
    hb_thread.start()

    # Initial display
    refresh_display()

    # Hook keyboard (global listener)
    keyboard.hook(on_key_event)

    log("INFO", "Listening for keypresses... Press [Esc] to quit.")

    try:
        while running:
            time.sleep(0.1)
    except KeyboardInterrupt:
        running = False

    # Cleanup
    print(f"\n  {Fore.YELLOW}Shutting down...{Style.RESET_ALL}")
    keyboard.unhook_all()

    with ser_lock:
        if ser and ser.is_open:
            try:
                ser.write(b"A0\n")  # Safe state: all OFF on exit
                ser.flush()
                time.sleep(0.2)
                ser.close()
            except Exception:
                pass

    log("OK", "All relays OFF — connection closed. Goodbye!")


if __name__ == "__main__":
    main()
