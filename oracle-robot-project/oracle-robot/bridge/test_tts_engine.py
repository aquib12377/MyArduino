"""Offline tests for the local TTS layer (tts_engine.py and bridge.tts). Run: python test_tts_engine.py
The real-synthesis tests are skipped, with a message, if no Piper voice / system voice is installed."""
import asyncio
import logging
import os
import shutil
import sys
import tempfile
from pathlib import Path

import numpy as np

import bridge                      # loads .env, so PIPER_VOICE from there is visible below
import tts_engine
import adpcm
from clips import clip_id
from tts_engine import TTSError

fails = skipped = 0
def check(name, cond, detail=""):
    global fails
    fails += not cond
    print(("PASS  " if cond else "FAIL  ") + name + (f"   [{detail}]" if detail else ""))

def skip(name, why):
    global skipped
    skipped += 1
    print(f"SKIP  {name}   [{why}]")

HERE = Path(__file__).resolve().parent
ENV_VOICE = os.environ.get("PIPER_VOICE", "")
ENV_ENGINE = os.environ.get("TTS_ENGINE", "")

def sine(freq, seconds, rate, amp):
    t = np.arange(int(seconds * rate)) / rate
    return np.rint(amp * np.sin(2 * np.pi * freq * t)).astype("<i2")

def use(engine, voice=""):
    os.environ["TTS_ENGINE"], os.environ["PIPER_VOICE"] = engine, voice

# ---- resampling: sample counts, dtype, no clipping ---------------------------------------------------------------
for src in (22050, 16000, 24000, 44100):
    x = sine(440, 1.0, src, 12000)
    for dst in (bridge.TTS_RATE, adpcm.CLIP_RATE):
        y = np.frombuffer(tts_engine.resample(x.tobytes(), src, dst), dtype="<i2")
        want = round(x.size * dst / src)
        check(f"resample {src} -> {dst}: sample count", abs(y.size - want) <= 1, f"{y.size} vs {want}")
check("streaming rate is 24 kHz, clip rate is 16 kHz", bridge.TTS_RATE == 24000 and adpcm.CLIP_RATE == 16000)

loud = sine(300, 0.5, 22050, 32767)                                # full-scale sine: resampling overshoots
y = np.frombuffer(tts_engine.resample(loud.tobytes(), 22050, 24000), dtype="<i2")
check("full-scale input: no wrap-around (clipped, not wrapped)",
      y.dtype == np.int16 and np.corrcoef(y[2000:-2000].astype(float), sine(300, 0.5, 24000, 32767)[2000:-2000].astype(float))[0, 1] > 0.99)
check("full-scale input: output stays inside int16", int(y.max()) <= 32767 and int(y.min()) >= -32768)
sq = np.where(np.arange(8000) // 40 % 2 == 0, 32767, -32768).astype("<i2")
y = np.frombuffer(tts_engine.resample(sq.tobytes(), 22050, 16000), dtype="<i2")
check("full-scale square: no wrap-around", np.abs(y.astype(np.int64)).max() <= 32768 and y[100:-100].std() > 20000)
check("resample keeps level (rms within 1 dB)",
      abs(20 * np.log10(np.frombuffer(tts_engine.resample(sine(440, 1, 22050, 8000).tobytes(), 22050, 24000), "<i2").astype(float).std()
                        / sine(440, 1, 22050, 8000).astype(float).std())) < 1.0)
check("same rate is a no-op", tts_engine.resample(loud.tobytes(), 22050, 22050) == loud.tobytes())
check("odd trailing byte is dropped, empty input is fine",
      len(tts_engine.resample(loud.tobytes() + b"\x01", 22050, 24000)) % 2 == 0 and tts_engine.resample(b"", 22050, 24000) == b"")

# ---- engine selection and clean failure ----------------------------------------------------------------------------
real_pyttsx3_check = tts_engine.pyttsx3_available
logging.getLogger("oracle.tts").setLevel(logging.CRITICAL)          # the warnings below are expected

use("piper", "/no/such/voice.onnx")
tts_engine.pyttsx3_available = lambda: (False, "test: not installed")
try:
    tts_engine.synth("hello")
    check("no usable engine raises TTSError", False, "returned audio")
except TTSError as e:
    check("no usable engine raises TTSError (with the reason)", "voice" in str(e).lower() or "piper" in str(e).lower(), str(e)[:90])
tts_engine.pyttsx3_available = lambda: (True, "")
tts_engine._resolved = None                                         # resolve() remembers its choice; force a fresh look
check("piper unavailable -> falls back to pyttsx3", tts_engine.resolve()[0] == "pyttsx3", str(tts_engine.resolve()))
tts_engine.pyttsx3_available = real_pyttsx3_check
tts_engine._resolved = None

for bad in ("", "   "):
    try:
        tts_engine.synth(bad)
        check("empty text raises TTSError", False)
    except TTSError:
        check(f"empty text {bad!r} raises TTSError", True)

# a voice file that exists but is garbage -> TTSError, not a crash of the caller
with tempfile.TemporaryDirectory() as d:
    (Path(d) / "junk.onnx").write_bytes(b"not a model")
    (Path(d) / "junk.onnx.json").write_text("{}")
    use("piper", str(Path(d) / "junk.onnx"))
    tts_engine.pyttsx3_available = lambda: (False, "test")
    tts_engine._piper_load.cache_clear()
    try:
        tts_engine.synth("hello")
        check("corrupt Piper voice raises TTSError", False)
    except TTSError as e:
        check("corrupt Piper voice raises TTSError", True, str(e)[:70])
    tts_engine.pyttsx3_available = real_pyttsx3_check
    tts_engine._piper_load.cache_clear()

# ---- real synthesis (skipped if not installed) ---------------------------------------------------------------------
voice = ENV_VOICE or next((str(p) for p in sorted((HERE / "voices").glob("*.onnx"))), "")
use("piper", voice)
ok, why = tts_engine.piper_available()
if ok:
    pcm, rate = tts_engine.synth("Hello, I am Oracle. Welcome to Equinox.")
    a = np.frombuffer(pcm, dtype="<i2")
    check("Piper: int16 mono PCM with a sane sample rate", len(pcm) % 2 == 0 and 8000 <= rate <= 48000, f"{rate} Hz, {a.size / rate:.2f}s")
    check("Piper: audible, with headroom (no full-scale samples)", 500 < int(np.abs(a).max()) < 32000 and a.astype(float).std() > 200,
          f"peak {int(np.abs(a).max())}")
    check("Piper: engine/voice reported for the cache key", tts_engine.resolve() == ("piper", Path(voice).stem), str(tts_engine.resolve()))
    for dst in (24000, 16000):
        y = np.frombuffer(tts_engine.resample(pcm, rate, dst), dtype="<i2")
        check(f"Piper -> {dst} Hz: duration preserved, no clipping",
              abs(y.size / dst - a.size / rate) < 0.01 and int(np.abs(y.astype(np.int64)).max()) < 32767,
              f"{y.size / dst:.2f}s, peak {int(np.abs(y.astype(np.int64)).max())}")
    check("Piper: a longer sentence is longer audio",
          len(tts_engine.synth("Hello, I am Oracle, a walking robot at Equinox twenty twenty six.")[0]) > len(pcm))
else:
    skip("real Piper synthesis", f"no Piper voice installed: {why}. Download a voice, set PIPER_VOICE (see .env.example)")

use("pyttsx3")
ok, why = tts_engine.pyttsx3_available()
if ok:
    try:
        pcm, rate = tts_engine.synth("Hello from the system voice.")
        a = np.frombuffer(pcm, dtype="<i2")
        check("pyttsx3: int16 mono PCM", len(pcm) % 2 == 0 and a.size > rate * 0.3 and rate > 0, f"{rate} Hz, {a.size / rate:.2f}s")
    except TTSError as e:
        skip("real pyttsx3 synthesis", f"no working system voice on this machine: {e}")
else:
    skip("real pyttsx3 synthesis", why)

# ---- bridge.tts(): cache key, engine failure -> cached apology -------------------------------------------------------
tmp = Path(tempfile.mkdtemp(prefix="oracle_tts_"))
bridge.CACHE_DIR = tmp
calls = []
def fake_synth(text):
    calls.append(text)
    if text.startswith("BOOM"):
        raise TTSError("engine crashed")
    return sine(200, 0.5, 22050, 9000).tobytes(), 22050
real_synth, tts_engine.synth = tts_engine.synth, fake_synth

pcm = asyncio.run(bridge.tts("Some new answer", cache=True))
a = np.frombuffer(pcm, dtype="<i2")
check("bridge.tts resamples the engine rate to the 24 kHz stream rate", abs(a.size - 12000) <= 1, f"{a.size} samples")
asyncio.run(bridge.tts("Some new answer", cache=True))
check("cache hit: engine not called again", calls == ["Some new answer"], str(calls))
asyncio.run(bridge.tts("Not cached", cache=False))
asyncio.run(bridge.tts("Not cached", cache=False))
check("cache=False is never written to disk", calls.count("Not cached") == 2)

old_voice = bridge.TTS_VOICE
bridge.TTS_VOICE = "another-voice"
asyncio.run(bridge.tts("Some new answer", cache=True))
check("changing the voice invalidates the cache", calls.count("Some new answer") == 2)
bridge.TTS_VOICE, old_engine = old_voice, bridge.TTS_ENGINE
bridge.TTS_ENGINE = "pyttsx3" if old_engine == "piper" else "piper"
asyncio.run(bridge.tts("Some new answer", cache=True))
check("changing the engine invalidates the cache", calls.count("Some new answer") == 3)
bridge.TTS_ENGINE = old_engine

ids = {clip_id("Same text", e, v) for e, v in
       (("piper", "v1"), ("piper", "v2"), ("pyttsx3", "v1"))}
check("clip ids differ per engine and per voice", len(ids) == 3)
check("the ClipStore is built from the engine and voice", bridge.CLIPS.model == bridge.TTS_ENGINE and bridge.CLIPS.voice == bridge.TTS_VOICE,
      f"{bridge.CLIPS.model}/{bridge.CLIPS.voice}")

try:
    asyncio.run(bridge.tts("BOOM please", cache=True))
    check("engine failure propagates out of tts() as an exception", False)
except TTSError:
    check("engine failure propagates out of tts() as TTSError", True)
check("a failed synthesis writes nothing to the cache", len(list(tmp.glob("*.pcm"))) == 3,
      f"{len(list(tmp.glob('*.pcm')))} cached files (3 good renders, 0 from BOOM)")


class Stub(bridge.Robot):
    def __init__(self):
        self.onboard, self.played, self.states = set(), [], []
    async def state(self, n): self.states.append(n)
    async def play(self, pcm): self.played.append(len(pcm)); return True

async def speak_flow(apology_works):
    global calls
    calls = []
    r = Stub()
    if not apology_works:
        tts_engine.synth = lambda t: (_ for _ in ()).throw(TTSError("engine dead"))
    # the offline apology has been rendered before (prebuild): put it in the disk cache
    if apology_works:
        await bridge.tts(bridge.KB.fallback_offline, cache=True)
        tts_engine.synth = lambda t: (_ for _ in ()).throw(TTSError("engine dead"))
    await r.speak("BOOM a brand new answer")
    return r

r = asyncio.run(speak_flow(True))
check("engine failure on a new answer -> the cached apology is spoken", len(r.played) == 1 and "confused" in r.states, f"played={r.played} states={r.states}")
shutil.rmtree(tmp); tmp.mkdir()
r = asyncio.run(speak_flow(False))
check("engine dead and apology not cached -> error state, nothing hangs", r.played == [] and "error" in r.states, f"states={r.states}")

tts_engine.synth = real_synth
shutil.rmtree(tmp, ignore_errors=True)
os.environ["TTS_ENGINE"], os.environ["PIPER_VOICE"] = ENV_ENGINE, ENV_VOICE

print(f"\n{'ALL PASSED' if not fails else str(fails) + ' FAILED'}" + (f"  ({skipped} skipped)" if skipped else ""))
sys.exit(1 if fails else 0)
