"""
On-device clip store.

Every sentence the robot can say from the script (greeting, the ten answers, venue answers, fallbacks,
movement acknowledgements) is rendered once, compressed to IMA ADPCM and stored in the ESP32's flash.
After that the robot plays them straight from flash: no speech synthesis, no network transfer, and they
keep working if the laptop or the internet disappears mid-event.

A clip id is a hash of the text plus the voice settings, so editing an answer in knowledge.json creates a
new id: the robot uploads the new clip and deletes the stale one by itself.  Flash is finite, so clips are
uploaded in priority order and the bridge stops when the robot reports it is nearly full - anything that
does not fit simply streams over Wi-Fi as before.
"""
from __future__ import annotations

import hashlib
from dataclasses import dataclass
from pathlib import Path

import adpcm


@dataclass
class Clip:
    id: str
    text: str
    priority: int          # lower = uploaded first
    path: Path

    @property
    def ready(self) -> bool:
        return self.path.exists() and self.path.stat().st_size > 0

    @property
    def size(self) -> int:
        return self.path.stat().st_size if self.ready else 0


def clip_id(text: str, model: str, voice: str) -> str:
    return hashlib.sha1(f"{model}|{voice}|{adpcm.CLIP_RATE}|{text}".encode()).hexdigest()[:12]


class ClipStore:
    """Builds the clip list from the knowledge base and keeps the encoded files on disk."""

    def __init__(self, kb, cache_dir: Path, model: str, voice: str):
        self.dir = cache_dir / "clips"
        self.dir.mkdir(parents=True, exist_ok=True)
        self.model, self.voice = model, voice
        self.clips: dict[str, Clip] = {}
        self.by_text: dict[str, Clip] = {}
        for text, prio in self._ordered(kb):
            cid = clip_id(text, model, voice)
            if cid in self.clips:
                continue
            c = Clip(cid, text, prio, self.dir / f"{cid}.adp")
            self.clips[cid] = c
            self.by_text[text] = c

    @staticmethod
    def _ordered(kb) -> list[tuple[str, int]]:
        """Most-used sentences first, so the important ones win the flash space."""
        out: list[tuple[str, int]] = [(kb.by_id["greeting"].answer, 0)]
        out += [(kb.fallback_no_speech, 1), (kb.fallback_offline, 1), (kb.fallback_bump, 1)]
        out += [(t, 1) for t in kb.action_acks.values()]
        out += [(i.answer, 2) for i in kb.intents if i.kind == "faq"]
        out += [(i.answer, 3) for i in kb.intents if i.kind == "venue"]
        out += [(i.answer, 4) for i in kb.intents if i.kind not in ("faq", "venue")]
        return out

    def get(self, text: str) -> Clip | None:
        return self.by_text.get(text)

    def encode_from_pcm(self, clip: Clip, pcm: bytes, src_rate: int) -> int:
        data = adpcm.pcm_bytes_to_clip(pcm, src_rate)
        clip.path.write_bytes(data)
        return len(data)

    def upload_order(self) -> list[Clip]:
        return sorted((c for c in self.clips.values() if c.ready), key=lambda c: (c.priority, -c.size))

    def total_bytes(self) -> int:
        return sum(c.size for c in self.clips.values())

    def missing(self) -> list[Clip]:
        return [c for c in self.clips.values() if not c.ready]
