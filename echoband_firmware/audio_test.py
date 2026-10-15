"""
audio_test.py  —  EchoBand live speech-to-text tester
Usage:
  python audio_test.py                  # one-shot: record → transcribe → print
  python audio_test.py --loop           # keep looping until Ctrl-C
  python audio_test.py --ip 192.168.1.100  # custom IP

Requires: pip install openai-whisper requests
"""

import argparse
import io
import os
import sys
import time
import wave

import requests
import whisper

# ── Config ───────────────────────────────────────────────────────────────────
DEFAULT_IP     = "192.168.4.1"   # SoftAP address — always reachable
STREAM_ENDPOINT = "/api/audio/stream"
SAMPLE_RATE    = 16000
SAMPLE_WIDTH   = 2               # 16-bit = 2 bytes
CHANNELS       = 1
WHISPER_MODEL  = "base"          # tiny | base | small | medium | large
# ─────────────────────────────────────────────────────────────────────────────

def load_model(name: str):
    print(f"[BOOT] Loading Whisper '{name}' model...", flush=True)
    m = whisper.load_model(name)
    print("[BOOT] Whisper ready.\n", flush=True)
    return m

def raw_to_wav(raw_bytes: bytes) -> bytes:
    """Wrap raw signed-16-bit PCM in a WAV container (in memory)."""
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(raw_bytes)
    return buf.getvalue()

def record_and_transcribe(ip: str, model) -> str:
    """
    1. Hit /api/audio/stream  → ESP32 records 3 s and returns raw PCM
    2. Wrap in WAV
    3. Run Whisper
    4. Return transcript
    """
    url = f"http://{ip}{STREAM_ENDPOINT}"
    print(f"[REC]  Requesting 3 s recording from {url} ...", flush=True)
    t0 = time.time()

    try:
        resp = requests.get(url, timeout=20)
    except requests.exceptions.ConnectionError:
        print(f"[ERR]  Cannot reach {ip} — is the device on?")
        return ""
    except requests.exceptions.Timeout:
        print("[ERR]  Request timed out (>20 s)")
        return ""

    if resp.status_code != 200:
        print(f"[ERR]  HTTP {resp.status_code}: {resp.text[:120]}")
        return ""

    raw_bytes = resp.content
    elapsed   = time.time() - t0
    print(f"[REC]  Got {len(raw_bytes):,} bytes in {elapsed:.1f}s", flush=True)

    if len(raw_bytes) < 1024:
        print("[ERR]  Too few bytes — mic may not be working")
        return ""

    # Write temp WAV (Whisper needs a file path, not bytes)
    wav_path = "_test_audio.wav"
    with open(wav_path, "wb") as f:
        f.write(raw_to_wav(raw_bytes))

    print("[STT]  Transcribing...", flush=True)
    t1 = time.time()
    try:
        result = model.transcribe(wav_path, fp16=False, language="en")
        text   = result["text"].strip()
    except Exception as e:
        print(f"[STT]  Whisper error: {e}")
        text = ""
    finally:
        if os.path.exists(wav_path):
            os.remove(wav_path)

    stt_elapsed = time.time() - t1
    print(f"[STT]  Done in {stt_elapsed:.1f}s", flush=True)
    return text

def count_wake_word(text: str, word: str = "help") -> int:
    lower = text.lower()
    count, pos = 0, 0
    while True:
        idx = lower.find(word, pos)
        if idx == -1:
            break
        count += 1
        pos = idx + len(word)
    return count

def send_stt_to_esp(ip: str, text: str) -> dict:
    """Forward transcript to ESP32 so it can update wake-word counter."""
    try:
        r = requests.post(
            f"http://{ip}/api/stt",
            json={"text": text},
            timeout=5
        )
        return r.json()
    except Exception as e:
        print(f"[WARN] Could not send STT to ESP32: {e}")
        return {}

# ── Main ─────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="EchoBand audio/STT tester")
    parser.add_argument("--ip",    default=DEFAULT_IP, help="ESP32 IP address")
    parser.add_argument("--model", default=WHISPER_MODEL,
                        choices=["tiny", "base", "small", "medium", "large"])
    parser.add_argument("--loop",  action="store_true",
                        help="Keep recording in a loop until Ctrl-C")
    parser.add_argument("--no-send", action="store_true",
                        help="Don't forward transcript to ESP32 /api/stt")
    args = parser.parse_args()

    model = load_model(args.model)

    def run_once():
        print("\n" + "─" * 50)
        print(">>> Speak now — 3 second window <<<")
        text = record_and_transcribe(args.ip, model)

        if not text:
            print("[STT]  (no transcript)")
            return

        print(f'\n[STT]  Transcript: "{text}"')

        wakes = count_wake_word(text)
        if wakes:
            print(f'[STT]  Wake word "help" found: {wakes}x  ⚠')
        else:
            print('[STT]  No wake word detected')

        if not args.no_send:
            ack = send_stt_to_esp(args.ip, text)
            if ack:
                print(f"[ESP]  help_found={ack.get('help_found',0)}  "
                      f"total={ack.get('wake_word_total',0)}/{ack.get('threshold',3)}  "
                      f"triggered={ack.get('triggered',False)}")

    if args.loop:
        print("Looping — press Ctrl-C to stop\n")
        try:
            while True:
                run_once()
                time.sleep(0.5)   # tiny gap between recordings
        except KeyboardInterrupt:
            print("\n[DONE] Stopped.")
    else:
        run_once()

if __name__ == "__main__":
    main()