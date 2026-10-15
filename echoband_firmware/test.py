import requests
import time
import wave
import struct
import io
import os
import sys
from datetime import datetime

# pip install openai-whisper
# If you want faster inference: pip install faster-whisper
import whisper

# ─── CONFIG ────────────────────────────────────────────────────────────────────
ESP32_IP      = "192.168.4.1"
SAMPLE_RATE   = 16000   # Must match I2S_SAMPLE_RATE in firmware
SAMPLE_WIDTH  = 2       # 16-bit = 2 bytes
CHANNELS      = 1       # Mono (INMP441 left channel only)
WHISPER_MODEL = "base"  # tiny | base | small | medium | large
                         # "tiny" is fastest; "small" gives best accuracy for "help"
# ───────────────────────────────────────────────────────────────────────────────

# Load Whisper once at startup (heavy — don't reload in loop)
print(f"[BOOT] Loading Whisper model '{WHISPER_MODEL}'...")
stt_model = whisper.load_model(WHISPER_MODEL)
print(f"[BOOT] Whisper ready.\n")


def raw_pcm_to_wav(raw_bytes: bytes,
                   sample_rate: int = SAMPLE_RATE,
                   sample_width: int = SAMPLE_WIDTH,
                   channels: int = CHANNELS) -> bytes:
    """
    Wraps raw signed-16-bit PCM (little-endian) from the INMP441
    in a proper WAV container so Whisper can read it.
    Returns WAV bytes in memory — no disk I/O.
    """
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(sample_width)
        wf.setframerate(sample_rate)
        wf.writeframes(raw_bytes)
    return buf.getvalue()


def run_stt(raw_path: str) -> str:
    """
    Converts .raw PCM → in-memory WAV → Whisper transcription.
    Writes a temp .wav beside the .raw file (avoids ffmpeg dependency).
    Returns the transcribed text string.
    """
    # Read raw PCM
    with open(raw_path, "rb") as f:
        raw_bytes = f.read()

    if len(raw_bytes) < 1024:
        return ""  # Too short — likely silence or incomplete chunk

    # Write temp WAV (Whisper needs a file path, not bytes)
    wav_path = raw_path.replace(".raw", ".wav")
    wav_bytes = raw_pcm_to_wav(raw_bytes)
    with open(wav_path, "wb") as f:
        f.write(wav_bytes)

    try:
        # fp16=False → safer on CPU; set True if running on GPU
        result = stt_model.transcribe(wav_path, fp16=False, language="en")
        text = result["text"].strip()
    except Exception as e:
        print(f"[STT ERROR] Whisper failed: {e}")
        text = ""
    finally:
        # Clean up temp WAV
        if os.path.exists(wav_path):
            os.remove(wav_path)

    return text


def print_dashboard(data: dict) -> None:
    """
    Prints a compact, human-readable sensor snapshot to the terminal.
    Replaces log_to_dashboard().
    """
    now = datetime.now().strftime("%H:%M:%S")

    hr      = data.get("heart_rate", {})
    gps     = data.get("gps", {})
    accel   = data.get("accelerometer", {})
    trig    = data.get("triggers", {})
    audio   = data.get("audio", {})
    hw      = data.get("hardware", {})
    state   = data.get("state", "?")
    uptime  = data.get("uptime_ms", 0) // 1000

    # Colour codes (works in most terminals; ignored gracefully in others)
    RED    = "\033[91m"
    YLW    = "\033[93m"
    GRN    = "\033[92m"
    CYN    = "\033[96m"
    RST    = "\033[0m"
    BLD    = "\033[1m"

    state_color = {
        "NORMAL":    GRN,
        "ALERT":     YLW,
        "EMERGENCY": RED,
        "STANDBY":   CYN,
    }.get(state, RST)

    bpm_str = (f"{hr.get('bpm', 0):.0f} BPM"
               if hr.get("valid") else "-- (no finger)")
    gps_str = (f"{gps.get('latitude', 0):.6f}, {gps.get('longitude', 0):.6f}"
               if gps.get("valid") else f"No fix ({gps.get('satellites', 0)} sat)")

    wake    = trig.get("wake_word_count", 0)
    wake_th = trig.get("wake_word_needed", 3)
    rec_str = "● REC" if audio.get("recording") else "○ idle"
    hw_ok   = [k for k, v in hw.items() if v]
    hw_fail = [k for k, v in hw.items() if not v]

    print(f"\n{'─'*60}")
    print(f"  {BLD}EchoBand  {now}  uptime {uptime}s{RST}")
    print(f"  State    : {state_color}{BLD}{state}{RST}")
    print(f"  Heart HR : {RED if hr.get('panic') else RST}{bpm_str}{RST}"
          + (f"  {RED}⚠ PANIC{RST}" if hr.get("panic") else ""))
    print(f"  GPS      : {gps_str}")
    print(f"  Accel    : {accel.get('magnitude_g', 0):.2f} g"
          + (f"  {RED}⚠ EXCESS{RST}" if trig.get("excess_movement") else ""))
    print(f"  Wake word: {wake}/{wake_th}"
          + (f"  {RED}⚠ THRESHOLD{RST}" if wake >= wake_th else ""))
    print(f"  Audio    : {rec_str}  chunk #{audio.get('chunk_index', '--')}")
    if hw_fail:
        print(f"  HW FAIL  : {RED}{', '.join(hw_fail)}{RST}")
    else:
        print(f"  Hardware : {GRN}all OK{RST}")
    print(f"{'─'*60}")


# ─── MAIN LOOP ──────────────────────────────────────────────────────────────────

print("[BOOT] Starting EchoBand host loop...")

while True:
    try:
        # 1. Fetch all sensor data
        data = requests.get(f"http://{ESP32_IP}/api/all", timeout=5).json()

        # 2. Check if a new audio chunk is ready
        if data["audio"]["chunk_ready"]:
            chunk_idx  = data["audio"]["chunk_index"]
            raw_path   = f"chunk_{chunk_idx}.raw"

            print(f"[AUDIO] Downloading chunk {chunk_idx}...")
            audio_resp = requests.get(f"http://{ESP32_IP}/api/audio", timeout=30)
            with open(raw_path, "wb") as f:
                f.write(audio_resp.content)
            print(f"[AUDIO] Saved {len(audio_resp.content):,} bytes → {raw_path}")

            # 3. Run Whisper STT on the raw chunk
            print("[STT]  Transcribing...")
            stt_text = run_stt(raw_path)
            print(f"[STT]  Result: \"{stt_text}\"")

            # 4. Send STT result back to ESP32 (wake-word counting happens there)
            resp = requests.post(
                f"http://{ESP32_IP}/api/stt",
                json={"text": stt_text},
                timeout=5
            ).json()
            print(f"[STT]  ESP32 ack → help_found={resp.get('help_found', 0)}"
                  f"  total={resp.get('wake_word_total', 0)}/{resp.get('threshold', 3)}"
                  f"  triggered={resp.get('triggered', False)}")

        # 5. Print sensor snapshot to terminal
        print_dashboard(data)

    except requests.exceptions.ConnectionError:
        print(f"\n[WARN] Cannot reach ESP32 at {ESP32_IP} — retrying...")
    except requests.exceptions.Timeout:
        print(f"\n[WARN] Request timed out — ESP32 busy?")
    except Exception as e:
        print(f"\n[ERROR] {e}")

    time.sleep(1)