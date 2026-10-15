"""Synthetic 'speech-like' test audio (voiced harmonics with syllable-rate modulation) + background noise."""
import numpy as np

RATE = 16000


def synth_speech(seconds=2.0, lead=0.5, tail=0.4, level=2500.0, noise=300.0, hum=0.0, seed=1):
    """Return (noisy int16 bytes, clean int16 array). Silence for `lead` s, speech, silence for `tail` s."""
    rng = np.random.default_rng(seed)
    n = int(RATE * seconds)
    t = np.arange(n) / RATE
    f0 = 140 + 30 * np.sin(2 * np.pi * 0.7 * t)
    phase = 2 * np.pi * np.cumsum(f0) / RATE
    voiced = sum(np.sin(k * phase) / k for k in range(1, 12))
    syll = 0.5 * (1 + np.sin(2 * np.pi * 3.5 * t))             # ~3.5 syllables per second
    speech = voiced * syll
    speech = speech / np.max(np.abs(speech)) * level
    env = np.zeros(n)
    env[int(lead * RATE): n - int(tail * RATE)] = 1.0
    clean = speech * env
    bg = rng.standard_normal(n) * noise                        # crowd/fan-like broadband noise
    bg = np.convolve(bg, np.ones(4) / 4, mode="same")          # a bit low-pass like a real hall
    mains = hum * np.sin(2 * np.pi * 50 * t)
    noisy = np.clip(clean + bg + mains, -32768, 32767)
    return noisy.astype("<i2").tobytes(), clean.astype("<i2")
