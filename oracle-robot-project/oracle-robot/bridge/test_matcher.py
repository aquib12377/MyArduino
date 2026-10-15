"""
Offline regression tests for the keyword matcher.
Run:  python test_matcher.py          (no API keys or hardware needed)
Add a line to CASES whenever a visitor question mis-routes at the event.
"""
import sys
from pathlib import Path
from matcher import Knowledge, detect_action, is_hallucination

K = Knowledge(Path(__file__).with_name("knowledge.json"))

# (spoken text, expected intent id or None => goes to the LLM)
CASES = [
    # the ten scripted questions, verbatim
    ("What is EQUINOX?", "what_is_equinox"),
    ("What is the theme of this year's exhibition?", "theme"),
    ("What does STEAM stand for?", "steam"),
    ("What is 'Lighting Lives'?", "lighting_lives"),
    ("Which cause or charity does 'Lighting Lives' support?", "charity"),
    ("What kinds of projects can I see today?", "projects"),
    ("Where can I buy the 'Lighting Lives' artifacts?", "buy"),
    ("How do these projects help students learn?", "learning"),
    ("Who organized EQUINOX?", "organizer"),
    ("How can visitors support our students today?", "support"),
    # keyword-style / sloppy phrasing
    ("about equinox", "what_is_equinox"),
    ("tell me about equinox please", "what_is_equinox"),
    ("what is the meaning of equinox", "what_is_equinox"),
    ("reimagining tomorrow", "theme"),
    ("what's the topic", "theme"),
    ("what is steam", "steam"),
    ("full form of STEAM", "steam"),
    ("what does stem stand for", "steam"),
    ("tell me about lightning lives", "lighting_lives"),          # STT: lighting -> lightning
    ("student startup", "lighting_lives"),
    ("where do the funds go", "charity"),
    ("what is the smith old age home", "charity"),               # STT: Smit -> Smith
    ("donation", "charity"),
    ("what can I see here", "projects"),
    ("exhibits", "projects"),
    ("where do I buy handmade items", "buy"),
    ("can I purchase artifacts", "buy"),
    ("why STEAM", "learning"),
    ("what skills will students get", None),                      # weak-only hit -> LLM answers from the facts
    ("who is the principal", "organizer"),
    ("which school is this", None),                               # weak-only hit -> LLM (it knows the school from the facts)
    ("how can I help", "support"),
    ("how can I participate", "support"),
    # greeting
    ("hello", "greeting"),
    ("hi oracle", "greeting"),
    ("hello oracle", "inauguration_welcome"),
    ("Hello, Oracle!", "inauguration_welcome"),
    ("who are you", "greeting"),
    ("what is your name", "greeting"),
    ("hello what is equinox", "what_is_equinox"),                # question beats the hello
    # venue lookups (from the docx)
    ("where is smart homes", "venue_smart_homes"),
    ("where can I find the future cities exhibit", "venue_future_cities"),
    ("where is the music room", "venue_future_cities"),
    ("where is the selfie point", "venue_selfie"),
    ("what is in classroom 1 A", "venue_clean_earth"),
    ("classroom 10 c", "venue_tech_with_heart"),
    ("where is robosphere", "venue_robosphere_the_robotics_projects"),
    ("where is future learning", "venue_future_learning"),       # 'learning' must not hijack
    ("roots and shoots", "venue_the_earthshot_challenge_and_roots_and_shoots"),
    # must NOT be answered by a script -> LLM (generic / off-topic / ambiguous)
    ("can you help me", None),
    ("what is the weather today", None),
    ("how far is the moon", None),
    ("where is the washroom", None),
    ("what time does the exhibition end", None),
    ("I want to buy a car", None),
    ("where can I buy snacks", None),
    ("what is machine learning", None),
    ("what is a steam engine", None),
    ("how does your display work", None),
    ("what is the cause of rain", None),
    ("what skills do I need for coding", None),
    ("what is the theme of the movie frozen", None),
    ("what is the capital of france", None),
    ("tell me a joke", None),
]

fails = 0
for text, expect in CASES:
    m = K.match(text)
    got = m.intent.id if m.intent else None
    ok = got == expect
    fails += not ok
    print(f"{'PASS' if ok else 'FAIL'}  {text!r:62} -> {got or 'LLM'}  (score {m.score}/{m.runner_up}, {m.reason})"
          + ("" if ok else f"   EXPECTED {expect or 'LLM'}"))

ACTION_CASES = [("come forward", "F"), ("go back", "B"), ("turn left", "L"), ("turn to the right", "R"),
                ("stop", "S"), ("please stop moving", "S"), ("come here", "F"),
                ("what is the theme of this exhibition and where can I go back", None),
                ("who is the principal", None)]
for text, expect in ACTION_CASES:
    got = detect_action(text)
    ok = got == expect
    fails += not ok
    print(f"{'PASS' if ok else 'FAIL'}  action {text!r:50} -> {got}" + ("" if ok else f"  EXPECTED {expect}"))

for text, expect in [("Thank you.", True), ("thanks for watching!", True), ("What is EQUINOX", False)]:
    ok = is_hallucination(text) == expect
    fails += not ok
    print(f"{'PASS' if ok else 'FAIL'}  hallucination-filter {text!r}")

print(f"\n{len(CASES)+len(ACTION_CASES)+3-fails} passed, {fails} failed")
sys.exit(1 if fails else 0)
