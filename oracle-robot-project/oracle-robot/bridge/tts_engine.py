"""
Local, offline text-to-speech for the ORACLE bridge.

    synth(text) -> (int16 mono PCM bytes, sample_rate)      at the engine's native rate
    resample(pcm, src_rate, dst_rate) -> int16 PCM bytes    e.g. 24 kHz for streaming, adpcm.CLIP_RATE for clips

Engines
    piper    primary. Neural, fast, natural.  Needs a voice model (.onnx + .onnx.json), path in PIPER_VOICE.
             Uses the `piper-tts` Python API, or the `piper` binary (--output_raw) if the package is missing.
    pyttsx3  fallback. Windows SAPI / macOS NSSpeechSynthesizer / espeak: works with no downloads.

TTS_ENGINE=piper|pyttsx3 picks the engine (default piper). If it is not usable the other one is chosen and a
warning is logged.  The engine is chosen once, by availability (resolve()), so cache keys and clip ids always
describe the engine that really produced the audio.  A failure while synthesising raises TTSError; the caller
decides what to say instead.
"""
from __future__ import annotations

import logging
import os
import shutil
import subprocess
import tempfile
import threading
import wave
from functools import lru_cache
from math import gcd
from pathlib import Path

import numpy as np
from scipy import signal

log = logging.getLogger("oracle.tts")
HERE = Path(__file__).resolve().parent


class TTSError(RuntimeError):
    """Speech synthesis failed (engine missing, voice missing, engine crashed, empty audio)."""


def _cfg(name: str, default: str = "") -> str:
    return os.getenv(name, default).strip()


# ----------------------------------------------------------------------------- resampling
def resample(pcm: bytes, src_rate: int, dst_rate: int) -> bytes:
    """int16 mono PCM bytes -> int16 mono PCM bytes at dst_rate (polyphase; clipped, never wraps)."""
    x = np.frombuffer(pcm[: len(pcm) // 2 * 2], dtype="<i2")
    if src_rate == dst_rate or x.size == 0:
        return x.tobytes()
    g = gcd(src_rate, dst_rate)
    y = signal.resample_poly(x.astype(np.float64), dst_rate // g, src_rate // g)
    return np.clip(np.rint(y), -32768, 32767).astype("<i2").tobytes()


HEADROOM_PEAK = 29000        # ~ -1 dB: engines (Piper peak-normalises to full scale) would clip when resampled


def _with_headroom(pcm: bytes) -> bytes:
    x = np.frombuffer(pcm, dtype="<i2")
    peak = int(np.abs(x.astype(np.int32)).max()) if x.size else 0
    if peak <= HEADROOM_PEAK:
        return pcm
    return np.rint(x.astype(np.float64) * (HEADROOM_PEAK / peak)).astype("<i2").tobytes()


# ----------------------------------------------------------------------------- Piper
def _piper_voice_path() -> Path | None:
    raw = _cfg("PIPER_VOICE")
    if not raw:
        return None
    p = Path(raw).expanduser()
    if not p.is_absolute():
        for base in (Path.cwd(), HERE):                   # relative to where you run it, or to bridge/
            if (base / p).is_file():
                return base / p
    return p if p.is_file() else None


def _piper_package_ok() -> bool:
    try:
        import piper  # noqa: F401
        return True
    except ImportError:
        return False


def piper_available() -> tuple[bool, str]:
    voice = _piper_voice_path()
    if voice is None:
        return False, f"PIPER_VOICE={_cfg('PIPER_VOICE')!r} is not set or the .onnx file does not exist"
    if not Path(str(voice) + ".json").is_file():
        return False, f"missing {voice.name}.json next to the voice (download both .onnx and .onnx.json)"
    if not _piper_package_ok() and shutil.which("piper") is None:
        return False, "neither the piper-tts package nor a `piper` binary is installed"
    return True, ""


_piper_lock = threading.Lock()


@lru_cache(maxsize=1)
def _piper_load(path: str):
    from piper import PiperVoice
    log.info("loading Piper voice %s", Path(path).name)
    return PiperVoice.load(path)


def _synth_piper(text: str) -> tuple[bytes, int]:
    ok, why = piper_available()
    if not ok:
        raise TTSError(f"Piper unavailable: {why}")
    voice = _piper_voice_path()
    if _piper_package_ok():
        with _piper_lock:
            v = _piper_load(str(voice))
            chunks = list(v.synthesize(text))
        if not chunks:
            raise TTSError("Piper produced no audio")
        return b"".join(c.audio_int16_bytes for c in chunks), int(chunks[0].sample_rate)
    # fallback: the piper binary, raw int16 on stdout; the sample rate is in the voice's .json
    import json
    rate = int(json.loads(Path(str(voice) + ".json").read_text(encoding="utf-8"))["audio"]["sample_rate"])
    try:
        r = subprocess.run(["piper", "--model", str(voice), "--output_raw"], input=text.encode("utf-8"),
                           capture_output=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as e:
        raise TTSError(f"piper binary failed: {type(e).__name__}: {e}") from e
    if r.returncode != 0 or not r.stdout:
        raise TTSError(f"piper binary exited {r.returncode}: {r.stderr.decode(errors='replace')[-200:]}")
    return r.stdout, rate


# ----------------------------------------------------------------------------- pyttsx3
def pyttsx3_available() -> tuple[bool, str]:
    try:
        import pyttsx3  # noqa: F401
        return True, ""
    except ImportError:
        return False, "pyttsx3 is not installed (pip install pyttsx3)"
    except Exception as e:                                # some platforms fail while importing the driver
        return False, f"pyttsx3 unusable ({type(e).__name__}: {e})"


_sapi_lock = threading.Lock()


def _synth_pyttsx3(text: str) -> tuple[bytes, int]:
    import pyttsx3
    fd, name = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        with _sapi_lock:                                  # the OS speech engines are not re-entrant
            eng = pyttsx3.init()
            try:
                eng.save_to_file(text, name)
                eng.runAndWait()
            finally:
                try:
                    eng.stop()
                except Exception:
                    pass
        with wave.open(name, "rb") as w:
            if w.getsampwidth() != 2:
                raise TTSError(f"pyttsx3 wrote {8 * w.getsampwidth()}-bit audio, expected 16-bit")
            rate, ch = w.getframerate(), w.getnchannels()
            pcm = w.readframes(w.getnframes())
        if ch > 1:                                        # keep the first channel only
            pcm = np.frombuffer(pcm, dtype="<i2").reshape(-1, ch)[:, 0].tobytes()
        return pcm, rate
    except TTSError:
        raise
    except Exception as e:                                # wave.Error, EOFError, driver errors ...
        raise TTSError(f"pyttsx3 failed: {type(e).__name__}: {e}") from e
    finally:
        try:
            os.unlink(name)
        except OSError:
            pass


# ----------------------------------------------------------------------------- engine choice + entry point
_resolved: tuple[str, str] | None = None
_resolved_key: tuple[str, str] | None = None


def resolve() -> tuple[str, str]:
    """(engine name, voice name) that synth() will use. Cheap; result is remembered until the settings change."""
    global _resolved, _resolved_key
    key = (_cfg("TTS_ENGINE", "piper").lower(), _cfg("PIPER_VOICE"))
    if _resolved is not None and _resolved_key == key:
        return _resolved
    want = key[0]
    if want not in ("piper", "pyttsx3"):
        log.warning("unknown TTS_ENGINE=%r - using piper", want)
        want = "piper"
    checks = {"piper": piper_available, "pyttsx3": pyttsx3_available}
    ok, why = checks[want]()
    engine = want
    if not ok:
        other = "pyttsx3" if want == "piper" else "piper"
        ok2, why2 = checks[other]()
        log.warning("TTS engine %s unavailable (%s)%s", want, why,
                    f" - falling back to {other}" if ok2 else "")
        if ok2:
            engine = other
        else:
            log.error("no usable TTS engine: %s: %s; %s: %s", want, why, other, why2)
            engine = want                                  # synth() will raise TTSError with the reason
    if engine == "piper":
        v = _piper_voice_path()
        voice = v.stem if v else "missing"
    else:
        voice = "system-default"
    _resolved, _resolved_key = (engine, voice), key
    return _resolved


def synth(text: str) -> tuple[bytes, int]:
    """Speak `text`. Returns (int16 mono PCM bytes, sample rate). Raises TTSError on any failure."""
    text = (text or "").strip()
    if not text:
        raise TTSError("nothing to say")
    engine, _ = resolve()
    try:
        pcm, rate = _synth_piper(text) if engine == "piper" else _synth_pyttsx3(text)
    except TTSError:
        raise
    except Exception as e:
        raise TTSError(f"{engine} failed: {type(e).__name__}: {e}") from e
    pcm = pcm[: len(pcm) // 2 * 2]
    if not pcm or rate <= 0:
        raise TTSError(f"{engine} returned no audio")
    return _with_headroom(pcm), int(rate)
