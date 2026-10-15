"""
ORACLE matcher - turns a speech transcript into a scripted answer, a movement
action, or "no confident match" (which the bridge then hands to the LLM).

No network / API dependencies on purpose: this module is unit-tested offline
(see test_matcher.py) and is the part you tune when a question mis-routes.

Scoring
-------
Every intent has keyword phrases with a weight:
    3 = distinctive phrase   ("where can i buy")
    2 = distinctive word     ("charity")
    1 = generic word         ("help", "school", "steam")  - never answers alone
An intent's score is the sum of the weights of the distinct phrases found in
the transcript (whole-word match), +4 if the transcript is almost identical to
the intent's canonical question.  The best intent wins only if
    best >= MIN_SCORE  and  best - runner_up >= MIN_MARGIN
otherwise the question is ambiguous or unknown and goes to the LLM.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from rapidfuzz import fuzz

MIN_SCORE = 2
MIN_MARGIN = 1
FUZZY_QUESTION_RATIO = 82
FUZZY_BONUS = 4
ACTION_MAX_WORDS = 6          # movement commands must be short utterances
LONG_UTTERANCE_WORDS = 10     # longer than this, a lone keyword is not enough ...
LONG_UTTERANCE_MIN_SCORE = 5  # ... so it goes to the LLM (which also sees the event facts)

# Common speech-to-text mishearings for this event's vocabulary.
ALIASES = {
    "lightning lives": "lighting lives",
    "lighting life": "lighting lives",
    "lightning life": "lighting lives",
    "smith old age home": "smit old age home",
    "smitt old age home": "smit old age home",
    "smit old age": "smit old age home",
    "equinox's": "equinox",
    "e quinox": "equinox",
    "equinoxes": "equinox",
    "stem": "steam",            # only applied when the word stands alone
    "reimagine tomorrow": "reimagining tomorrow",
    "re imagining tomorrow": "reimagining tomorrow",
    "oracle's": "oracle",
}
# STEM->STEAM is risky ("stem cells"), so it is only applied next to "what is/does".
_STEM_FIX = re.compile(r"\b(what is|what does|meaning of|why)\s+stem\b")

HALLUCINATIONS = {
    "", "you", "thank you", "thanks", "thanks for watching", "thank you for watching",
    "bye", "bye bye", "okay", "ok", "the end", "subscribe", "um", "uh", "hmm", "mm",
    "please subscribe", "see you next time",
}


def normalize(text: str) -> str:
    t = text.lower().replace("\u2019", "'")
    t = t.replace("'", "")                       # "year's" -> "years"
    t = re.sub(r"[^a-z0-9 ]+", " ", t)
    t = re.sub(r"\s+", " ", t).strip()
    t = re.sub(r"\bwhats\b", "what is", t)
    t = _STEM_FIX.sub(lambda m: m.group(1) + " steam", t)
    for wrong, right in ALIASES.items():
        if wrong == "stem":
            continue
        t = re.sub(rf"\b{re.escape(normalize_kw(wrong))}\b", right, t)
    # "1 A", "1-a", "one a" style room codes -> "1a"
    t = re.sub(r"\b(\d{1,2}) ?([a-e])\b", r"\1\2", t)
    return t


def normalize_kw(s: str) -> str:
    s = s.lower().replace("'", "")
    s = re.sub(r"[^a-z0-9 ]+", " ", s)
    return re.sub(r"\s+", " ", s).strip()


def is_hallucination(text: str) -> bool:
    return normalize(text) in HALLUCINATIONS


@dataclass
class Intent:
    id: str
    kind: str
    question: str
    answer: str
    question_norm: str = ""
    min_score: int = MIN_SCORE
    patterns: list = field(default_factory=list)   # (compiled_regex, weight, phrase)
    blockers: list = field(default_factory=list)   # compiled regexes: if any hits, intent scores 0


@dataclass
class Match:
    intent: Intent | None
    score: int
    runner_up: int
    reason: str


class Knowledge:
    def __init__(self, path: str | Path):
        raw = json.loads(Path(path).read_text(encoding="utf-8"))
        self.robot_name: str = raw.get("robot_name", "ORACLE")
        self.fallback_no_speech: str = raw["fallback_no_speech"]
        self.fallback_offline: str = raw["fallback_offline"]
        self.fallback_bump: str = raw["fallback_bump"]
        self.action_acks: dict[str, str] = raw["action_acks"]
        self.intents: list[Intent] = []
        for it in raw["intents"]:
            intent = Intent(id=it["id"], kind=it["kind"], question=it["question"], answer=it["answer"],
                            question_norm=normalize(it["question"]), min_score=int(it.get("min_score", MIN_SCORE)))
            for b in it.get("block", []):
                intent.blockers.append(re.compile(rf"\b{re.escape(normalize_kw(b))}\b"))
            for weight, phrases in it["keywords"].items():
                for p in phrases:
                    pn = normalize_kw(p)
                    if pn:
                        intent.patterns.append((re.compile(rf"\b{re.escape(pn)}\b"), int(weight), pn))
            self.intents.append(intent)
        self.by_id = {i.id: i for i in self.intents}

    # ------------------------------------------------------------------ matching
    def score(self, text_norm: str, intent: Intent) -> int:
        if any(rx.search(text_norm) for rx in intent.blockers):
            return 0
        s = sum(w for rx, w, _ in intent.patterns if rx.search(text_norm))
        if s and fuzz.ratio(text_norm, intent.question_norm) >= FUZZY_QUESTION_RATIO:
            s += FUZZY_BONUS
        elif not s and fuzz.ratio(text_norm, intent.question_norm) >= FUZZY_QUESTION_RATIO + 6:
            s += FUZZY_BONUS          # near-verbatim question with no keyword hit
        return s

    def match(self, transcript: str) -> Match:
        t = normalize(transcript)
        if not t:
            return Match(None, 0, 0, "empty")
        scored = sorted(((self.score(t, i), i) for i in self.intents), key=lambda x: -x[0])
        best, best_i = scored[0]
        second = scored[1][0] if len(scored) > 1 else 0
        if best < max(MIN_SCORE, best_i.min_score):
            return Match(None, best, second, "below-threshold")
        if len(t.split()) > LONG_UTTERANCE_WORDS and best < LONG_UTTERANCE_MIN_SCORE:
            return Match(None, best, second, "long-utterance-weak-hit")
        if best - second < MIN_MARGIN:
            return Match(None, best, second, f"ambiguous({best_i.id} vs {scored[1][1].id})")
        return Match(best_i, best, second, "keyword")

    # ------------------------------------------------------------------ LLM context
    def facts_block(self) -> str:
        lines = []
        for i in self.intents:
            if i.kind == "faq":
                lines.append(f"Q: {i.question}\nA: {i.answer}")
        venues = [i.answer for i in self.intents if i.kind == "venue"]
        lines.append("Exhibit locations:\n" + "\n".join(venues))
        return "\n\n".join(lines)

    def speakable_texts(self) -> list[str]:
        texts = [i.answer for i in self.intents]
        texts += [self.fallback_no_speech, self.fallback_offline, self.fallback_bump]
        texts += list(self.action_acks.values())
        return list(dict.fromkeys(texts))


# ---------------------------------------------------------------------- actions
_ACTIONS = [
    (re.compile(r"\b(come|move|go|walk|step|drive)\b.*\b(forward|forwards|ahead)\b|\bcome here\b|\bcome closer\b"), "F"),
    (re.compile(r"\b(go|move|step|walk|drive)\b.*\bback(ward|wards)?\b|\bback up\b|\breverse\b"), "B"),
    (re.compile(r"\bturn\b.*\bleft\b"), "L"),
    (re.compile(r"\bturn\b.*\bright\b"), "R"),
    (re.compile(r"\b(stop|halt|freeze|stay)\b"), "S"),
]


def detect_action(transcript: str) -> str | None:
    """Return F/B/L/R/S for a short movement command, else None."""
    t = normalize(transcript)
    if not t or len(t.split()) > ACTION_MAX_WORDS:
        return None
    for rx, code in _ACTIONS:
        if rx.search(t):
            return code
    return None
