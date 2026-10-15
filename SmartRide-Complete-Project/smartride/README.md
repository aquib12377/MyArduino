# SmartRide — Smart Helmet + Bike Safety Prototype

SmartRide is a complete local-Wi-Fi prototype comprising:

- a Flask web application with rider and guardian accounts;
- live helmet, alcohol, GPS, battery, bike and accident status;
- a two-minute accident-alert countdown with physical cancellation;
- emergency SMS firmware support through SIM800L;
- ride history, route points, distance, speed and safety events;
- a transparent prototype safety score and explanation;
- one combined ESP32 firmware file for the helmet sensors and bike controller;
- a hardware-free ride simulator; and
- a Windows `SmartRide.exe` build script.

> This is an educational prototype, not a certified vehicle safety device. Test with a small DC motor first. Do not connect a hobby relay or development board directly to a real motorcycle ignition, starter, fuel pump or safety circuit.

## 1. Architecture

The ESP32 creates its own Wi-Fi hotspot (SoftAP) and is always reachable at the fixed address `192.168.4.1`. It does **not** join a home router or a phone's internet hotspot. The Windows PC running Flask (and any phone viewing the dashboard) instead joins the ESP32's hotspot.

1. The single ESP32 reads the helmet switch, MQ-3, MPU6050, GPS, battery, start button and accident-cancel button.
2. The ESP32 locally allows the prototype bike motor only when the helmet is worn and alcohol is not detected.
3. The same ESP32 sends helmet and bike telemetry to Flask every second for the dashboards and ride history.
4. The ESP32 handles accident timing locally. After 120 seconds without cancellation, it sends the GPS link through SIM800L and reports confirmation to Flask.
5. Rider and guardian browsers receive current data from Flask, as long as their device is joined to the ESP32's hotspot.

Because the start decision is local, temporary Wi-Fi/server failure does not bypass the helmet or alcohol rules. Dashboard updates and ride storage pause until connectivity returns.

The local URL is normally `http://PC-IP-ADDRESS:5000` — with a SoftAP setup, that's the PC's fixed static IP on the ESP32's network (see section 6), for example `http://192.168.4.2:5000`.

**Practical trade-off of this mode:** because the ESP32 has no internet uplink, nothing on this network has internet either. Live map tiles won't load (coordinates and history still work), NTP time isn't available to the ESP32 (Flask's clock is used instead — no functional loss), and any phone that wants the guardian dashboard must be within Wi-Fi range of the ESP32 and joined to its hotspot, not just "on the same Wi-Fi as before." A guardian who wants to watch from somewhere else — a different room, home while the rider is out — is out of range by design; that only works with the ESP32 joining an actual internet-connected network, which is the opposite of what this mode does.

## 2. Components

### Single ESP32 prototype

| Qty | Component | Purpose |
|---:|---|---|
| 1 | ESP32 DevKit V1 (38-pin recommended) | All sensors, motor permission and Wi-Fi |
| 1 | MPU6050 | Acceleration and tilt |
| 1 | MQ-3 module | Alcohol prototype sensor |
| 1 | NEO-6M GPS | Position and speed |
| 1 | SIM800L EVB board | Emergency SMS |
| 1 | Helmet strap microswitch/limit switch | Helmet-worn detection |
| 1 | Momentary push button | Cancel accident alert |
| 1 | Momentary push button | Prototype bike start/stop input |
| 1 | Active buzzer module | Local warning |
| 1 | Logic-level MOSFET module or L298N | Small DC motor control |
| 1 | Small DC geared motor/wheel | Safe ignition demonstration |
| 2 | Red and green LED + 220 Ω resistors | Start permission indication |
| 1 | Stable regulated 5 V supply | ESP32/GPS/MQ-3 rail |
| 1 | SIM800L-suitable 4.0 V supply, 2 A peak | Cellular power |
| 1 | Separate motor-rated supply | Motor power |
| 2 | 100 kΩ resistors | Battery voltage divider |
| 2 | 10 kΩ + 20 kΩ resistors | MQ-3 output divider if module output can reach 5 V |

Also required: a Windows PC with a 2.4 GHz Wi-Fi adapter (to join the ESP32's own hotspot — no separate router or phone hotspot is needed), jumper wires and common ground where specified.

## 3. Connections

### Single ESP32 connections

| Module | Module pin | ESP32 / supply |
|---|---|---|
| Helmet switch | One side | GPIO 27 |
| Helmet switch | Other side | GND |
| Cancel button | One side | GPIO 26 |
| Cancel button | Other side | GND |
| Start/stop button | One side | GPIO 14 |
| Start/stop button | Other side | GND |
| MPU6050 | SDA | GPIO 21 |
| MPU6050 | SCL | GPIO 22 |
| MPU6050 | VCC/GND | 3.3 V / GND |
| MQ-3 | AO | GPIO 34 through voltage divider |
| MQ-3 | VCC/GND | 5 V / GND |
| Battery divider midpoint | — | GPIO 35 |
| Active buzzer signal | IN | GPIO 25 |
| Motor driver | EN or logic input | GPIO 13 |
| Green LED | Anode through 220 Ω / cathode | GPIO 18 / GND |
| Red LED | Anode through 220 Ω / cathode | GPIO 19 / GND |
| GPS | TX | GPIO 16 (ESP32 RX1) |
| GPS | RX | GPIO 17 (ESP32 TX1, optional) |
| GPS | VCC/GND | Per breakout rating / GND |
| SIM800L | TX | GPIO 32 (ESP32 RX2) |
| SIM800L | RX | GPIO 33 (ESP32 TX2; add level shifting if required) |
| SIM800L | Power | Dedicated supply per board specification |

All grounds must be common. The bare SIM800L supply must tolerate approximately 2 A current bursts. Never power it from the ESP32 3.3 V pin.

Connect the motor to the driver output and power the motor driver from a separate motor-rated supply. Join the driver ground to ESP32 ground. The sketch defaults to `MOTOR_ACTIVE_LOW = true` for its existing relay wiring. Set it to `false` for a HIGH-enable motor driver. Verify that the motor is off at startup.

## 4. Run the website from source on Windows

1. Install Python 3.11 or 3.12 and select **Add Python to PATH** during installation.
2. Extract this project.
3. Double-click `run_windows.bat`.
4. Windows Firewall may ask for permission. Allow **Private networks** so phones on the same Wi-Fi can connect.
5. The browser opens automatically. The console also shows the phone/guardian URL.

Default prototype accounts:

| Role | Email | Password |
|---|---|---|
| Rider | `rider@smartride.local` | `Rider@123` |
| Guardian | `guardian@smartride.local` | `Guardian@123` |

Change these passwords from **Edit profile**. Before a real demo, also change the secret and device key using the variables documented in `.env.example`.

## 5. Build the Windows EXE or installer

Double-click `build_exe.bat`. The first build installs dependencies and may take several minutes. The result is:

`dist\SmartRide.exe`

Copy that EXE to the demo PC and run it. Its SQLite database is stored under `%LOCALAPPDATA%\SmartRide\data`, so records survive replacement of the EXE.

For a normal Windows setup wizard:

1. Install **Inno Setup 6**.
2. Double-click `build_installer.bat`.
3. The finished installer is `installer-output\SmartRide-Setup.exe`.

The setup wizard installs SmartRide for the current Windows user and can create Start Menu and desktop shortcuts.

## 6. Configure and upload the firmware

Arduino IDE setup:

1. Install the **esp32 by Espressif Systems** board package.
2. Install `ArduinoJson`, `Adafruit MPU6050`, `Adafruit Unified Sensor`, and `TinyGPSPlus` from Library Manager.
3. Open `firmware/SmartRide_Single_ESP32/SmartRide_Single_ESP32.ino`.
4. Set `AP_SSID`, `AP_PASSWORD` (8+ characters), device key and emergency phone near the top of the sketch. Leave `SERVER_BASE_URL` as `http://192.168.4.2:5000` if you follow step 6b below unchanged; the ESP32's own address (`192.168.4.1`) never needs to change.
5. Select **ESP32 Dev Module**, the correct COM port and 115200 upload speed, then upload this one sketch to the ESP32.

### 6a. Join the PC to the ESP32's hotspot

After uploading, power the ESP32. It broadcasts the `AP_SSID` network. On the Windows PC, connect to that Wi-Fi network like any other, using `AP_PASSWORD`.

### 6b. Give the PC a fixed static IP on that network (do this once)

The ESP32's own IP (`192.168.4.1`) is always fixed, but by default the PC's IP is handed out by the ESP32's DHCP server and isn't guaranteed to be the same every time it reconnects — which is exactly the "IP changes" problem this setup is meant to avoid. Pin it down once:

1. Windows Settings → **Network & internet → Wi-Fi**, click the `AP_SSID` network → **Edit** IP settings (or **Properties**, depending on Windows version).
2. Change from **Automatic (DHCP)** to **Manual**, turn on **IPv4**, and set:
   - IP address: `192.168.4.2`
   - Subnet mask/prefix length: `255.255.255.0` (or `/24`)
   - Gateway: `192.168.4.1`
   - DNS: can be left blank — this network has no internet access anyway.
3. Save, and reconnect to the `AP_SSID` network if it doesn't reconnect automatically.

This matches the `SERVER_BASE_URL` already set in the firmware, so the ESP32 always finds the PC at the same address, and the PC always finds the ESP32 at `192.168.4.1`. Skip this step only if you plan to read the PC's assigned IP from `ipconfig` each time and re-flash `SERVER_BASE_URL` to match — not recommended, since it's exactly what a fixed hotspot IP is meant to avoid.

Do not put `localhost` or `127.0.0.1` in ESP32 firmware.

## 7. Test without hardware

Start the website, sign in, then open Command Prompt in the project folder:

```bat
.venv\Scripts\python scripts\simulator.py
```

It creates a short sample ride around Mumbai, shows a ten-second pending accident and then cancels it. Open **Ride history** after it ends.

## Simple ride controls

- Press Start once to start with helmet worn and no alcohol detected; press again to stop.
- Accident detection runs only during a ride. An impact or sustained tilt stops the motor and starts the 120-second alert countdown.
- Press Cancel once to dismiss the pending alert. If it expires, the controller attempts emergency SMS.
- Press Start for the next ride to automatically re-arm detection after cancellation or confirmation. Start cannot bypass a pending alert.
- No three-second reset hold is required. Helmet removal or alcohol detection still stops the prototype motor.

## 8. Calibration

- Allow the MQ-3 heater to warm up for several minutes in the prototype and determine clean-air and alcohol-test readings through Serial Monitor. Update `MQ3_THRESHOLD`. MQ-3 is not a certified breathalyser.
- Mount the MPU6050 firmly with its Z axis pointing up and print acceleration/tilt readings during safe bench tests. Tune `IMPACT_G_THRESHOLD` and `TILT_THRESHOLD_DEG` to reduce false alerts.
- Verify GPS outdoors; the NEO-6M may take several minutes for its first fix.
- Calibrate the battery divider multiplier against a multimeter reading.
- Test SMS separately before relying on accident flow. A 2G-capable SIM/network is required for SIM800L.

## 9. Prototype scoring

The current safety score starts at 100 and deducts points for excessive speed, sudden speed changes, helmet removal, alcohol events and confirmed accidents. The calculation is in `scoring.py` and returns an explanation. It is deliberately labelled a prototype rule-based score; genuine AI/ML requires a larger labelled riding dataset.

## 10. Local prototype limitations

- Guardian access works only while the PC (running Flask) and phone are both joined to the ESP32's own hotspot — typically within normal Wi-Fi range (roughly 20-40 m indoors, less outdoors on a moving bike). This mode has no internet uplink, so it cannot notify a guardian who is elsewhere; only the SIM800L SMS path reaches beyond that range.
- The ESP32's Wi-Fi radio supports a handful of simultaneous hotspot clients (a few devices by default) — the PC plus one or two phones is fine; don't expect many phones to join at once.
- One ESP32 is practical when the helmet sensors and prototype bike controls are physically wired to that board. A real separate helmet and motorcycle would require either a detachable cable or a second wireless controller.
- OpenStreetMap tiles require internet, which this network doesn't have. Coordinates, telemetry and history still work without map tiles.
- SIM800L SMS can work independently of Wi-Fi, but requires compatible cellular coverage and proper power.
- The website uses HTTP on a trusted local network. Internet deployment requires HTTPS, stronger account management, CSRF protection, secure secrets, rate limiting and a production database.
- For a real motorcycle, a qualified automotive electrician must design an isolated, fused, transient-protected fail-safe interface. The supplied motor demonstration circuit is the intended scope.

## 11. Automated test

From the activated environment:

```bat
python -m unittest discover -s tests -v
```
