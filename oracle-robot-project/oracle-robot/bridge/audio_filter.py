"""
Noise filtering for the raw microphone audio that the ESP32 streams to the laptop.

Chain (all on the laptop, the ESP32 only removes DC offset):
  1. high-pass 100 Hz          - rumble, motor thump, handling noise, mains hum below 100 Hz
  2. spectral gate             - estimates the steady background (crowd murmur, fans, hall reverb) from the
                                 quietest parts of the clip itself and subtracts it, with smoothing so it does not
                                 leave "musical noise" or chew up consonants
  3. trim leading/trailing silence (keeps 200 ms padding)
  4. level normalisation       - the INMP441 is quiet; lift speech to a healthy level for speech-to-text

Pure numpy/scipy, no network.  Toggle with NOISE_FILTER=0 in .env if a venue's noise makes it counter-productive
(over-aggressive denoising can hurt recognition just as much as noise does - always A/B it on the real hall).
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy import ndimage, signal

RATE = 16000
NPERSEG, NOVERLAP = 512, 384          # 32 ms window, 8 ms hop


@dataclass
class Cleaned:
    pcm: bytes           # int16 little-endian, 16 kHz, cleaned + normalised (empty if nothing usable)
    raw_rms: float       # RMS of what the mic delivered (int16 units)
    filt_rms: float      # RMS after filtering, before normalisation
    seconds: float       # duration after trimming


def _rms(x: np.ndarray) -> float:
    return float(np.sqrt(np.mean(x * x))) if x.size else 0.0


def highpass(x: np.ndarray, fc: float = 100.0) -> np.ndarray:
    if x.size < 200:
        return x
    sos = signal.butter(4, fc, btype="highpass", fs=RATE, output="sos")
    return signal.sosfiltfilt(sos, x)


def spectral_gate(x: np.ndarray, alpha: float = 1.5, floor: float = 0.12, noise_pct: float = 10.0) -> np.ndarray:
    """Spectral subtraction against a per-frequency noise floor taken from the clip's own quiet frames."""
    if x.size < RATE // 2:                                  # under 0.5 s there is nothing to learn a floor from
        return x
    _, _, z = signal.stft(x, fs=RATE, nperseg=NPERSEG, noverlap=NOVERLAP, boundary="zeros", padded=True)
    mag = np.abs(z)
    noise = np.percentile(mag, noise_pct, axis=1, keepdims=True)
    gain = np.clip((mag - alpha * noise) / np.maximum(mag, 1e-9), floor, 1.0)
    gain = ndimage.uniform_filter(gain, size=(3, 5), mode="nearest")       # smooth over frequency and time
    _, y = signal.istft(z * gain, fs=RATE, nperseg=NPERSEG, noverlap=NOVERLAP, boundary=True)
    y = y[: x.size]
    return np.pad(y, (0, x.size - y.size)) if y.size < x.size else y


def trim_silence(x: np.ndarray, pad_ms: int = 200) -> np.ndarray:
    frame = RATE // 50                                      # 20 ms
    n = x.size // frame
    if n < 5:
        return x
    e = np.sqrt(np.mean(x[: n * frame].reshape(n, frame) ** 2, axis=1))
    thr = max(np.percentile(e, 10) * 3.0, 0.05 * e.max())
    idx = np.where(e > thr)[0]
    if idx.size == 0:
        return x
    pad = pad_ms * RATE // 1000
    return x[max(0, idx[0] * frame - pad): min(x.size, (idx[-1] + 1) * frame + pad)]


def normalise(x: np.ndarray, target_peak: float = 0.6 * 32767, max_gain: float = 30.0) -> np.ndarray:
    peak = float(np.max(np.abs(x))) if x.size else 0.0
    if peak < 1.0:
        return x
    return np.clip(x * float(np.clip(target_peak / peak, 0.5, max_gain)), -32768, 32767)


def clean_audio(pcm: bytes, enable_filter: bool = True) -> Cleaned:
    x = np.frombuffer(pcm[: len(pcm) // 2 * 2], dtype="<i2").astype(np.float64)
    if x.size == 0:
        return Cleaned(b"", 0.0, 0.0, 0.0)
    raw_rms = _rms(x)
    y = x
    if enable_filter:
        y = highpass(y)
        y = spectral_gate(y)
        y = trim_silence(y)
    filt_rms = _rms(y)
    y = normalise(y)
    return Cleaned(y.astype("<i2").tobytes(), raw_rms, filt_rms, y.size / RATE)
