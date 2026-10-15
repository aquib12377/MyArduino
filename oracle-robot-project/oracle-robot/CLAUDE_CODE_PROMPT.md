# Claude Code prompt — migrate the ORACLE bridge to Claude (Haiku) with local STT/TTS

Run `claude` inside the unzipped `oracle-robot/` folder and paste everything below the line.

---

I have an ESP32-S3 exhibition robot ("ORACLE") with a Python bridge that runs on my laptop. Read `README.md` and
everything in `bridge/` before changing anything.

**The only AI API key I have is an Anthropic (Claude) key.** The Anthropic API is text-only: it has no speech-to-text and
no text-to-speech. So migrate the bridge to Claude for the language step and move speech recognition and speech synthesis
to local, offline engines. Do not add any other paid provider.

## Hard constraints

- Audio must **never** be sent to a model. The pipeline stays: mic audio → local STT → **text** → Claude → **text** →
  local TTS → audio. Do not use any audio-in/audio-out or realtime API.
- Do not change `oracle_esp32s3/oracle_esp32s3.ino` or the WebSocket protocol. The firmware must keep working unflashed.
- Do not change `matcher.py`, `knowledge.json`, `audio_filter.py`, `adpcm.py` or the clip-store logic in `clips.py`
  except where explicitly listed below.
- Keep the existing behaviour: keyword-matched scripted answers first, Claude only for unmatched questions; on-device
  ADPCM clips with streaming fallback; all existing timeouts and error paths.
- Every existing test must still pass: `test_matcher.py`, `test_audio_filter.py`, `test_adpcm.py`, `test_protocol.py`,
  `test_clips.py`.

## Task 1 — Claude replaces OpenAI/DeepSeek for the language step

In `bridge.py`:

- Remove the `openai`/`deepseek` clients and the provider-failover loop. Use the official `anthropic` package with
  `AsyncAnthropic(api_key=..., max_retries=0, timeout=15)`.
- Rewrite `ask_llm(question)` to call `client.messages.create(...)` with:
  - `model` from env `ANTHROPIC_MODEL`, default `claude-haiku-4-5` (a convenience alias that resolves to the pinned
    snapshot `claude-haiku-4-5-20251001` — put both in a comment in `.env.example`).
  - `max_tokens=160`, `temperature=0.4`.
  - The existing `SYSTEM_PROMPT` passed as the **top-level `system=` parameter**, not as a message. Keep the prompt text
    unchanged — it already contains the event facts and the safety rules.
  - `messages=[{"role": "user", "content": question}]`. Each visitor question stays stateless; do not add history.
  - Do **not** enable extended thinking — this is a latency-critical kiosk.
- Extract the reply by concatenating the `.text` of content blocks whose `type == "text"`, then pass it through the
  existing `clean_for_speech()`. Keep the existing behaviour on failure: log the exception type and message, return
  `None`, and let the caller speak the cached offline apology.
- Keep the `LLM_TIMEOUT` wrapper. Since there is no second provider now, on failure just return `None`.
- Verify the exact request shape and model IDs against the official docs before writing the code:
  https://docs.claude.com/en/api/overview and https://platform.claude.com/docs/en/models/haiku-4-5/overview

## Task 2 — speech-to-text becomes local-only

- `transcribe()` must use `faster-whisper` only. Delete the OpenAI STT path and the `STT_PROVIDER` env var; keep
  `STT_MODEL` (whisper size, default `small.en`) and `STT_LANGUAGE`.
- Keep the existing `initial_prompt` with the event vocabulary (EQUINOX, ORACLE, Lighting Lives, Smit Old Age Home,
  Lodha World School, RoboSphere, GreenNova, Ethinova) — it materially improves recognition of these names.
- **Preload the model in a background thread at bridge startup**, so the first visitor does not wait 10–30 s for it to
  load. Log when loading starts and finishes. If a question arrives before loading is done, wait for it rather than
  failing.
- If the model fails to load, log a clear error and make `transcribe()` return `""` so the robot says "I didn't catch
  that" instead of hanging.

## Task 3 — text-to-speech becomes local-only

Create `bridge/tts_engine.py` exposing `synth(text) -> tuple[bytes, int]` (int16 mono PCM, sample rate):

- Primary engine: **Piper** (offline, fast, natural). Voice model path from env `PIPER_VOICE`. Prefer the `piper-tts`
  Python API; fall back to invoking the `piper` binary with `--output_raw` if the package is unavailable.
- Fallback engine: `pyttsx3` (Windows SAPI / macOS NSSpeechSynthesizer / espeak), so the project still runs if Piper is
  not installed. Selected by env `TTS_ENGINE=piper|pyttsx3`, default `piper`, with automatic fallback and a warning log.
- Resample the engine's native rate to the target rate with `scipy.signal.resample_poly` (`adpcm.resample_to_clip_rate`
  already shows the pattern). Streaming playback needs 24 kHz; clips need `adpcm.CLIP_RATE` (16 kHz).
- In `bridge.py`, `tts(text, cache=False)` keeps its signature, its disk cache and its error behaviour — only the engine
  changes. The cache key and the `ClipStore` id must include the engine name and voice instead of the old OpenAI model
  and voice, so switching voice invalidates old caches and re-uploads clips. (`ClipStore(kb, cache_dir, model, voice)`
  is already parameterised — pass the new values.)
- Delete `TTS_STYLE`/`instructions` handling, which was OpenAI-specific.
- Note in the logs at `--prebuild` that clip generation is now fully offline and free.

## Task 4 — configuration, dependencies, docs

- `.env.example`: remove `OPENAI_API_KEY`, `DEEPSEEK_API_KEY`, `LLM_PROVIDER`, `OPENAI_CHAT_MODEL`,
  `DEEPSEEK_CHAT_MODEL`, `STT_PROVIDER`, `TTS_MODEL`, `TTS_VOICE`. Add `ANTHROPIC_API_KEY`, `ANTHROPIC_MODEL`,
  `TTS_ENGINE`, `PIPER_VOICE`, `LOCAL_STT_MODEL`. Keep every other variable and comment.
- `requirements.txt`: drop `openai`, add `anthropic`, `faster-whisper` (now required, not optional) and `piper-tts`,
  with `pyttsx3` as an optional line.
- `README.md`: update the architecture diagram and the setup section — where to download a Piper voice (`.onnx` +
  `.onnx.json`), the first-run Whisper model download, and the fact that after `--prebuild` the scripted path needs no
  internet at all. Update the scenario matrix rows that mention OpenAI/DeepSeek: with Claude-only keys, losing the
  internet now costs only the ChatGPT-style fallback answers, because STT, TTS and all clips are local.
- Update the "what has and has not been verified" section honestly for whatever you did and did not test.

## Task 5 — tests

- Add `bridge/test_llm_claude.py`: monkeypatch the Anthropic client (no network, no key) and assert the system prompt is
  passed via `system=`, the model comes from env, `max_tokens` is set, text is correctly extracted from a multi-block
  response, markdown/emoji are stripped by `clean_for_speech()`, and an API exception or timeout returns `None`.
- Add `bridge/test_tts_engine.py`: assert `synth()` returns int16 PCM at the expected rate, that resampling to 24 kHz and
  16 kHz produces the right sample counts and no clipping, and that an engine failure raises cleanly so `tts()` can fall
  back to the cached apology. Skip the real-synthesis test with a clear message if no Piper voice is installed.
- Keep the existing test files working. Where they stub `bridge.tts` / `bridge.ask_llm`, update the stubs to the new
  signatures rather than rewriting the tests.

## Acceptance

Run and show me the output of all seven test files, plus:

- `python bridge.py --ask "what is equinox"` → scripted answer, no API call.
- `python bridge.py --ask "how far is the moon"` → a real Claude Haiku answer (I will have `ANTHROPIC_API_KEY` set).
- `python bridge.py --prebuild` → every clip encoded with no network errors.

Work in small steps, run the tests after each one, and tell me plainly anything you could not verify without hardware.
