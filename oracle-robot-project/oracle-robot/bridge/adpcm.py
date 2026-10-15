"""
IMA ADPCM 4:1 codec for the on-device clip store.

Why: raw PCM at 24 kHz is 48 KB/s, so a 10 s answer is ~480 KB to push over Wi-Fi before the robot
can speak. The same answer as 16 kHz IMA ADPCM is ~80 KB, and once it is stored in the ESP32's flash
it costs zero network time at all.

Format (identical to the block layout used by WAV IMA ADPCM, mono):
    block = 256 bytes = 4-byte header + 252 data bytes
    header: int16 LE predictor, uint8 step index, uint8 reserved(0)
    data  : 504 nibbles, low nibble first = 504 samples
    -> 505 samples per block, the last block is zero-padded
The decoder in oracle_esp32s3.ino implements exactly this; test_adpcm.py checks the two agree
sample-for-sample, so a clip encoded here cannot drift from what the robot plays.
"""
from __future__ import annotations

import numpy as np
from scipy import signal

BLOCK_BYTES = 256
SAMPLES_PER_BLOCK = 505
CLIP_RATE = 16000                      # speech at 16 kHz is clear and halves the storage vs 24 kHz

STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73,
    80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494,
    544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499,
    2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
]
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def _clamp(v: int, lo: int, hi: int) -> int:
    return lo if v < lo else (hi if v > hi else v)


def _encode_sample(sample: int, pred: int, index: int) -> tuple[int, int, int]:
    step = STEP_TABLE[index]
    diff = sample - pred
    code = 0
    if diff < 0:
        code = 8
        diff = -diff
    delta = step >> 3
    for bit in (4, 2, 1):
        t = step if bit == 4 else (step >> 1 if bit == 2 else step >> 2)
        if diff >= t:
            code |= bit
            diff -= t
            delta += t
    pred = _clamp(pred + (-delta if code & 8 else delta), -32768, 32767)
    index = _clamp(index + INDEX_TABLE[code], 0, 88)
    return code, pred, index


def _decode_sample(code: int, pred: int, index: int) -> tuple[int, int, int]:
    step = STEP_TABLE[index]
    delta = step >> 3
    if code & 4:
        delta += step
    if code & 2:
        delta += step >> 1
    if code & 1:
        delta += step >> 2
    pred = _clamp(pred - delta if code & 8 else pred + delta, -32768, 32767)
    index = _clamp(index + INDEX_TABLE[code], 0, 88)
    return pred, index


def encode(pcm16: np.ndarray) -> bytes:
    """int16 mono samples -> block-aligned IMA ADPCM bytes."""
    x = np.asarray(pcm16, dtype=np.int16)
    pad = (-x.size) % SAMPLES_PER_BLOCK
    if pad:
        x = np.concatenate([x, np.zeros(pad, dtype=np.int16)])
    out = bytearray()
    pred, index = 0, 0
    for b in range(x.size // SAMPLES_PER_BLOCK):
        blk = x[b * SAMPLES_PER_BLOCK: (b + 1) * SAMPLES_PER_BLOCK]
        pred, index = int(blk[0]), _clamp(index, 0, 88)        # each block restarts from a real sample: no drift
        out += int(pred).to_bytes(2, "little", signed=True) + bytes([index, 0])
        nib, data = [], bytearray()
        for s in blk[1:]:
            code, pred, index = _encode_sample(int(s), pred, index)
            nib.append(code)
        for i in range(0, len(nib), 2):
            data.append((nib[i] & 0x0F) | ((nib[i + 1] & 0x0F) << 4))
        assert len(data) == BLOCK_BYTES - 4
        out += data
    return bytes(out)


def decode(data: bytes) -> np.ndarray:
    """IMA ADPCM bytes -> int16 mono samples (reference implementation of the firmware decoder)."""
    out = np.zeros((len(data) // BLOCK_BYTES) * SAMPLES_PER_BLOCK, dtype=np.int16)
    o = 0
    for b in range(len(data) // BLOCK_BYTES):
        blk = data[b * BLOCK_BYTES: (b + 1) * BLOCK_BYTES]
        pred = int.from_bytes(blk[0:2], "little", signed=True)
        index = _clamp(blk[2], 0, 88)
        out[o] = pred
        o += 1
        for byte in blk[4:]:
            for code in (byte & 0x0F, byte >> 4):
                pred, index = _decode_sample(code, pred, index)
                out[o] = pred
                o += 1
    return out


def resample_to_clip_rate(pcm16: np.ndarray, src_rate: int) -> np.ndarray:
    """24 kHz TTS output -> 16 kHz (anti-aliased polyphase resampling)."""
    if src_rate == CLIP_RATE:
        return np.asarray(pcm16, dtype=np.int16)
    from math import gcd
    g = gcd(src_rate, CLIP_RATE)
    y = signal.resample_poly(np.asarray(pcm16, dtype=np.float64), CLIP_RATE // g, src_rate // g)
    return np.clip(y, -32768, 32767).astype(np.int16)


def pcm_bytes_to_clip(pcm: bytes, src_rate: int) -> bytes:
    x = np.frombuffer(pcm[: len(pcm) // 2 * 2], dtype="<i2")
    return encode(resample_to_clip_rate(x, src_rate))
