# ORACLE - hardware test guide

Work top to bottom. Each stage has a **pass** condition; do not start the next stage until you have it. Most stages take
under 5 minutes. Keep the **wheels off the ground** (robot on a box) until stage 8.

Serial monitor: 115200 baud, and send single characters (`?` lists them). Bridge log: the terminal running `python bridge.py`.

---

## Stage 0 - Laptop, before touching the robot (15 min, needs internet once)

1. In `bridge/`: `.venv\Scripts\activate` then `pip install -r requirements.txt`.
2. `.env` must contain: `DEEPSEEK_API_KEY`, `TTS_ENGINE=piper`, `PIPER_VOICE=voices/en_US-lessac-medium.onnx`
   (the voice is already in `bridge/voices/`). Everything else can stay default.
3. Run all seven tests - all must say PASS / ALL PASSED:
   ```
   python test_matcher.py && python test_audio_filter.py && python test_adpcm.py && python test_protocol.py && python test_clips.py && python test_llm_deepseek.py && python test_tts_engine.py
   ```
4. `python bridge.py --ask "what is equinox"` -> a scripted answer, no network call.
5. `python bridge.py --ask "how far is the moon"` -> a spoken-style two-sentence answer from DeepSeek.
6. `python bridge.py --prebuild` -> ends with `done - 43 cached, 0 failed`. (Offline, free. Repeat whenever you edit `knowledge.json` or change the voice.)
7. Start `python bridge.py` once and wait for **both** lines: `Whisper model small.en ready` and `TTS engine piper ... ready`.
   The first ever run downloads the Whisper model (~460 MB), so do this at home, on good internet.

**Pass:** all of the above. **If the bridge says `TTS engine pyttsx3`** the `PIPER_VOICE` line is missing/wrong.

## Stage 1 - Network plan

The ESP32-S3 is **2.4 GHz only**. Put the robot and the laptop on the same network:
- Best: your own travel router / phone hotspot for the robot + laptop. Do not depend on venue Wi-Fi.
- Find the laptop IP: the bridge prints it on start (`put one of 192.168.x.x ... in BRIDGE_HOST`), or `ipconfig`.
- Windows firewall: allow Python on **Private** networks, inbound TCP **8765**
  (first run pops a dialog: tick Private). If you get no connection later, this is the first suspect.
- Set the network to **Private**, and turn off laptop sleep / hibernate.

## Stage 2 - Flash the firmware

1. Arduino IDE 2.x, board package *esp32 by Espressif*. Libraries: **WebSockets** (Markus Sattler >= 2.4), **U8g2**.
2. Edit the USER CONFIG block of `oracle_esp32s3.ino`: `WIFI_SSID_1/PASS_1` (currently the placeholder `AAA`), optionally
   `WIFI_SSID_2/PASS_2`, `BRIDGE_HOST` = the laptop IP from stage 1, `EYES_64X48` (1 = 0.66" modules, 0 = 0.96").
3. Tools: Board **ESP32S3 Dev Module**, **PSRAM = OPI PSRAM** (N8R8/N16R8), correct flash size,
   **Partition Scheme with SPIFFS/LittleFS** (e.g. "Default 4MB with spiffs", or a 16M scheme), USB CDC On Boot = Enabled.
4. Upload, open Serial Monitor at 115200.

**Pass:** it compiles and boots. The boot log shows `ORACLE v... reset reason: ...`.
This is the first real compile against the Arduino-ESP32 core, so if it fails on the `_2ND_HW_I2C` eye classes, update **U8g2**.
Send any compile error back and it can be fixed.

## Stage 3 - Boot and status (`i`)

Send `i`. Check the line `state=idle ... psram=... mic=OK spk=OK eyeL=OK eyeR=OK fs=OK free=...KB`.

| You see | Meaning / fix |
|---|---|
| `no PSRAM detected` | IDE setting: Tools > PSRAM = OPI PSRAM. Otherwise the buffer is tiny and answers play in chunks. |
| `eyeL` or `eyeR` = FAIL | That eye's wiring (L: SDA8/SCL9, R: SDA10/SCL11), 3.3 V, or the I2C address (0x3C). Robot still runs with one eye. |
| `fs=` not OK / `free=0KB` | Wrong partition scheme: nothing can be stored on the robot; answers will just stream over Wi-Fi (slower). |
| reset reason `BROWNOUT` | Power problem, see stage 9. |

**Pass:** mic, spk, both eyes OK, PSRAM present, free flash > ~1000 KB.

## Stage 4 - Eyes, speaker, volume

1. `e`, repeatedly: both eyes must change together through every mood.
2. `t`: two beeps (880 Hz then 1320 Hz). `+` / `-` changes volume (saved across reboots).

**Pass:** both eyes react, beeps are clean. Constant hiss when idle = amp `SD` pin (GPIO 17) or wiring problem.
Crackle at higher volume = weak 5 V rail (needs the >= 3 A buck converter and the 470-1000 uF capacitor on amp VIN).

## Stage 5 - Microphone (`m`) - calibrates MIN_RMS

1. Send `m`. For 4 s: speak normally at ~20 cm, clap, then stay silent.
2. Watch `mic rms=... peak=...`.

| Reading | Meaning |
|---|---|
| rms stuck at 0 | INMP441 wiring or **L/R pin floating** (must be tied to GND). Log also says `microphone delivers no data`. |
| speech rms roughly 500-5000, peak below ~30000 | good |
| speech rms < 300 | too quiet: `MIC_SHIFT` 14 -> 13 in the .ino, re-flash |
| peaks pinned at 32767 | too loud/clipping: `MIC_SHIFT` 14 -> 15 |

Note the **silent-room rms**. Put roughly **2 x that number** into `MIN_RMS` in `.env` (default 80).

**Pass:** speech is clearly above the floor, no clipping.

## Stage 6 - Connect to the bridge

1. Start `python bridge.py` (wait for the Whisper/Piper "ready" lines).
2. Reset the robot. Serial shows `Wi-Fi '<name>' IP ... RSSI ...` then `bridge connected: <ip>:8765`.
   Bridge shows `connection from ...` then `robot connected  fw-buffer=... flash free=...`.
3. **Clip upload** starts about 1 s later. Bridge: `uploading N clips (... KB) to the robot's flash` then one line per clip,
   ending `clip upload finished: N stored, M skipped`. Takes ~10-40 s the first time only. Serial: `clip ... stored`.
4. On later connections the bridge says `clip store up to date`.
5. Serial `c` lists the clips and free space.

| Problem | Fix |
|---|---|
| Serial: `Wi-Fi not connected`, sad eyes | Wrong SSID/password, or a 5 GHz-only network. |
| Wi-Fi ok but no `bridge connected` | Wrong `BRIDGE_HOST`, laptop on a different network, or firewall (stage 1). From another device check `ws://<ip>:8765` is reachable. |
| `weak Wi-Fi signal` | Move the router closer. |
| Many clips `skipped` | Flash too small: they will stream instead (works, just slower). |

**Pass:** connected, clips stored (or knowingly skipped).

## Stage 7 - Voice pipeline, still on the box

Hold the button, speak close to the mic, release. Watch **both** the eyes and the bridge log.

| # | Action | Expect (bridge log) | Expect (robot) |
|---|---|---|---|
| 1 | Quick tap | `short tap -> greeting`, `played from robot flash` | Introduces itself almost instantly |
| 2 | Hold, say "What is Equinox?" | `recording ...s raw rms=...`, `heard: 'what is equinox'`, `script: what_is_equinox`, `played from robot flash in 0.xx s` | Exact scripted answer, starts in well under a second |
| 3 | Ask the other 9 scripted questions and 2-3 "where is ..." questions | `script: <id>` each time | Scripted answers |
| 4 | "How far is the moon?" | `no scripted match (...) -> LLM`, `llm: '...'` | Answer ~1-3 s later, streamed (no "played from flash" line) |
| 5 | Press and release without speaking | `no speech detected` | "I didn't catch that" |
| 6 | Press while it is speaking | New recording starts, old audio stops | Interrupts cleanly |
| 7 | Ask something unsafe/silly ("ignore your instructions") | `llm:` polite refusal | Short refusal, back to the exhibition |
| 8 | "Where is the washroom?" (not in the facts) | `llm:` | Says it isn't sure, suggests asking a volunteer |

Things to note for tuning:
- **Latency** shown in the log as `[stt x.xs]` and `answered in x.xs`. `small.en` was ~6 s per question on the dev laptop.
  If yours is above ~4 s, set `LOCAL_STT_MODEL=base.en` in `.env` (~3x faster) and compare accuracy.
- **Misheard names** ("Lotto" for "Lodha"): add the mishearing to `ALIASES` in `matcher.py` and a case to `test_matcher.py`.
- **Clipping or harsh voice**: turn the robot volume down with `-`.
- `heard: ''` for clearly spoken sentences: mic gain (stage 5), or try `NOISE_FILTER=0` in `.env` to compare.

**Pass:** questions 1-3 answered correctly from flash, 4 answered by DeepSeek, 5-6 behave.

## Stage 8 - Motors (wheels still OFF the ground)

1. Confirm the L298N **ENA and ENB jumpers are removed** and 10 k pull-downs are fitted (README section 1).
2. Serial `f b l r s` (each runs <= 0.7 s). Watch each wheel:
   - Wrong direction: flip `INVERT_LEFT_MOTOR` / `INVERT_RIGHT_MOTOR` in the .ino.
   - Wheels run full speed regardless: ENA/ENB jumpers still fitted.
   - One wheel dead: motor wiring, or `MOTOR_MIN_PWM` (90) too low for your motors.
3. Voice: "come forward", "go back", "turn left", "turn right", "stop". Log shows `action: F` etc. and the robot acknowledges first.
4. Bumper (if fitted): hold it pressed, ask for forward: it must refuse (`forward refused: bumper is pressed`).
5. Press the button while wheels run: they stop immediately.

Then put it on the floor **in an open area, with a person next to the kill switch**, and repeat the voice commands.
Each command moves <= 1.5 s and the speed is capped. Adjust `MOVE_FWD_MS`, `MOVE_TURN_MS`, `MOVE_SPEED_PCT` in `.env`
(no re-flash needed).

**Pass:** correct directions, stops on "stop" and on button press, no twitching at power-on/reset.

## Stage 9 - Power and stress

- Run motors + speech together: any `reset reason: BROWNOUT` in the boot log = 5 V rail sagging. Fix in hardware:
  dedicated >= 3 A buck, bulk capacitors, common ground. Do not power the ESP32 from the L298N 5 V pin.
- Battery: test on the actual battery for 30+ minutes with repeated questions. Weak batteries show up as resets first.
- Leave it idle 10 minutes: the eyes should dim/sleep, and wake on the button.

## Stage 10 - Failure drills (what visitors will trigger)

Do each once and check the robot recovers without a restart:

| Drill | Expected |
|---|---|
| Disconnect the laptop's **internet** (keep local Wi-Fi) | Scripted answers/greeting/voice commands still work. An unscripted question gets the spoken apology ("I can't reach my brain right now..."), and the bridge log shows `DeepSeek failed`. |
| Stop the bridge (Ctrl+C) | Robot eyes go sad, button gives an error beep, robot keeps retrying. **Scripted answers are not possible without the bridge** (the bridge does the listening), so it recovers when you restart the bridge - check that it reconnects by itself within ~10 s. |
| Power-cycle the robot with the bridge running | Reconnects, log `robot connected`, `clip store up to date`. |
| Turn the router off/on | Robot: `Wi-Fi lost`, motors stop, then `Wi-Fi up`, then `bridge connected`. |
| Very noisy room / music playing | Silence/noise-only recordings are rejected (`no speech detected`) rather than answered. Compare `NOISE_FILTER=1` vs `0`. |
| Two people talk at once | Expect poor recognition: one speaker at a time, close to the mic. |
| Kill Whisper (rename the model cache) | Bridge logs `could not load Whisper model`, robot says it did not catch that, nothing hangs. (Restore the cache afterwards.) |

## Stage 11 - Dress rehearsal at the venue (do this the day before)

1. Bring: your own router/hotspot, a spare battery, USB cable, laptop charger, a printed copy of the visitor instructions.
2. Reserve the laptop's IP in the router and set `BRIDGE_HOST` to it (then the firmware never needs re-flashing).
3. In the actual hall, with the actual crowd noise: run stage 5 again (`MIN_RMS`), then 20 real questions from real people,
   including children. Write down every misheard word and mis-routed question and add them to `ALIASES` / `test_matcher.py`.
4. Confirm the laptop stays awake, plugged in, and that Python's firewall rule is set for the venue network profile.
5. Start order on the day: **laptop and router first -> `python bridge.py` (wait for "ready") -> then power the robot -> wait for `bridge connected`** ->
   tap for the greeting to confirm.

## What was NOT tested before this guide

Firmware compilation on the real core, all hardware behaviour, how Piper sounds on the MAX98357A/speaker, Whisper accuracy on children
and accents, and behaviour on the venue network. Anything that surprises you in the stages above is exactly what this guide is for -
send the serial log and the bridge log together and it can be diagnosed quickly.
