/*
 * BuildingLED.cpp
 * Implementation of the BuildingLED controller
 */

#include "BuildingLED.h"

/* ═══════════════════════ STATIC INSTANCE ═══════════════════════ */

BuildingLED* BuildingLED::_instance = nullptr;

/* ═══════════════════════ CONSTRUCTOR / DESTRUCTOR ═══════════════════════ */

BuildingLED::BuildingLED(const BuildingConfig& cfg)
  : _cfg(cfg)
  , _strips(nullptr)
  , _hasPkt(false)
  , _hasLegacy(false)
  , _legacyCmd(0)
  , _animActive(false)
  , _animId(ANIM_NONE)
  , _animStartTime(0)
  , _animLastFrame(0)
  , _animStep(0)
  , _cmdHandler(nullptr)
  , _lastCmdTime(0)
  , _idleTriggered(false)
{
  memset(_anims, 0, sizeof(_anims));
  memset(_presets, 0, sizeof(_presets));
  memset((void*)_isrRaw, 0, BLED_PKT_SIZE);
  _instance = this;
}

BuildingLED::~BuildingLED() {
  if (_strips) {
    for (uint8_t i = 0; i < _cfg.floorCount; i++) {
      delete _strips[i];
    }
    delete[] _strips;
  }
  if (_instance == this) _instance = nullptr;
}

/* ═══════════════════════ LIFECYCLE ═══════════════════════ */

void BuildingLED::begin() {
  BLED_DBGFLN("[BLED] Initializing...");

  // Allocate strip array
  _strips = new Adafruit_NeoPixel*[_cfg.floorCount];

  for (uint8_t i = 0; i < _cfg.floorCount; i++) {
    uint8_t pin = _cfg.floors[i].pin;
    uint16_t leds = _getLayout(i).totalLeds;

    _strips[i] = new Adafruit_NeoPixel(leds, pin, NEO_GRB + NEO_KHZ800);
    _strips[i]->begin();
    _strips[i]->setBrightness(_cfg.brightness);
    _strips[i]->clear();
    _strips[i]->show();

    BLED_PRINTF("[BLED] Floor %u: pin=%u leds=%u\n", i, pin, leds);

    // Yield every 10 floors to prevent watchdog issues on large buildings
    if (i % 10 == 9) delay(1);
  }

  // Setup I2C
  Wire.begin(_cfg.i2cAddress);
  Wire.onReceive(_onI2CRecv);

  // Register built-in animations
  registerAnimation(ANIM_FLOOR_SWEEP_UP,   _animFloorSweepUp,   200, false);
  registerAnimation(ANIM_FLOOR_SWEEP_DOWN, _animFloorSweepDown, 200, false);
  registerAnimation(ANIM_FLOOR_BOUNCE,     _animFloorBounce,    200, true);
  registerAnimation(ANIM_ROOM_SWEEP,       _animRoomSweep,      400, false);
  registerAnimation(ANIM_CROSSFADE,        _animCrossfade,      30,  true);
  registerAnimation(ANIM_RANDOM_80,        _animRandom80,       50,  true);
  registerAnimation(ANIM_BREATHE_ALL,      _animBreatheAll,     30,  true);
  registerAnimation(ANIM_FLOOR_CHASE,      _animFloorChase,     150, true);

  _lastCmdTime = millis();

  BLED_PRINTF("[BLED] Ready. I2C=0x%02X Floors=%u\n", _cfg.i2cAddress, _cfg.floorCount);
}

void BuildingLED::update() {
  // Process packets
  BLEDPacket pkt;
  if (_readPacket(pkt)) {
    _handlePacket(pkt);
  }

  // Legacy single-byte support
  if (_hasLegacy) {
    uint8_t c;
    noInterrupts();
    c = _legacyCmd;
    _hasLegacy = false;
    interrupts();
    
    // Map legacy bytes to standard commands
    BLEDPacket lp = {0, 0, 0, 0, 0};
    switch (c) {
      case 0x00: lp.cmd = CMD_ALL_OFF; break;
      case 0x01: lp.cmd = CMD_PATTERN; lp.a = _cfg.idleAnimationId; break;
      case 0x04: lp.cmd = CMD_ALL_ON; break;
      default: lp.cmd = c; break;
    }
    _handlePacket(lp);
  }

  // Tick animation
  _tickAnimation();

  // Check idle timeout
  _checkIdle();

  delay(1);
}

/* ═══════════════════════ I2C ═══════════════════════ */

void BuildingLED::_onI2CRecv(int howMany) {
  if (!_instance) return;

  uint8_t buf[16];
  int n = 0;
  while (Wire.available() && n < (int)sizeof(buf)) {
    buf[n++] = (uint8_t)Wire.read();
  }

  // Try to find 8-byte framed packet
  if (n >= BLED_PKT_SIZE) {
    for (int i = 0; i <= n - BLED_PKT_SIZE; i++) {
      if (buf[i] == BLED_PKT_HEADER && buf[i + 7] == BLED_PKT_FOOTER) {
        uint8_t sum = (uint8_t)(buf[i+1] + buf[i+2] + buf[i+3] + buf[i+4]);
        if (sum == buf[i + 6]) {
          for (int k = 0; k < BLED_PKT_SIZE; k++) {
            _instance->_isrRaw[k] = buf[i + k];
          }
          _instance->_hasPkt = true;
          return;
        }
      }
    }
  }
  
  // Fallback: single-byte legacy command
  if (n == 1) {
    _instance->_legacyCmd = buf[0];
    _instance->_hasLegacy = true;
  }
}

bool BuildingLED::_readPacket(BLEDPacket& pkt) {
  if (!_hasPkt) return false;

  uint8_t raw[BLED_PKT_SIZE];
  noInterrupts();
  for (uint8_t i = 0; i < BLED_PKT_SIZE; i++) raw[i] = _isrRaw[i];
  _hasPkt = false;
  interrupts();

  pkt.cmd = raw[1];
  pkt.a   = raw[2];
  pkt.b   = raw[3];
  pkt.c   = raw[4];
  pkt.d   = raw[5];

  BLED_PRINTF("[PKT] cmd=0x%02X a=%u b=%u c=%u d=%u\n", pkt.cmd, pkt.a, pkt.b, pkt.c, pkt.d);
  return true;
}

void BuildingLED::_handlePacket(const BLEDPacket& pkt) {
  resetIdleTimer();

  switch (pkt.cmd) {
    case CMD_PATTERN:
      startAnimation(pkt.a ? pkt.a : _cfg.idleAnimationId);
      break;

    case CMD_ALL_ON:
      stopAnimation();
      allOn();
      break;

    case CMD_ALL_OFF:
      stopAnimation();
      allOff();
      break;

    case CMD_KEEPALIVE:
      // Just reset idle timer (already done above)
      BLED_DBGFLN("[KEEPALIVE]");
      break;

    case CMD_SET_UNIT:
      stopAnimation();
      if (pkt.a < _cfg.floorCount && pkt.b > 0) {
        uint32_t col;
        switch (pkt.c) {
          case 0: col = _cfg.colors.off; break;
          case 1: col = _cfg.colors.primary; break;
          case 2: col = rgb(0, 255, 0); break;    // available
          case 3: col = rgb(255, 0, 0); break;    // blocked
          case 4: col = rgb(0, 90, 255); break;   // sold
          default: col = _cfg.colors.primary; break;
        }
        setSegment(pkt.a, pkt.b - 1, col);  // Convert 1-based to 0-based
        showFloor(pkt.a);
      }
      break;

    case CMD_PRESET:
      stopAnimation();
      applyPreset(pkt.a);
      break;

    case CMD_ROOM:
      stopAnimation();
      lightRoom(pkt.a, _cfg.colors.primary);
      showAll();
      break;

    default:
      // Forward to user handler
      if (_cmdHandler) {
        _cmdHandler(*this, pkt);
      } else {
        BLED_PRINTF("[PKT] Unhandled cmd=0x%02X\n", pkt.cmd);
      }
      break;
  }
}

/* ═══════════════════════ LAYOUT HELPERS ═══════════════════════ */

const FloorLayout& BuildingLED::_getLayout(uint8_t floorIdx) {
  uint8_t li = _cfg.floors[floorIdx].layoutIndex;
  return _cfg.layouts[li];
}

const SegmentDef& BuildingLED::_getSegment(uint8_t floorIdx, uint8_t segIdx) {
  const FloorLayout& layout = _getLayout(floorIdx);
  return layout.segments[segIdx];
}

/* ═══════════════════════ SEGMENT HELPERS ═══════════════════════ */

bool BuildingLED::getSegmentRange(uint8_t floorIdx, uint8_t segIdx, uint16_t& outStart, uint16_t& outLen) {
  if (floorIdx >= _cfg.floorCount) return false;
  
  const FloorLayout& layout = _getLayout(floorIdx);
  if (segIdx >= layout.segmentCount) return false;

  outStart = 0;
  for (uint8_t i = 0; i < segIdx; i++) {
    outStart += layout.segments[i].ledCount;
  }
  outLen = layout.segments[segIdx].ledCount;
  return true;
}

uint8_t BuildingLED::getSegmentCount(uint8_t floorIdx) {
  if (floorIdx >= _cfg.floorCount) return 0;
  return _getLayout(floorIdx).segmentCount;
}

SegmentType BuildingLED::getSegmentType(uint8_t floorIdx, uint8_t segIdx) {
  return _getSegment(floorIdx, segIdx).type;
}

uint8_t BuildingLED::getSegmentRoom(uint8_t floorIdx, uint8_t segIdx) {
  return _getSegment(floorIdx, segIdx).roomNumber;
}

uint8_t BuildingLED::findSegmentByRoom(uint8_t floorIdx, uint8_t roomNumber) {
  if (floorIdx >= _cfg.floorCount) return 255;
  const FloorLayout& layout = _getLayout(floorIdx);
  for (uint8_t i = 0; i < layout.segmentCount; i++) {
    if (layout.segments[i].roomNumber == roomNumber) return i;
  }
  return 255;
}

/* ═══════════════════════ LED CONTROL ═══════════════════════ */

void BuildingLED::setSegment(uint8_t floorIdx, uint8_t segIdx, uint32_t color) {
  uint16_t start, len;
  if (!getSegmentRange(floorIdx, segIdx, start, len)) return;
  for (uint16_t i = 0; i < len; i++) {
    _strips[floorIdx]->setPixelColor(start + i, color);
  }
}

void BuildingLED::setSegmentScaled(uint8_t floorIdx, uint8_t segIdx, uint32_t baseColor, uint8_t brightness) {
  uint16_t start, len;
  if (!getSegmentRange(floorIdx, segIdx, start, len)) return;
  uint32_t col = scaleColor(baseColor, brightness);
  for (uint16_t i = 0; i < len; i++) {
    _strips[floorIdx]->setPixelColor(start + i, col);
  }
}

void BuildingLED::setSegmentsByType(uint8_t floorIdx, SegmentType type, uint32_t color) {
  if (floorIdx >= _cfg.floorCount) return;
  const FloorLayout& layout = _getLayout(floorIdx);
  for (uint8_t s = 0; s < layout.segmentCount; s++) {
    if (layout.segments[s].type == type) {
      setSegment(floorIdx, s, color);
    }
  }
}

void BuildingLED::setSegmentsByTypeAll(SegmentType type, uint32_t color) {
  for (uint8_t f = 0; f < _cfg.floorCount; f++) {
    setSegmentsByType(f, type, color);
  }
}

void BuildingLED::lightRoom(uint8_t roomNumber, uint32_t color) {
  clearAll();
  for (uint8_t f = 0; f < _cfg.floorCount; f++) {
    uint8_t segIdx = findSegmentByRoom(f, roomNumber);
    if (segIdx != 255) {
      setSegment(f, segIdx, color);
    }
    applyFixedSegments(f);
  }
}

void BuildingLED::lightRooms(const uint8_t* roomNumbers, uint8_t count, uint32_t color) {
  clearAll();
  for (uint8_t f = 0; f < _cfg.floorCount; f++) {
    for (uint8_t r = 0; r < count; r++) {
      uint8_t segIdx = findSegmentByRoom(f, roomNumbers[r]);
      if (segIdx != 255) {
        setSegment(f, segIdx, color);
      }
    }
    applyFixedSegments(f);
  }
}

void BuildingLED::setFloor(uint8_t floorIdx, uint32_t color) {
  if (floorIdx >= _cfg.floorCount) return;
  uint16_t leds = _getLayout(floorIdx).totalLeds;
  for (uint16_t i = 0; i < leds; i++) {
    _strips[floorIdx]->setPixelColor(i, color);
  }
}

void BuildingLED::setFloorScaled(uint8_t floorIdx, uint32_t baseColor, uint8_t brightness) {
  if (floorIdx >= _cfg.floorCount) return;
  uint32_t col = scaleColor(baseColor, brightness);
  uint16_t leds = _getLayout(floorIdx).totalLeds;
  for (uint16_t i = 0; i < leds; i++) {
    _strips[floorIdx]->setPixelColor(i, col);
  }
}

void BuildingLED::applyFixedSegments(uint8_t floorIdx) {
  if (floorIdx >= _cfg.floorCount) return;
  const FloorLayout& layout = _getLayout(floorIdx);
  
  bool isRefuge = isRefugeFloor(floorIdx);
  
  for (uint8_t s = 0; s < layout.segmentCount; s++) {
    SegmentType type = layout.segments[s].type;
    
    if (type == SEG_FIXED) {
      setSegment(floorIdx, s, _cfg.colors.fixed);
    }
    else if (type == SEG_REFUGE && isRefuge) {
      setSegment(floorIdx, s, _cfg.colors.refuge);
    }
    else if (type == SEG_DUCT) {
      setSegment(floorIdx, s, _cfg.colors.duct);
    }
  }
}

void BuildingLED::applyFixedSegmentsAll() {
  for (uint8_t f = 0; f < _cfg.floorCount; f++) {
    applyFixedSegments(f);
  }
}

void BuildingLED::applyRefugeFloors() {
  for (uint8_t f = 0; f < _cfg.floorCount; f++) {
    if (isRefugeFloor(f)) {
      // Override all room segments with refuge color
      const FloorLayout& layout = _getLayout(f);
      for (uint8_t s = 0; s < layout.segmentCount; s++) {
        if (layout.segments[s].type == SEG_ROOM) {
          setSegment(f, s, _cfg.colors.refuge);
        }
      }
    }
  }
}

/* ═══════════════════════ BULK OPERATIONS ═══════════════════════ */

void BuildingLED::allOn() {
  BLED_DBGFLN("[LED] allOn");
  for (uint8_t f = 0; f < _cfg.floorCount; f++) {
    const FloorLayout& layout = _getLayout(f);
    bool isRefuge = isRefugeFloor(f);
    
    for (uint8_t s = 0; s < layout.segmentCount; s++) {
      SegmentType type = layout.segments[s].type;
      uint32_t col;
      
      switch (type) {
        case SEG_ROOM:
          col = isRefuge ? _cfg.colors.refuge : _cfg.colors.primary;
          break;
        case SEG_STAIRCASE:
          col = _cfg.colors.staircase;
          break;
        case SEG_DUCT:
          col = _cfg.colors.duct;
          break;
        case SEG_REFUGE:
          col = _cfg.colors.refuge;
          break;
        case SEG_LOBBY:
        case SEG_SHOP:
          col = _cfg.colors.shop;
          break;
        case SEG_FIXED:
          col = _cfg.colors.fixed;
          break;
        default:
          col = _cfg.colors.primary;
          break;
      }
      setSegment(f, s, col);
    }
  }
  showAll();
}

void BuildingLED::allOff() {
  BLED_DBGFLN("[LED] allOff");
  clearAll();
  showAll();
}

void BuildingLED::clearFloor(uint8_t floorIdx) {
  if (floorIdx < _cfg.floorCount) _strips[floorIdx]->clear();
}

void BuildingLED::clearAll() {
  for (uint8_t f = 0; f < _cfg.floorCount; f++) _strips[f]->clear();
}

void BuildingLED::showFloor(uint8_t floorIdx) {
  if (floorIdx < _cfg.floorCount) _strips[floorIdx]->show();
}

void BuildingLED::showAll() {
  for (uint8_t f = 0; f < _cfg.floorCount; f++) {
    _strips[f]->show();
    // Yield every 10 floors on large buildings
    if (f % 10 == 9) delayMicroseconds(50);
  }
}

/* ═══════════════════════ COLOR HELPERS ═══════════════════════ */

uint32_t BuildingLED::rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t BuildingLED::scaleColor(uint32_t color, uint8_t brightness) {
  uint8_t r = ((color >> 16) & 0xFF);
  uint8_t g = ((color >> 8) & 0xFF);
  uint8_t b = (color & 0xFF);
  r = (uint8_t)(((uint16_t)r * brightness) >> 8);
  g = (uint8_t)(((uint16_t)g * brightness) >> 8);
  b = (uint8_t)(((uint16_t)b * brightness) >> 8);
  return rgb(r, g, b);
}

uint32_t BuildingLED::colorForType(SegmentType type) {
  switch (type) {
    case SEG_ROOM:      return _cfg.colors.primary;
    case SEG_STAIRCASE: return _cfg.colors.staircase;
    case SEG_DUCT:      return _cfg.colors.duct;
    case SEG_REFUGE:    return _cfg.colors.refuge;
    case SEG_LOBBY:     return _cfg.colors.primary;
    case SEG_SHOP:      return _cfg.colors.shop;
    case SEG_FIXED:     return _cfg.colors.fixed;
    default:            return _cfg.colors.primary;
  }
}

bool BuildingLED::isRefugeFloor(uint8_t floorIdx) {
  if (floorIdx >= _cfg.floorCount) return false;
  return _cfg.floors[floorIdx].type == FLOOR_REFUGE;
}

Adafruit_NeoPixel* BuildingLED::getStrip(uint8_t floorIdx) {
  if (floorIdx >= _cfg.floorCount) return nullptr;
  return _strips[floorIdx];
}

FloorType BuildingLED::getFloorType(uint8_t floorIdx) {
  if (floorIdx >= _cfg.floorCount) return FLOOR_RESIDENTIAL;
  return _cfg.floors[floorIdx].type;
}

uint16_t BuildingLED::getFloorLedCount(uint8_t floorIdx) {
  if (floorIdx >= _cfg.floorCount) return 0;
  return _getLayout(floorIdx).totalLeds;
}

/* ═══════════════════════ ANIMATION SYSTEM ═══════════════════════ */

void BuildingLED::registerAnimation(uint8_t id, AnimTickFn fn, uint16_t frameMs, bool loop) {
  if (id >= BLED_MAX_ANIMATIONS) return;
  _anims[id].fn = fn;
  _anims[id].frameMs = frameMs;
  _anims[id].loop = loop;
  _anims[id].registered = true;
}

void BuildingLED::startAnimation(uint8_t id) {
  if (id >= BLED_MAX_ANIMATIONS || !_anims[id].registered) {
    BLED_PRINTF("[ANIM] Invalid id=%u\n", id);
    return;
  }
  
  stopAnimation();
  _animActive = true;
  _animId = id;
  _animStartTime = millis();
  _animLastFrame = 0;
  _animStep = 0;

  BLED_PRINTF("[ANIM] Start id=%u\n", id);
}

void BuildingLED::stopAnimation() {
  if (_animActive) {
    BLED_PRINTF("[ANIM] Stop id=%u\n", _animId);
  }
  _animActive = false;
  _animId = ANIM_NONE;
  _animStep = 0;
}

void BuildingLED::_tickAnimation() {
  if (!_animActive) return;
  
  uint32_t now = millis();
  uint16_t frameMs = _anims[_animId].frameMs;
  
  if (now - _animLastFrame < frameMs) return;
  _animLastFrame = now;

  uint32_t elapsed = now - _animStartTime;
  bool done = _anims[_animId].fn(*this, elapsed, _animStep);
  _animStep++;

  if (done) {
    if (_anims[_animId].loop) {
      // Restart
      _animStartTime = now;
      _animStep = 0;
    } else {
      stopAnimation();
    }
  }
}

/* ═══════════════════════ PRESET SYSTEM ═══════════════════════ */

void BuildingLED::registerPreset(uint8_t id, PresetFn fn) {
  if (id >= BLED_MAX_PRESETS) return;
  _presets[id].fn = fn;
  _presets[id].registered = true;
}

void BuildingLED::applyPreset(uint8_t id) {
  if (id >= BLED_MAX_PRESETS || !_presets[id].registered) {
    BLED_PRINTF("[PRESET] Invalid id=%u\n", id);
    return;
  }
  stopAnimation();
  _presets[id].fn(*this);
  BLED_PRINTF("[PRESET] Applied id=%u\n", id);
}

/* ═══════════════════════ COMMAND HANDLER ═══════════════════════ */

void BuildingLED::onCommand(CommandHandler handler) {
  _cmdHandler = handler;
}

/* ═══════════════════════ IDLE TIMEOUT ═══════════════════════ */

void BuildingLED::resetIdleTimer() {
  _lastCmdTime = millis();
  _idleTriggered = false;
}

void BuildingLED::setIdleTimeout(uint32_t ms) {
  // Note: modifying config requires a non-const copy pattern
  // For simplicity, we store idle override locally
  // This is a design limitation - for now use config
}

void BuildingLED::setIdleAnimation(uint8_t animId) {
  // Same note as above
}

void BuildingLED::_checkIdle() {
  if (_cfg.idleTimeoutMs == 0) return;
  if (_idleTriggered) return;
  if (_animActive) return;

  uint32_t now = millis();
  // Handle millis() overflow
  if (now < _lastCmdTime) { _lastCmdTime = now; return; }

  if ((now - _lastCmdTime) >= _cfg.idleTimeoutMs) {
    BLED_DBGFLN("[IDLE] Timeout - starting auto animation");
    startAnimation(_cfg.idleAnimationId);
    _idleTriggered = true;
  }
}

/* ═══════════════════════ BUILT-IN ANIMATIONS ═══════════════════════ */

// --- Floor Sweep Up: Light floors one by one from bottom to top ---
bool BuildingLED::_animFloorSweepUp(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  uint8_t fc = b.getFloorCount();
  uint8_t totalSteps = fc * 2; // on phase + clear phase

  if (step < fc) {
    // Lighting up phase
    b.setFloor(step, b.colors().primary);
    b.applyFixedSegments(step);
    b.showFloor(step);
  } else if (step == fc) {
    // Hold briefly, then clear
    delay(200);
    b.clearAll();
    b.showAll();
  } else {
    return true; // done
  }
  return false;
}

// --- Floor Sweep Down ---
bool BuildingLED::_animFloorSweepDown(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  uint8_t fc = b.getFloorCount();

  if (step < fc) {
    uint8_t f = fc - 1 - step;
    b.setFloor(f, b.colors().primary);
    b.applyFixedSegments(f);
    b.showFloor(f);
  } else if (step == fc) {
    delay(200);
    b.clearAll();
    b.showAll();
  } else {
    return true;
  }
  return false;
}

// --- Floor Bounce: Sweep up, hold, sweep down ---
bool BuildingLED::_animFloorBounce(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  uint8_t fc = b.getFloorCount();
  uint8_t totalSteps = fc * 2 + 2; // up + clear + down + clear

  if (step < fc) {
    // Sweep up
    b.setFloor(step, b.colors().primary);
    b.applyFixedSegments(step);
    b.showFloor(step);
  } else if (step == fc) {
    // Clear
    b.clearAll();
    b.showAll();
  } else if (step < fc * 2 + 1) {
    // Sweep down
    uint8_t f = fc * 2 - step;
    b.setFloor(f, b.colors().primary);
    b.applyFixedSegments(f);
    b.showFloor(f);
  } else if (step == fc * 2 + 1) {
    b.clearAll();
    b.showAll();
  } else {
    return true; // done (will loop)
  }
  return false;
}

// --- Room Sweep: Light each room type across all floors ---
bool BuildingLED::_animRoomSweep(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  // Find max room number across all layouts
  uint8_t maxRoom = 0;
  for (uint8_t f = 0; f < b.getFloorCount(); f++) {
    uint8_t sc = b.getSegmentCount(f);
    for (uint8_t s = 0; s < sc; s++) {
      uint8_t rn = b.getSegmentRoom(f, s);
      if (rn > maxRoom) maxRoom = rn;
    }
  }
  
  if (maxRoom == 0) return true;

  uint8_t roomIdx = (step % (maxRoom + 1)); // 0 = clear, 1..maxRoom = rooms
  
  if (roomIdx == 0) {
    b.clearAll();
    b.showAll();
  } else {
    b.lightRoom(roomIdx, b.colors().primary);
    b.showAll();
  }

  return (step >= (uint16_t)(maxRoom + 1));
}

// --- Crossfade: One floor lit at a time with smooth crossfade ---
bool BuildingLED::_animCrossfade(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  uint8_t fc = b.getFloorCount();
  if (fc == 0) return true;

  const uint16_t HOLD_FRAMES = 15;  // ~450ms hold at 30ms/frame
  const uint16_t FADE_FRAMES = 8;   // ~240ms fade
  const uint16_t TOTAL_FRAMES = HOLD_FRAMES + FADE_FRAMES;

  uint16_t cycleStep = step % TOTAL_FRAMES;
  uint8_t curFloor = (step / TOTAL_FRAMES) % fc;
  uint8_t nextFloor = (curFloor + 1) % fc;

  b.clearAll();

  if (cycleStep < HOLD_FRAMES) {
    // Hold current floor at full brightness
    b.setFloor(curFloor, b.colors().primary);
    b.applyFixedSegments(curFloor);
  } else {
    // Crossfade
    uint8_t fadeProgress = cycleStep - HOLD_FRAMES;
    uint8_t outBright = 255 - (fadeProgress * 255 / FADE_FRAMES);
    uint8_t inBright = fadeProgress * 255 / FADE_FRAMES;

    b.setFloorScaled(curFloor, b.colors().primary, outBright);
    b.setFloorScaled(nextFloor, b.colors().primary, inBright);
  }

  b.applyFixedSegmentsAll();
  b.showAll();

  return false; // Loops forever
}

// --- Random 80%: 80% of floors randomly lit, smooth fading ---
bool BuildingLED::_animRandom80(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  // Use static arrays - safe for single instance
  static uint8_t cur[BLED_MAX_FLOORS];
  static uint8_t tgt[BLED_MAX_FLOORS];
  static uint16_t lastReseed = 0;

  uint8_t fc = b.getFloorCount();
  const uint16_t RESEED_EVERY = 25;
  const uint8_t FADE_DIVISOR = 16;

  if (step == 0) {
    memset(cur, 0, sizeof(cur));
    memset(tgt, 0, sizeof(tgt));
    lastReseed = 0;
  }

  // Pick new targets periodically
  if ((uint16_t)(step - lastReseed) >= RESEED_EVERY) {
    lastReseed = step;
    for (uint8_t i = 0; i < fc; i++) {
      if (random(100) < 80) {
        tgt[i] = random(60, 256);
      } else {
        tgt[i] = 0;
      }
    }
  }

  // Smooth chase
  for (uint8_t f = 0; f < fc; f++) {
    int16_t delta = (int16_t)tgt[f] - (int16_t)cur[f];
    cur[f] = (uint8_t)((int16_t)cur[f] + delta / FADE_DIVISOR);

    if (cur[f] == 0) {
      b.setFloor(f, 0);
    } else {
      b.setFloorScaled(f, b.colors().primary, cur[f]);
    }
    
    if (b.isRefugeFloor(f) && cur[f] > 0) {
      b.setFloor(f, b.scaleColor(b.colors().refuge, cur[f]));
    }
  }

  b.showAll();
  return false; // Loops forever
}

// --- Breathe All: All LEDs pulse in unison ---
bool BuildingLED::_animBreatheAll(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  // Cosine-based breathing, period ~3 seconds at 30ms/frame = 100 frames
  const uint16_t PERIOD = 100;
  float phase = (float)(step % PERIOD) / (float)PERIOD;
  float val = 0.5f * (1.0f - cosf(phase * 2.0f * 3.14159f));
  uint8_t bright = (uint8_t)(30 + val * 225); // Range 30-255

  for (uint8_t f = 0; f < b.getFloorCount(); f++) {
    if (b.isRefugeFloor(f)) {
      b.setFloorScaled(f, b.colors().refuge, bright);
    } else {
      b.setFloorScaled(f, b.colors().primary, bright);
    }
  }
  b.applyFixedSegmentsAll();
  b.showAll();

  return false; // Loops forever
}

// --- Floor Chase: Single lit floor moving like a scanner ---
bool BuildingLED::_animFloorChase(BuildingLED& b, uint32_t elapsed, uint16_t step) {
  uint8_t fc = b.getFloorCount();
  if (fc == 0) return true;

  // Bounce: 0..fc-1, fc-2..1
  uint16_t cycle = (fc - 1) * 2;
  uint16_t pos = step % cycle;
  uint8_t activeFloor = (pos < fc) ? pos : (cycle - pos);

  b.clearAll();
  b.setFloor(activeFloor, b.colors().primary);
  b.applyFixedSegments(activeFloor);
  
  // Add dim trail on adjacent floors
  if (activeFloor > 0) {
    b.setFloorScaled(activeFloor - 1, b.colors().primary, 60);
  }
  if (activeFloor < fc - 1) {
    b.setFloorScaled(activeFloor + 1, b.colors().primary, 60);
  }

  b.showAll();
  return false; // Loops forever
}
