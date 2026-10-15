# ORACLE - EQUINOX 2026 answering robot

ESP32-S3 walking robot that listens (INMP441), answers out loud (MAX98357A), shows expressions on two OLED eyes and
moves on two L298N-driven motors. **All AI work happens on a laptop**, the ESP32 only records, plays and animates.

```
 INMP441 --I2S--> ESP32-S3 --Wi-Fi/WebSocket, raw PCM 16 kHz-->  LAPTOP (bridge.py)
                                                                   1. noise filter  (high-pass, spectral gate, trim, level)
                                                                   2. speech-to-text (local faster-whisper, offline)
                                                                   3. keyword match -> scripted answer (exact wording)
                                                                      no match      -> DeepSeek (TEXT only, over the internet)
                                                                   4. text-to-speech (local Piper, offline) -> PCM 24 kHz byte array
 MAX98357A <--I2S-- ESP32-S3 <--Wi-Fi/WebSocket, PCM bytes-------  (only for answers not already in the robot's flash)
```

**Scripted answers never travel over Wi-Fi.** On connect, the bridge uploads each scripted sentence to the ESP32's flash
once, as 16 kHz IMA ADPCM (4:1). After that the bridge just sends `PLAY:<id>` and the robot plays it straight from flash:
no synthesis, no transfer, speaking starts in tens of milliseconds, and it keeps working if the internet dies mid-event.
(The laptop must stay up: it does the listening and sends `PLAY`. If it dies the robot beeps an error on button press.) Anything not in flash - DeepSeek answers, or clips that did not fit - streams as PCM exactly as before.

**Audio never leaves the laptop.** Speech recognition and speech synthesis are local; the only thing sent to the internet is
the *text* of a question that has no scripted answer, and only text comes back (the chat API is text-only). The single
API key (`DEEPSEEK_API_KEY`) lives on the laptop in `.env`. The ESP32 never talks to the internet.

## Folder

| Path | What it is |
|---|---|
| `oracle_esp32s3/oracle_esp32s3.ino` | ESP32-S3 firmware (Arduino) |
| `bridge/bridge.py` | Laptop server: STT -> match -> LLM -> TTS |
| `bridge/tts_engine.py` | Local text-to-speech: Piper (primary), pyttsx3 (fallback), resampling |
| `bridge/audio_filter.py` | Noise filtering |
| `bridge/adpcm.py`, `bridge/clips.py` | On-device clip store: scripted answers compressed 4:1 and kept in the robot's flash |
| `bridge/matcher.py` + `bridge/knowledge.json` | Keywords, scripted answers, venue data. **Edit `knowledge.json` to change answers/keywords** |
| `bridge/test_*.py`, `testsignals.py` | Offline tests (no keys, no hardware, no network) |
| `bridge/.env.example`, `requirements.txt` | Settings and dependencies |
| `bridge/voices/` | Piper voice files (`.onnx` + `.onnx.json`) - you download these once |

## 1. Wiring (ESP32-S3-DevKitC-1 N16R8 pin set - pins are constants at the top of the .ino)

| Part | Part pin | ESP32-S3 GPIO |
|---|---|---|
| INMP441 | SCK / WS / SD | 4 / 5 / 6 |
| INMP441 | **L/R** | **GND** (left channel - never leave it floating) |
| INMP441 | VDD / GND | 3V3 / GND |
| MAX98357A | BCLK / LRC / DIN | 15 / 16 / 7 |
| MAX98357A | SD | 17 (firmware switches the amp off when silent = no idle hiss) |
| MAX98357A | VIN / GND / GAIN | 5V / GND / leave floating (9 dB, volume is set in software) |
| Left-eye OLED | SDA / SCL | 8 / 9 (bus `Wire`) |
| Right-eye OLED | SDA / SCL | 10 / 11 (bus `Wire1`) - **separate bus, so both can share address 0x3C** |
| L298N | ENA / IN1 / IN2 | 12 / 13 / 14 |
| L298N | ENB / IN3 / IN4 | 21 / 47 / 42 |
| Push-to-talk button | one leg | 1 (other leg GND, internal pull-up) |
| Bumper microswitch (optional) | one leg | 2 (other leg GND, normally open) |

Never use GPIO 0, 3, 45, 46 (strapping), 19, 20 (USB), 26-37 (flash/PSRAM), 43, 44 (UART0). 38/48 drive the on-board RGB LED.

**Power (this is where most builds fail)**
- Battery -> master switch (**hardware kill switch, reachable from outside the robot**).
- Motor pack 9-12 V -> L298N `+12V`. The L298N drops ~2 V, so a 7.4 V pack gives weak wheels.
- Separate 5 V buck converter, **>= 3 A**, feeds the ESP32 `5V` pin and the MAX98357A `VIN`. Do **not** power the ESP32 from the L298N's 5 V pin.
  If the motor supply is above 12 V, remove the L298N `5V-EN` jumper.
- One common GND (star point). 1000 uF across the motor supply at the L298N, 470-1000 uF across the amp `VIN`.
- **Remove the ENA and ENB jumpers** on the L298N (otherwise PWM speed control does nothing).
- **10 k pull-downs to GND on ENA, ENB, IN1-IN4**, so the wheels cannot twitch while the ESP32 is resetting.
- Keep the INMP441 and its wires away from the motors and speaker cable; short wires, ideally twisted with GND; a small
  foam windscreen over the mic helps in a crowd.

## 2. Install

**Firmware**
1. Arduino IDE 2.x, board package *esp32 by Espressif* (2.0.17 or 3.x).
2. Library Manager: **WebSockets** (Markus Sattler, >= 2.4) and **U8g2** (olikraus).
3. Open `oracle_esp32s3.ino`, edit the USER CONFIG block: Wi-Fi name/password (2.4 GHz), `BRIDGE_HOST` = laptop IP,
   `EYES_64X48` (1 for 0.66" 64x48 modules, 0 for 0.96" 128x64).
4. Tools: Board *ESP32S3 Dev Module*, **PSRAM = OPI PSRAM** (N8R8/N16R8) or QSPI (R2), correct flash size,
   **Partition Scheme = one with a SPIFFS/LittleFS partition** (e.g. "Default 4MB with spiffs" = 1.5 MB, or an 8M/16M
   scheme for more). The clip store uses whatever is there and simply stores fewer clips if the partition is small.
   USB CDC On Boot = Enabled if you use the USB port.
5. Upload, open Serial Monitor at 115200.

**Laptop bridge** (Python 3.10+)
```
cd bridge
python -m venv .venv && source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
cp .env.example .env                                     # then put your DEEPSEEK_API_KEY in it
```
1. **Piper voice (once).** Download BOTH files of one voice from <https://huggingface.co/rhasspy/piper-voices> into `bridge/voices/`,
   e.g. `en/en_US/lessac/medium/en_US-lessac-medium.onnx` and `en_US-lessac-medium.onnx.json` (about 60 MB), and set
   `PIPER_VOICE=voices/en_US-lessac-medium.onnx` in `.env`. Without it the bridge warns and falls back to the system voice
   (`pip install pyttsx3`), which sounds more robotic.
2. **Whisper model (once, needs internet).** The first start downloads `small.en` (about 460 MB) into the Hugging Face cache; after
   that it is fully offline. It loads in a background thread at startup - the log says when it is ready, and a question
   that arrives earlier simply waits for it. To pre-download at home:
   `python -c "from faster_whisper import WhisperModel; WhisperModel('small.en')"`.
3. `python bridge.py --prebuild` - renders every scripted answer with Piper and encodes the robot's flash clips.
   **Fully offline and free** - no internet, no key. Re-run it whenever you edit `knowledge.json` or change voice/engine
   (the cache key and clip ids include the engine and voice, so old audio is never reused by mistake).
   After this, **the scripted path needs no internet at all**.
4. Tests: `python test_matcher.py && python test_audio_filter.py && python test_adpcm.py && python test_protocol.py && python test_clips.py && python test_llm_deepseek.py && python test_tts_engine.py`
5. `python bridge.py` - prints the IP to put in `BRIDGE_HOST`.

Allow Python through the laptop firewall on **private** networks (inbound TCP 8765). Turn off sleep / hibernate.
`python bridge.py --ask "what is equinox"` tests matching from the keyboard (no API call); a question with no scripted answer,
e.g. `--ask "how far is the moon"`, calls DeepSeek (needs `DEEPSEEK_API_KEY` and internet).

## 3. Bring-up order (serial monitor, 115200; type `?` for the list)

1. `i` - shows state, Wi-Fi, buffer, PSRAM, and whether mic / speaker / both eyes initialised.
2. `e` (repeat) - cycles every eye expression. Both eyes must respond; if one is dead the log says which bus.
3. `t` - two beeps from the speaker. `+` / `-` sets volume (saved).
4. `m` - mic meter for 4 s: speak, clap, then stay quiet. Put the quiet-room RMS x 2 into `MIN_RMS` in `.env`.
   RMS stuck at 0 => INMP441 wiring or L/R pin. Very low RMS => raise gain (`MIC_SHIFT` 14 -> 13).
5. `c` lists the clips stored in flash and the free space; `C` erases them (they re-upload on the next connect).
6. **Wheels off the ground:** `f b l r s`. If a wheel spins the wrong way flip `INVERT_LEFT_MOTOR` / `INVERT_RIGHT_MOTOR`.
7. Start the bridge; the robot log shows `bridge connected`, then the clip upload (about 10-40 s, once only). Tap the button: the robot introduces itself.
   Hold, ask "What is EQUINOX?", release.

## 4. How visitors use it (put this on a sign on the robot)

Hold the button, ask your question close to the microphone, let go. **Quick tap = hello.** Press again to interrupt.
Voice commands (short phrases only): "come forward", "go back", "turn left", "turn right", "stop". Each moves the robot <= 1.5 s.

## 5. Scripted answers and the venue list

The ten questions and the greeting are in `bridge/knowledge.json` with your exact answers (I only fixed an unbalanced
quote in the theme answer and a stray space in "( Optical"). Keywords have weights (3 = phrase, 2 = distinctive word,
1 = generic word that never answers alone). Anything not confidently matched goes to DeepSeek, which is given the same
facts and told not to invent details.

From `EQUINOX_26_Responsibilities.docx` I added a "where is ..." answer for every theme (Clean Earth ... RoboSphere) and the selfie point.
**Please verify:** the document lists Classrooms 1A-3D under *First Floor* and 10B-10E, PE/Music/Dance Room, RoboSphere, EarthShot
and the selfie point under *5th Floor*; I used that literally. Also, the "principal" trigger returns the "hosted by Lodha World
School" line as specified - it does not give a person's name.

## 6. What the clip store changes

| | Before | Now |
|---|---|---|
| Scripted answer starts speaking in | ~1-2 s (transfer) | ~50 ms (flash) |
| Wi-Fi traffic per scripted answer | 300-600 KB | ~20 bytes |
| Laptop dies after startup | robot goes silent | robot still cannot listen (error beep); clips stay in flash, so a restarted bridge needs no re-upload |
| Internet dies | scripted answers need nothing online (local STT + local speech) | same, and the clips are already on the robot |

Flash budget: about 8 KB per second of speech, so a 1.5 MB SPIFFS partition holds roughly 3 minutes - enough for the
greeting, fallbacks, movement acknowledgements and the ten answers, with venue answers next in line. Clips upload in
priority order and the bridge stops when the robot says it is nearly full; whatever does not fit simply streams.
Edit an answer in `knowledge.json`, re-run `--prebuild`, and the robot swaps that clip on its own (the id is a hash of
the text, so the stale one is deleted). Set `USE_CLIPS=0` in `.env` to go back to pure streaming.

## 7. Scenario matrix (what happens, and what protects you)

### Power and hardware
| Scenario | What happens / protection |
|---|---|
| 5 V rail sags when motors start, amp peaks, Wi-Fi transmits (brownout reset) | Motors ramp over ~0.25 s, speed capped. Boot prints the reset reason (`BROWNOUT` is called out) and chirps, so an unexpected chirp at the venue = reset. **Fix is hardware:** dedicated >=3 A buck, bulk capacitors, common ground. |
| Motors twitch on power-up / reset | Firmware drives IN/EN low first; your 10 k pull-downs cover the reset window. |
| ENA/ENB jumpers still fitted | PWM has no effect, wheels run full speed. Remove them. |
| L298N voltage drop makes wheels weak | Use a 9-12 V motor pack (or swap to a TB6612 later). |
| Motor noise corrupts the recording | Firmware refuses to move unless idle: never while listening, downloading or speaking. Motors stop the instant the button is pressed. |
| INMP441 L/R pin floating, or SD/WS/SCK wrong | Recording peak is checked; log says `SILENT: check INMP441 wiring / L-R pin`; `m` command shows live levels. |
| Microphone hears the speaker (echo loop) | Half-duplex by design: push-to-talk, mic not read while speaking. Keep speaker and mic apart anyway. |
| Amp hiss or pop | Amp shut down via SD pin when silent; silence padding before/after audio; 5 ms fades on beeps. |
| Two OLEDs with the same I2C address | Each eye has its own I2C bus. Each is probed at boot; if one is missing the robot runs with the other and logs which. |
| OLED is a different size or an SH1106 | One constructor line (`EYES_64X48` or the `U8G2_...` type). |
| OLED burn-in over a full day | Eyes move constantly, dim and sleep after 10 idle minutes, wake on any button/bridge activity. |
| 3 ft robot tips or hits a child | Speed cap, ramped start/stop, every command <= 1.5 s, forward motion stops on the bumper and refuses while it is pressed, motors stop on Wi-Fi/bridge loss and on any button press. Also: keep the centre of gravity low, wide wheelbase, hardware kill switch, a volunteer nearby. Voice "stop" works. |
| Wheel turns the wrong direction | `INVERT_*_MOTOR` flags. |
| Battery runs low | Weak battery shows up as brownouts/resets first - swap or charge before it happens; carry a spare pack. |

### Network
| Scenario | What happens / protection |
|---|---|
| Venue Wi-Fi is 5 GHz-only, has a captive portal, client isolation or blocks ports | ESP32-S3 is 2.4 GHz only. **Bring your own travel router or phone hotspot** for the robot and laptop; the laptop gets internet from another link (phone USB tether / school cable). Two Wi-Fi networks can be stored in the firmware (`WIFI_SSID_2`). |
| Laptop IP changes | Reserve the laptop's IP in the router (DHCP reservation) and use the IP, not a hostname, in `BRIDGE_HOST`. |
| Bridge not running / laptop asleep | Eyes go sad, button gives an error beep; the robot probes the bridge every 3 s and reconnects itself, without freezing the animation. |
| Wi-Fi drops mid-answer | Motors stop, playback stops, partial download is discarded (length check + 5 s stall timeout), auto-reconnect, watchdog re-scans both networks after 20 s. |
| Wi-Fi jitter while speaking | The whole answer is downloaded into RAM first and only then played - no stutter. Cost: ~0.5-2 s before speech starts. |
| Internet down at the venue | Only the DeepSeek fallback answers are lost: STT, TTS and all clips are local, so scripted answers, voice commands and the greeting keep working. Unscripted questions get the spoken apology. |
| DeepSeek unavailable / quota / bad key / overloaded | There is no second provider: the bridge logs the error and the robot speaks the cached apology. Nothing hangs: LLM 9 s, TTS 12 s, whole answer 32 s. |
| Robot reboots while the bridge is connected | The new connection replaces the stale one. |
| Very long LLM answer | Prompt limits it to ~40 words, `max_tokens=160`; firmware buffer holds ~43 s (2 MB PSRAM); longer audio is split into chunks. |

### Audio and language
| Scenario | What happens / protection |
|---|---|
| Crowded, noisy hall | Push-to-talk (only the hold time is recorded), then high-pass + spectral noise gate + silence trim + level lift, then an energy gate. Mount the mic facing visitors, add a foam cover, ask people to speak close. `NOISE_FILTER=0` in `.env` to A/B if recognition gets worse - test in the real hall. |
| Accidental press / nobody speaks | Silence and noise-only recordings are rejected before any API call; Whisper's typical phantom output ("thank you", "thanks for watching") is filtered. Robot says it did not catch that. |
| Quick tap | Plays the ORACLE greeting. |
| Child speaks quietly | +12 dB gain in firmware (`MIC_SHIFT`), up to +30 dB normalisation on the laptop. |
| Indian accents, names like Smit, Lodha, Ethinova, "lightning" instead of "lighting" | Whisper is primed with the event vocabulary; the matcher has an alias table and fuzzy matching for near-verbatim questions. Add new mishearings to `ALIASES` in `matcher.py`. |
| Hindi / Marathi questions | Set `STT_LANGUAGE=` (empty) for auto-detect **and** use a multilingual Whisper model (`LOCAL_STT_MODEL=small`; the `.en` models are English-only). DeepSeek replies in the visitor's language, but the Piper voice is English, so non-English replies sound wrong unless you also install a matching voice. Scripted answers stay English. |
| Two people talk at once / interruptions | One speaker at a time by design. Pressing the button mid-answer stops the audio and cancels the pending answer. |
| Keyword appears in an unrelated question ("buy a car", "machine learning", "steam engine", "movie theme") | Weights, `min_score`, block-words and a long-sentence rule route these to DeepSeek. `test_matcher.py` has 60+ cases - add every mis-route you see. |
| Event question the script does not cover (timings, food, washrooms) | DeepSeek is told to say it is not sure and to ask a volunteer, never to invent. |
| Rude questions, "ignore your instructions", personal questions | System prompt refuses politely in one sentence and steers back; answers are capped; each question is stateless (no carry-over between visitors). |
| Privacy | Audio is not saved and never leaves the laptop (recognition and speech are local); transcripts only appear on the laptop console. **Only the text of questions with no scripted answer is sent to DeepSeek**, a China-based company, so the text leaves the school's and the country's network. Children are involved - get the school's approval for that text flow, or leave `DEEPSEEK_API_KEY` empty: scripted answers still work and nothing is sent anywhere. |

### Firmware and software
| Scenario | What happens / protection |
|---|---|
| No PSRAM enabled in the IDE | Detected and logged; falls back to a small buffer (answers play in chunks with short gaps). |
| Clip file corrupt, erased, or never uploaded | Robot answers `PLAYED:ERR`, bridge streams that answer instead and re-uploads on the next connect. |
| Flash partition small or missing | Robot reports its free space; the bridge fills it in priority order and streams the rest. If LittleFS will not mount at all, everything streams as before. |
| Visitor presses the button during a clip upload | Upload is aborted, the half-written file is deleted, the question is answered immediately, and the bridge retries when the robot is idle again. |
| Answer text edited in `knowledge.json` | New clip id -> uploaded, stale one deleted automatically. No risk of the robot speaking an old answer. |
| Corrupted / wrong-length audio | Validated; robot answers `PLAYED:ERR`, shows confused eyes, bridge stops sending. |
| USB serial with no PC attached | TX timeout disabled on core 3.x so logging never stalls the loop. |
| NVS (flash storage) failure | Volume just falls back to the default. |
| Arduino core 2.x vs 3.x differences | PWM (LEDC) wrapped for both; legacy I2S driver works on both (3.x prints a harmless deprecation warning). |
| Button bounce / stuck button | 30 ms debounce; recording is capped at 15 s. |
| Whisper model missing / fails to load | Logged clearly at startup; every question gets "I didn't catch that" instead of hanging. A question that arrives while the model is still loading waits for it. |
| TTS engine fails for a non-cached sentence | Cached apology is played instead; if even that is missing the eyes show X and the robot beeps. |

## 8. Settings you will actually touch

`bridge/.env`: `DEEPSEEK_API_KEY`, `DEEPSEEK_MODEL`, `LOCAL_STT_MODEL`, `STT_LANGUAGE`, `TTS_ENGINE`, `PIPER_VOICE`, `USE_CLIPS`,
`NOISE_FILTER`, `SCRIPT_FIRST` (0 = send everything to DeepSeek), `MIN_RMS`, `MOVE_*`.
Firmware: pins, `EYES_64X48`, `MIC_SHIFT`, `MOTOR_MIN_PWM/MAX_PWM`, `MOVE_MAX_MS`, `EYE_SLEEP_MS`.

## 9. What has and has not been verified

**Verified in a sandbox (no hardware, no DeepSeek key):** keyword matcher (74 cases incl. all ten questions and the off-topic traps), noise
filter on synthetic speech-plus-noise, the ADPCM codec, the complete bridge<->robot protocol (chunking, cancel, silence gate, fallbacks,
voice commands, bumper) and the clip store (sync, play-from-flash, missing-clip fallback, reconnect, priority order on a small flash)
against a fake ESP32, plus the firmware **syntax and types** against stub headers.
Added with the DeepSeek / local-speech migration:
- **DeepSeek step** (`test_llm_deepseek.py`, monkeypatched client): request shape (system + one stateless user message, model from env,
  `max_tokens=160`, `temperature=0.4`, `thinking` disabled), speech cleaning, and that an API error, timeout, empty/null reply, no choices or
  a missing key returns `None` so the robot speaks the cached apology. The base URL, model names and the way to disable thinking were read
  from DeepSeek's docs. **A live call to the real DeepSeek API has not been made** - no key was available in the development sandbox.
- **Local TTS** (`test_tts_engine.py`): real Piper synthesis with `en_US-lessac-medium` (22.05 kHz native) and real pyttsx3 / Windows SAPI,
  resampling to 24 kHz / 16 kHz (sample counts, no clipping - Piper peak-normalises to full scale, so `synth()` leaves ~1 dB of headroom),
  cache keys and clip ids that change with engine/voice, and engine failure -> cached apology. `--prebuild` rendered and ADPCM-encoded all
  43 clips offline (1.69 MB) with no errors.
- **Local STT:** faster-whisper `small.en` loads in the background thread, and a question that arrived before loading finished waited for
  it and was transcribed. This was tested only on a Piper-generated question, **not on real voices**. On that test the first run gave
  "Lotto World School" for "Lodha World School" and a later run got it right, so expect some misheard names and keep the matcher's `ALIASES` up to date.

**Speed on the development laptop (8 CPU cores, no GPU), one 3-second question:** `small.en` about 5.8 s, `base.en` about 1.8 s. Both fit the
32 s answer budget, but 6 s of silence feels long at an exhibition: try `LOCAL_STT_MODEL=base.en` and compare accuracy in the real hall.

**Not verified:** compilation against the real Arduino-ESP32 core and libraries, anything on real hardware (including how the Piper voice
sounds through the MAX98357A and speaker, and Whisper accuracy on children's voices, accents and a noisy hall), and the quality of live DeepSeek
replies. Expect to tune `MIN_RMS`, `MIC_SHIFT`, motor PWM and the noise filter on site. If the compiler complains about the `_2ND_HW_I2C` eye
classes, update the U8g2 library (they need a recent version).
