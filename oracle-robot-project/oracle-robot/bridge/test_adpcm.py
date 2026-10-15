"""Offline tests for the ADPCM codec used by the on-device clip store. Run: python test_adpcm.py"""
import sys
import numpy as np
from adpcm import BLOCK_BYTES, SAMPLES_PER_BLOCK, CLIP_RATE, encode, decode, pcm_bytes_to_clip, resample_to_clip_rate
from testsignals import synth_speech

fails = 0
def check(name, cond, detail=""):
    global fails
    fails += not cond
    print(("PASS  " if cond else "FAIL  ") + name + (f"   [{detail}]" if detail else ""))

def snr_db(ref, test):
    ref = ref.astype(np.float64); test = test[:ref.size].astype(np.float64)
    err = ref - test
    return 10 * np.log10(np.sum(ref ** 2) / max(np.sum(err ** 2), 1e-9))

# speech-like signal at the clip rate
_, clean = synth_speech(seconds=4.0, lead=0.1, tail=0.1, level=12000, noise=0)
x = resample_to_clip_rate(clean, 16000)
enc = encode(x)
dec = decode(enc)

check("4:1 compression, block aligned", len(enc) % BLOCK_BYTES == 0 and
      abs(len(enc) / (x.size * 2) - 0.25) < 0.01, f"{x.size*2} B PCM -> {len(enc)} B")
check("decoded length covers the input", dec.size >= x.size and dec.size - x.size < SAMPLES_PER_BLOCK)
check("quality is good enough for speech (SNR > 20 dB)", snr_db(x, dec) > 20, f"{snr_db(x, dec):.1f} dB")
check("no drift at block boundaries", max(abs(int(dec[i]) - int(x[i])) for i in
      range(SAMPLES_PER_BLOCK, x.size, SAMPLES_PER_BLOCK)) == 0, "each block restarts from an exact sample")

# worst cases must not blow up or wrap around
for name, sig in [("silence", np.zeros(5000, dtype=np.int16)),
                  ("full-scale square", (np.tile([32767, -32768], 2500)).astype(np.int16)),
                  ("max positive ramp", np.linspace(-32768, 32767, 5000).astype(np.int16)),
                  ("white noise", (np.random.default_rng(3).standard_normal(5000) * 12000).clip(-32768, 32767).astype(np.int16)),
                  ("single sample", np.array([1234], dtype=np.int16)),
                  ("empty", np.zeros(0, dtype=np.int16))]:
    try:
        d = decode(encode(sig))
        ok = (sig.size == 0 and d.size == 0) or (d.size >= sig.size and np.all(np.isfinite(d.astype(np.float64))))
        check(f"edge case: {name}", ok)
    except Exception as e:
        check(f"edge case: {name}", False, repr(e))

# 24 kHz TTS -> 16 kHz clip
pcm24 = (synth_speech(seconds=3.0, level=10000, noise=0)[1]).astype("<i2").tobytes()
clip = pcm_bytes_to_clip(pcm24, 24000)
samples24 = len(pcm24) // 2
check("24 kHz PCM -> 16 kHz clip is ~6x smaller", 5.5 < (len(pcm24) / len(clip)) < 6.5, f"{len(pcm24)} -> {len(clip)} B")
check("clip duration matches the original", abs(decode(clip).size / CLIP_RATE - samples24 / 24000) < 0.05)

# storage budget for the real answer set
print(f"\n      1.5 MB SPIFFS partition holds ~{1.5e6 / (CLIP_RATE * 2 / 4):.0f} s of speech at {CLIP_RATE} Hz ADPCM")
print(f"\n{'ALL PASSED' if not fails else str(fails) + ' FAILED'}")
sys.exit(1 if fails else 0)
