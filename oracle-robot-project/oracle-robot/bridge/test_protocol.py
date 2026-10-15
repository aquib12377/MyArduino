"""
Protocol test: a fake ESP32 talks to the real bridge code over a real WebSocket.
Speech-to-text / text-to-speech / LLM are stubbed, so no API keys or hardware are needed.
Run:  python test_protocol.py
"""
import asyncio
import numpy as np
import websockets
from websockets.asyncio.server import serve

import bridge
from testsignals import synth_speech

bridge.MIN_RMS = 50.0
bridge.NOISE_FILTER = True
TRANSCRIPT = {"text": ""}
TTS_CALLS = []


async def fake_transcribe(pcm): return TRANSCRIPT["text"]
async def fake_tts(text, cache=False):
    TTS_CALLS.append(text)
    n = int(0.5 * bridge.TTS_RATE)                                  # 0.5 s tone = 24 000 bytes
    return (np.sin(np.arange(n) * 0.05) * 8000).astype("<i2").tobytes()
async def fake_llm(q): return "Stubbed LLM answer."

bridge.transcribe, bridge.tts, bridge.ask_llm = fake_transcribe, fake_tts, fake_llm


def speech(seconds=1.5, amp=2500, noise=200):
    """Speech-like audio in background noise. amp=5,noise=5 => near-silence."""
    if seconds < 0.6:                                              # a quick button tap: not enough audio to be a question
        return (np.random.randn(int(16000 * seconds)) * noise).astype("<i2").tobytes()
    return synth_speech(seconds=seconds, lead=0.3, tail=0.2, level=amp, noise=noise)[0]


async def ask(ws, said, pcm=None, expect_move=False):
    """Do one press/release cycle; return (list of text msgs, audio bytes received)."""
    TRANSCRIPT["text"] = said
    await ws.send("PTT:1")
    data = pcm if pcm is not None else speech()
    for i in range(0, len(data), 2048):
        await ws.send(data[i:i + 2048])
    await ws.send("PTT:0")
    texts, audio, chunk, expected = [], 0, 0, 0
    while True:
        m = await asyncio.wait_for(ws.recv(), 5)
        if isinstance(m, bytes):
            audio += len(m)
            chunk += len(m)
            continue
        texts.append(m)
        if m.startswith("TTS_BEGIN:"):
            expected, chunk = int(m.split(":")[1]), 0
        if m == "TTS_END":
            assert chunk == expected, f"chunk {chunk} != announced {expected}"
            await ws.send("PLAYED:OK")
        if m == "STATE:idle" and (audio or "STATE:error" in texts):
            if expect_move:
                texts.append(await asyncio.wait_for(ws.recv(), 2))
            return texts, audio


async def main():
    async with serve(bridge.handler, "127.0.0.1", 8799, max_size=2**20):
        async with websockets.connect("ws://127.0.0.1:8799") as ws:
            await ws.send("HELLO:20000:24000:1400")
            await ws.send("CLIPS:")                                     # fresh robot: nothing stored yet
            await ws.send("CLIPS_END")
            assert await ws.recv() == "STATE:idle"

            t, a = await ask(ws, "What is EQUINOX?")
            assert bridge.KB.by_id["what_is_equinox"].answer in TTS_CALLS, TTS_CALLS
            assert a == 24000 and any(x.startswith("TTS_BEGIN:20000") for x in t), t       # split by firmware buffer
            print("PASS scripted answer, chunked to the firmware buffer:", [x for x in t if x.startswith("TTS")])

            t, a = await ask(ws, "how far is the moon")
            assert TTS_CALLS[-1] == "Stubbed LLM answer."; print("PASS LLM fallback path")

            t, a = await ask(ws, "", pcm=speech(1.5, amp=5, noise=5))
            assert TTS_CALLS[-1] == bridge.KB.fallback_no_speech; print("PASS silence gate -> 'didn't catch that'")

            t, a = await ask(ws, "", pcm=(np.random.randn(16000 * 2) * 400).astype("<i2").tobytes())
            assert TTS_CALLS[-1] == bridge.KB.fallback_no_speech; print("PASS loud steady background noise alone -> rejected by filter gate")

            t, a = await ask(ws, "thank you")
            assert TTS_CALLS[-1] == bridge.KB.fallback_no_speech; print("PASS Whisper-hallucination filter")

            t, a = await ask(ws, "", pcm=speech(0.2))
            assert TTS_CALLS[-1] == bridge.KB.by_id["greeting"].answer; print("PASS quick tap -> greeting")

            t, a = await ask(ws, "come forward", expect_move=True)
            assert t[-1].startswith("MOVE:F:"), t; print("PASS voice command ->", t[-1])

            # visitor presses again while the robot is still busy: old work must be cancelled
            TRANSCRIPT["text"] = "what is steam"
            await ws.send("PTT:1"); await ws.send(speech(1.0)); await ws.send("PTT:0")
            await ws.send("PTT:1")                                    # interrupt immediately
            await asyncio.sleep(0.3)
            await ws.send("PTT:0")
            while True:                                               # drain: ack whatever is still being spoken
                try:
                    m = await asyncio.wait_for(ws.recv(), 1.0)
                except asyncio.TimeoutError:
                    break
                if m == "TTS_END":
                    await ws.send("PLAYED:OK")
            print("PASS interrupt / re-press does not crash the bridge")

            await ws.send("BUMP")
            seen = []
            while True:
                m = await asyncio.wait_for(ws.recv(), 3)
                if isinstance(m, str):
                    seen.append(m)
                    if m.startswith("TTS_BEGIN"): pass
                    if m == "TTS_END": await ws.send("PLAYED:OK")
                    if m == "STATE:idle" and "STATE:speaking" in seen: break
            assert TTS_CALLS[-1] == bridge.KB.fallback_bump; print("PASS bumper -> spoken 'Oops'")

            # simulate TTS totally unavailable
            async def broken_tts(text, cache=False): raise RuntimeError("offline")
            bridge.tts = broken_tts
            t, a = await ask(ws, "What is EQUINOX?")
            assert "STATE:error" in t and a == 0; print("PASS TTS offline and not cached -> error state (firmware beeps)")
    print("\nALL PROTOCOL TESTS PASSED")

asyncio.run(main())
