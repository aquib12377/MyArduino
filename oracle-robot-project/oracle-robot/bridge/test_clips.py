"""
Clip-store test: a fake ESP32 with a fake flash filesystem syncs clips with the real bridge code and
plays them back. Speech synthesis is stubbed, so no API keys or hardware are needed.
Run: python test_clips.py
"""
import asyncio
import shutil
import numpy as np
import websockets
from pathlib import Path
from websockets.asyncio.server import serve

import bridge
import adpcm
from testsignals import synth_speech

TMP = Path("cache_test")
if TMP.exists():
    shutil.rmtree(TMP)

# rebuild the clip store into a scratch directory and pre-encode every clip (as --prebuild would)
bridge.CLIPS = bridge.ClipStore(bridge.KB, TMP, "test-model", "test-voice")
TTS_CALLS = []
PCM_BY_TEXT = {}
for c in bridge.CLIPS.clips.values():
    seconds = 1.0 + (len(c.text) % 5) * 0.5
    pcm = synth_speech(seconds=seconds, lead=0.05, tail=0.05, level=9000, noise=0)[1].astype("<i2").tobytes()
    PCM_BY_TEXT[c.text] = pcm
    bridge.CLIPS.encode_from_pcm(c, pcm, 24000)


async def fake_tts(text, cache=False):
    TTS_CALLS.append(text)
    return PCM_BY_TEXT.get(text, b"\x00\x00" * 2400)

async def fake_transcribe(pcm): return TRANSCRIPT["text"]
async def fake_llm(q): return "Stubbed LLM answer."

TRANSCRIPT = {"text": ""}
bridge.tts, bridge.transcribe, bridge.ask_llm = fake_tts, fake_transcribe, fake_llm
bridge.MIN_RMS = 50.0


class FakeRobot:
    """Minimal firmware model: flash store with a size limit, clip upload, PLAY from flash."""

    def __init__(self, ws, free_bytes):
        self.ws = ws
        self.flash: dict[str, bytes] = {}
        self.free = free_bytes
        self.rx_clip = None
        self.rx_buf = bytearray()
        self.task = None
        self.played = []          # ("clip", id) or ("stream", bytes)
        self.stream_expect = 0
        self.stream_got = 0

    async def start(self):
        await self.ws.send(f"HELLO:20000:24000:{self.free // 1024}")
        for i in range(0, max(len(self.flash), 1), 20):
            await self.ws.send("CLIPS:" + ",".join(list(self.flash)[i:i + 20]))
        await self.ws.send("CLIPS_END")
        self.task = asyncio.create_task(self._pump())          # the firmware handles messages continuously

    async def stop(self):
        self.task.cancel()
        try:
            await self.task
        except asyncio.CancelledError:
            pass

    async def wait(self, cond, timeout=60):
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while not cond(self):
            if loop.time() > deadline:
                return False
            await asyncio.sleep(0.05)
        return True

    async def _pump(self):
        while True:
            m = await self.ws.recv()
            if isinstance(m, bytes):
                if self.rx_clip:
                    self.rx_buf.extend(m)
                else:
                    self.stream_got += len(m)
                continue
            if m.startswith("CLIP_BEGIN:"):
                cid, n = m[11:].split(":")
                if int(n) + 16384 > self.free:                       # firmware refuses when nearly full
                    await self.ws.send(f"CLIP_ERR:{cid}")
                    self.rx_clip = None
                else:
                    self.rx_clip, self.rx_buf = (cid, int(n)), bytearray()
            elif m == "CLIP_END":
                cid, n = self.rx_clip
                ok = len(self.rx_buf) == n and n % adpcm.BLOCK_BYTES == 0
                if ok:
                    self.flash[cid] = bytes(self.rx_buf)
                    self.free -= n
                await self.ws.send(f"CLIP_OK:{cid}" if ok else f"CLIP_ERR:{cid}")
                self.rx_clip = None
            elif m.startswith("CLIP_DEL:"):
                cid = m[9:]
                self.free += len(self.flash.pop(cid, b""))
            elif m.startswith("PLAY:"):
                cid = m[5:]
                if cid in self.flash:
                    self.played.append(("clip", cid))
                    await self.ws.send("PLAYED:OK")
                else:
                    await self.ws.send("PLAYED:ERR")                 # bridge must fall back to streaming
            elif m.startswith("TTS_BEGIN:"):
                self.stream_expect, self.stream_got = int(m[10:]), 0
            elif m == "TTS_END":
                self.played.append(("stream", self.stream_got))
                await self.ws.send("PLAYED:OK")

    async def ask(self, said, seconds=1.5):
        TRANSCRIPT["text"] = said
        before = len(self.played)
        await self.ws.send("PTT:1")
        pcm = synth_speech(seconds=seconds, lead=0.3, tail=0.2, level=2500, noise=200)[0]
        for i in range(0, len(pcm), 2048):
            await self.ws.send(pcm[i:i + 2048])
        await self.ws.send("PTT:0")
        assert await self.wait(lambda r: len(r.played) > before, 40), "robot never got an answer"
        return self.played[-1]


fails = 0
def check(name, cond, detail=""):
    global fails
    fails += not cond
    print(("PASS  " if cond else "FAIL  ") + name + (f"   [{detail}]" if detail else ""))


async def main():
    async with serve(bridge.handler, "127.0.0.1", 8801, max_size=2**20):
        # ---------- 1. plenty of flash: everything syncs, answers play from flash ----------
        async with websockets.connect("ws://127.0.0.1:8801") as ws:
            r = FakeRobot(ws, free_bytes=4 * 1024 * 1024)
            await r.start()
            total = len(bridge.CLIPS.upload_order())
            await r.wait(lambda x: len(x.flash) >= total, 90)
            check("all clips uploaded to flash", len(r.flash) == total, f"{len(r.flash)} clips, {r.free/1024:.0f} KB left")
            check("clips decode to sensible audio", all(adpcm.decode(v).size > 0 for v in r.flash.values()))

            n_tts = len(TTS_CALLS)
            kind, cid = await r.ask("What is EQUINOX?")
            check("scripted answer plays from flash, nothing streamed", kind == "clip")
            check("no speech synthesis for a scripted answer", len(TTS_CALLS) == n_tts)
            text = next(c.text for c in bridge.CLIPS.clips.values() if c.id == cid)
            check("the right clip was played", text == bridge.KB.by_id["what_is_equinox"].answer)

            kind, size = await r.ask("how far is the moon")
            check("LLM answer still streams as PCM", kind == "stream" and size > 0, f"{size} bytes")

            # ---------- 2. clip missing from flash -> automatic fallback ----------
            gone = bridge.CLIPS.get(bridge.KB.by_id["steam"].answer).id
            del r.flash[gone]                                        # simulate a corrupt/erased file
            kind, val = await r.ask("what does steam stand for")
            check("missing clip falls back to streaming", kind == "stream" and val > 0)
            await r.stop()

        # ---------- 3. reconnect: already-stored clips are not re-uploaded ----------
        async with websockets.connect("ws://127.0.0.1:8801") as ws:
            r2 = FakeRobot(ws, free_bytes=4 * 1024 * 1024 - sum(len(v) for v in r.flash.values()))
            r2.flash = dict(r.flash)
            await r2.start()
            await r2.wait(lambda x: len(x.flash) == len(r.flash) + 1, 30)
            uploaded = [k for k in r2.flash if k not in r.flash]
            check("only the missing clip is re-uploaded on reconnect", len(uploaded) == 1, f"{len(uploaded)} uploaded")
            await r2.stop()

        # ---------- 4. small flash: priority order, no overflow, rest still works ----------
        async with websockets.connect("ws://127.0.0.1:8801") as ws:
            r3 = FakeRobot(ws, free_bytes=140 * 1024)
            await r3.start()
            await r3.wait(lambda x: x.free < 40 * 1024 or len(x.flash) >= len(bridge.CLIPS.clips), 60)
            await asyncio.sleep(1.5)
            stored = {c.id: c for c in bridge.CLIPS.clips.values() if c.id in r3.flash}
            check("small flash is not overfilled", r3.free >= 0 and len(r3.flash) < len(bridge.CLIPS.clips),
                  f"{len(r3.flash)}/{len(bridge.CLIPS.clips)} clips, {r3.free/1024:.0f} KB left")
            check("greeting and fallbacks win the space", all(p <= 1 for p in (c.priority for c in stored.values())),
                  "priorities " + str(sorted({c.priority for c in stored.values()})))
            greeting = bridge.CLIPS.get(bridge.KB.by_id["greeting"].answer)
            check("greeting is stored first", greeting.id in r3.flash)
            kind, val = await r3.ask("where is the music room")       # a venue answer that did not fit
            check("answers that did not fit still stream", kind == "stream" and val > 0)
            await r3.stop()

    shutil.rmtree(TMP, ignore_errors=True)
    print(f"\n{'ALL CLIP TESTS PASSED' if not fails else str(fails) + ' FAILED'}")
    return fails


import sys
sys.exit(1 if asyncio.run(main()) else 0)
