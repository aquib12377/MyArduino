"""Offline tests for the DeepSeek language step of the bridge (no network, no API key). Run: python test_llm_deepseek.py"""
import asyncio
import importlib
import os
import sys
import time
from types import SimpleNamespace as NS

os.environ["DEEPSEEK_MODEL"] = "deepseek-test-model"         # set before the import: the model must come from the env
os.environ["DEEPSEEK_API_KEY"] = ""                            # empty (not unset) so load_dotenv() cannot pull the real key from .env

import bridge

fails = 0
def check(name, cond, detail=""):
    global fails
    fails += not cond
    print(("PASS  " if cond else "FAIL  ") + name + (f"   [{detail}]" if detail else ""))


def reply(content, finish="stop"):
    return NS(choices=[NS(message=NS(content=content, reasoning_content="ignore me"), finish_reason=finish)])


class FakeCompletions:
    def __init__(self, response=None, exc=None, delay=0.0):
        self.response, self.exc, self.delay, self.calls = response, exc, delay, []

    async def create(self, **kwargs):
        self.calls.append(kwargs)
        if self.delay:
            await asyncio.sleep(self.delay)
        if self.exc:
            raise self.exc
        return self.response


def install(**kw):
    c = FakeCompletions(**kw)
    bridge._llm = NS(chat=NS(completions=c))
    return c


def ask(q="how far is the moon"):
    return asyncio.run(bridge.ask_llm(q))


# ---- request shape ---------------------------------------------------------------------------------------------
c = install(response=reply("About 384,000 kilometres."))
out = ask("how far is the moon")
call = c.calls[0]
check("returns the reply text", out == "About 384,000 kilometres.", repr(out))
check("system prompt is the first (system-role) message", call["messages"][0] == {"role": "system", "content": bridge.SYSTEM_PROMPT})
check("then exactly one stateless user message", call["messages"][1:] == [{"role": "user", "content": "how far is the moon"}],
      str(call["messages"][1:]))
check("model comes from DEEPSEEK_MODEL", call["model"] == "deepseek-test-model", call["model"])
check("max_tokens is set (160)", call.get("max_tokens") == 160, str(call.get("max_tokens")))
check("temperature 0.4", call.get("temperature") == 0.4)
check("thinking mode is explicitly disabled (DeepSeek thinks by default)",
      call.get("extra_body") == {"thinking": {"type": "disabled"}}, str(call.get("extra_body")))
check("system prompt still carries the event facts and the safety rules",
      "EQUINOX" in bridge.SYSTEM_PROMPT and "FACTS" in bridge.SYSTEM_PROMPT and "unsafe" in bridge.SYSTEM_PROMPT)
ask("and the sun?")
check("stateless: second call has system + one user message only", len(c.calls[1]["messages"]) == 2)

# ---- default model / endpoint / key handling ---------------------------------------------------------------------
os.environ.pop("DEEPSEEK_MODEL")
importlib.reload(bridge)
check("default model is deepseek-flash", bridge.DEEPSEEK_MODEL == "deepseek-flash", bridge.DEEPSEEK_MODEL)
os.environ["DEEPSEEK_MODEL"] = "deepseek-v4-pro"
importlib.reload(bridge)
check("another model can be set through the env", bridge.DEEPSEEK_MODEL == "deepseek-v4-pro")
check("endpoint is DeepSeek's", bridge.DEEPSEEK_BASE_URL == "https://api.deepseek.com")
check("client is not built without a key", bridge._llm is None)
check("no key -> None (caller speaks the apology)", asyncio.run(bridge.ask_llm("hi")) is None)
os.environ["DEEPSEEK_API_KEY"] = "sk-test"
importlib.reload(bridge)
check("with a key, the client points at DeepSeek", bridge._llm is not None and "api.deepseek.com" in str(bridge._llm.base_url),
      str(getattr(bridge._llm, "base_url", None)))
os.environ["DEEPSEEK_API_KEY"] = ""
importlib.reload(bridge)

# ---- speech cleaning ---------------------------------------------------------------------------------------------
install(response=reply("**The moon** is about 384,000 km away ☀ - see https://example.com/moon \U0001F680 for #more.\n\n"))
out = ask()
check("markdown, emoji, urls and symbols are stripped for speech",
      out is not None and not any(ch in out for ch in "*#_`☀\U0001F680") and "http" not in out, repr(out))
check("text is kept", out is not None and "The moon is about 384,000 km away" in out, repr(out))
check("whitespace is collapsed", out is not None and "  " not in out and out == out.strip())
check("reasoning_content is never spoken", out is not None and "ignore me" not in out)
check("clean_for_speech: bullets and newlines", bridge.clean_for_speech("- one\n- two\n\n• three") == "one two three")

# ---- failures return None ------------------------------------------------------------------------------------------
class Boom(Exception): pass

install(exc=Boom("503 server busy"))
check("API exception -> None", ask() is None)
install(exc=ConnectionError("network down"))
check("connection error -> None", ask() is None)

bridge.LLM_TIMEOUT = 0.15
install(response=reply("too late"), delay=2.0)
t0 = time.monotonic()
r = ask()
check("timeout -> None, quickly", r is None and time.monotonic() - t0 < 1.5, f"{time.monotonic() - t0:.2f}s")

install(response=reply("", finish="length"))
check("empty content -> None", ask() is None)
install(response=reply(None))
check("null content -> None", ask() is None)
install(response=NS(choices=[]))
check("no choices -> None", ask() is None)
install(response=reply("  \U0001F600  "))
check("emoji-only reply cleans to empty -> None", ask() is None)


# ---- the caller: an unmatched question falls back to the offline apology when DeepSeek fails ---------------------
async def respond_flow():
    said = []
    bridge._llm = NS(chat=NS(completions=FakeCompletions(exc=Boom("down"))))

    class R(bridge.Robot):
        def __init__(self): self.busy = False; self.sent = []
        async def state(self, n): pass
        async def send(self, m): self.sent.append(m)
        async def speak(self, t, mood="happy", cache=False): said.append(t)

    async def fake_transcribe(pcm): return "how far is the moon"
    bridge.transcribe = fake_transcribe
    bridge.NOISE_FILTER = False
    bridge.MIN_RMS = 0
    import numpy as np
    pcm = (np.random.default_rng(1).normal(0, 3000, 16000 * 2)).astype("<i2").tobytes()
    await R()._respond(pcm)
    return said

said = asyncio.run(respond_flow())
check("DeepSeek failure -> the robot speaks the cached offline apology", said == [bridge.KB.fallback_offline], str(said))

print(f"\n{'ALL PASSED' if not fails else str(fails) + ' FAILED'}")
sys.exit(1 if fails else 0)
