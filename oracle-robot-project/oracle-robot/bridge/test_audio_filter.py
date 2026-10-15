"""Offline tests for the noise filter (no API keys, no hardware).  Run: python test_audio_filter.py"""
import sys
import numpy as np
from audio_filter import clean_audio, highpass, RATE
from testsignals import synth_speech

fails = 0


def check(name, cond, detail=""):
    global fails
    fails += not cond
    print(("PASS  " if cond else "FAIL  ") + name + (f"   [{detail}]" if detail else ""))


def rms(a): return float(np.sqrt(np.mean(np.asarray(a, dtype=np.float64) ** 2)))


# 1. mains hum / rumble below 100 Hz is removed
t = np.arange(RATE * 2) / RATE
hum = (np.sin(2 * np.pi * 50 * t) * 5000)
check("high-pass kills 50 Hz hum", rms(highpass(hum)) < 0.05 * rms(hum), f"{rms(hum):.0f} -> {rms(highpass(hum)):.0f}")

# 2. steady background noise is reduced a lot, speech is kept
noisy, clean = synth_speech(seconds=2.5, lead=0.7, tail=0.6, level=2500, noise=400, hum=800)
res = clean_audio(noisy)
out = np.frombuffer(res.pcm, dtype="<i2").astype(np.float64)
check("output is shorter than input (silence trimmed)", res.seconds < 2.5, f"{res.seconds:.2f}s")
in_noise = rms(np.frombuffer(noisy, dtype="<i2")[: int(0.5 * RATE)])
# measure residual noise on a noise-only stretch by filtering a noise-only clip with the same statistics
noise_only, _ = synth_speech(seconds=2.5, lead=10, tail=0, level=2500, noise=400, hum=800)   # lead>duration => no speech
res_noise = clean_audio(noise_only)
check("noise-only clip is suppressed (>12 dB)", res_noise.filt_rms < 0.25 * res_noise.raw_rms,
      f"raw {res_noise.raw_rms:.0f} -> filtered {res_noise.filt_rms:.0f}")
check("speech clip keeps most of its energy", res.filt_rms > 0.45 * res.raw_rms,
      f"raw {res.raw_rms:.0f} -> filtered {res.filt_rms:.0f}")
check("speech-to-noise separation improved", (res.filt_rms / max(res_noise.filt_rms, 1e-6)) > 2 * (res.raw_rms / res_noise.raw_rms),
      f"before {res.raw_rms/res_noise.raw_rms:.1f}x  after {res.filt_rms/max(res_noise.filt_rms,1e-6):.1f}x")
check("output is normalised to a healthy level", 8000 < np.max(np.abs(out)) < 32768, f"peak {np.max(np.abs(out)):.0f}")

# 3. very quiet INMP441-style speech is lifted
quiet, _ = synth_speech(seconds=2.0, level=250, noise=20)
q = clean_audio(quiet)
check("quiet speech is amplified", np.max(np.abs(np.frombuffer(q.pcm, dtype="<i2"))) > 3000)

# 4. edge cases must not crash
for name, pcm in [("empty", b""), ("1 byte", b"\x01"), ("50 ms", b"\x10\x00" * 800), ("all zeros 3 s", b"\x00\x00" * 48000),
                  ("full-scale square", (b"\xff\x7f" * 200 + b"\x00\x80" * 200) * 100)]:
    try:
        clean_audio(pcm)
        check(f"edge case: {name}", True)
    except Exception as e:
        check(f"edge case: {name}", False, repr(e))

# 5. disabled filter only normalises
raw = clean_audio(noisy, enable_filter=False)
check("filter can be switched off", abs(raw.seconds - 2.5) < 0.01)
print(f"\n{'ALL PASSED' if not fails else str(fails) + ' FAILED'}")
sys.exit(1 if fails else 0)
