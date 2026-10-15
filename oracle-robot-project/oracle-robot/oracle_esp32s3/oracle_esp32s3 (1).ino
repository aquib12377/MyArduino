/*
 * ============================================================================
 *  ORACLE - Optical & Robotic Assistant for Curiosity, Learning and Exploration
 *  EQUINOX 2026 answering robot  |  ESP32-S3 firmware  v1.0.0
 * ============================================================================
 *  Hardware : ESP32-S3 (PSRAM strongly recommended) + INMP441 mic + MAX98357A amp
 *             + 2 x SSD1306 OLED "eyes" (one per I2C bus) + L298N + 2 DC motors
 *             + push-to-talk button + optional front bumper switch
 *             + 2 x hobby servos (left/right arm) that wave when the robot says hello
 *  Software : Arduino-ESP32 core 2.0.x or 3.x
 *  Libraries: "WebSockets" by Markus Sattler (>= 2.4), "U8g2" by olikraus
 *  Board    : "ESP32S3 Dev Module"; PSRAM = OPI PSRAM for N8R8/N16R8 modules
 *             (QSPI PSRAM for R2 modules); Flash size = your module's size.
 *
 *  How it works
 *    Hold the button and speak  -> mic audio is streamed to the bridge (laptop).
 *    Release                    -> bridge does speech-to-text, picks a scripted
 *                                  answer or asks ChatGPT/DeepSeek, then sends
 *                                  speech audio back.  The whole answer is buffered
 *                                  in RAM first (no Wi-Fi jitter while speaking),
 *                                  then played through the MAX98357A.
 *    Quick tap                  -> robot introduces itself (greeting).
 *    Press while it talks       -> it stops and listens again.
 *
 *  Protocol (text frames unless noted)
 *    robot -> bridge : HELLO:<bufBytes>:<rate>  PTT:1  PTT:0  <binary PCM16 16 kHz mic>
 *                      PLAYED:OK|ERR  BUMP
 *    bridge -> robot : STATE:<idle|thinking|speaking|happy|confused|error>
 *                      TTS_BEGIN:<bytes>  <binary PCM16 24 kHz mono>  TTS_END
 *                      MOVE:<F|B|L|R|S>:<ms>:<speed%>   VOL:<0-100>
 *                      WAVE  or  WAVE:<cycles>          (both arms wave, then lower)
 *
 *  Serial (115200) bench commands:  ? help | i info | e cycle eye mood | m mic meter
 *                                   t speaker beep | + - volume | f b l r s motor pulses | w wave
 * ============================================================================
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h>
#include <Preferences.h>
#include <Wire.h>
// U8g2 only compiles working Wire1 support for the "_2ND_HW_I2C" display classes when
// WIRE_INTERFACES_COUNT>1 is defined (see u8g2/cppsrc/U8x8lib.h). AVR/SAMD board files define
// this automatically; the ESP32 Arduino core never does, for any board - so without this line
// eyeR's begin()/read/write calls silently compile to no-ops, Wire1 never gets started, and the
// right eye always reports "NOT FOUND" no matter how it's wired. Must come before <U8g2lib.h>.
#define WIRE_INTERFACES_COUNT 2
#include <U8g2lib.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#endif
#include <math.h>

#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#define FW_VERSION "1.0.0"
#define LOG(tag, fmt, ...) Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
struct Debounced {
  int pin;
  bool raw;
  bool stable;
  uint32_t since;
};
enum State : uint8_t { ST_BOOT, ST_OFFLINE, ST_IDLE, ST_RECORDING, ST_WAITING, ST_RECEIVING, ST_PLAYING };
static const char *const STATE_NAMES[] = {"BOOT", "OFFLINE", "IDLE", "RECORDING", "WAITING", "RECEIVING", "PLAYING"};
enum Mood : uint8_t { M_BOOT, M_IDLE, M_LISTEN, M_THINK, M_SPEAK, M_HAPPY, M_CONFUSED, M_ERROR, M_SAD, M_SLEEP, M_COUNT };
static const char *const MOOD_NAMES[] = {"boot", "idle", "listen", "think", "speak", "happy", "confused", "error", "sad", "sleep"};
enum RxMode : uint8_t { RX_NONE, RX_TTS, RX_CLIP };

// ============================================================================
//  USER CONFIG
// ============================================================================
static const char *WIFI_SSID_1 = "Vrushank_2.4Ghz";   // 2.4 GHz ONLY - the ESP32-S3 has no 5 GHz radio
static const char *WIFI_PASS_1 = "vrushank1909";
static const char *WIFI_SSID_2 = "";                          // optional backup network (leave "" to disable)
static const char *WIFI_PASS_2 = "";
static const char *BRIDGE_HOST = "192.168.1.51";
static const uint16_t BRIDGE_PORT = 8765;

#define EYES_64X48 1      // 1 = 0.66" 64x48 SSD1306 modules ("0.6 inch"); 0 = 0.96" 128x64 SSD1306
constexpr uint8_t OLED_ADDR_LEFT = 0x3C;                      // 7-bit I2C addresses (each eye has its own bus)
constexpr uint8_t OLED_ADDR_RIGHT = 0x3C;

constexpr bool INVERT_LEFT_MOTOR = false;                     // flip if a wheel turns the wrong way
constexpr bool INVERT_RIGHT_MOTOR = false;

constexpr bool INVERT_LEFT_SERVO = false;                     // arm servos are mirror-mounted: flip a side if its arm lifts the wrong way
constexpr bool INVERT_RIGHT_SERVO = true;
constexpr int ARM_DOWN_DEG = 10;                              // servo angle with the arm hanging down (0..180)
constexpr int ARM_UP_DEG = 120;                               // servo angle with the arm raised for waving
constexpr int WAVE_SWING_DEG = 25;                            // +/- swing around ARM_UP_DEG while waving

// ============================================================================
//  PINS  (ESP32-S3-DevKitC-1 N16R8 safe set: avoids 0,3,45,46 strapping; 19,20 USB;
//         26-37 flash/PSRAM; 43,44 UART0; 38/48 on-board RGB LED)
// ============================================================================
constexpr int PIN_MIC_SCK = 4;      // INMP441 SCK
constexpr int PIN_MIC_WS = 5;       // INMP441 WS
constexpr int PIN_MIC_SD = 6;       // INMP441 SD   (L/R pin -> GND = left channel)
constexpr int PIN_SPK_BCLK = 15;    // MAX98357A BCLK
constexpr int PIN_SPK_LRC = 16;     // MAX98357A LRC
constexpr int PIN_SPK_DIN = 7;      // MAX98357A DIN
constexpr int PIN_SPK_SD = 17;      // MAX98357A SD  (HIGH = on, LOW = shutdown: kills idle hiss)
constexpr int PIN_OLED_L_SDA = 8;   // left eye  -> Wire
constexpr int PIN_OLED_L_SCL = 9;
constexpr int PIN_OLED_R_SDA = 10;  // right eye -> Wire1
constexpr int PIN_OLED_R_SCL = 11;
constexpr int PIN_ENA = 12;         // L298N ENA (PWM)  - remove the ENA jumper on the board
constexpr int PIN_IN1 = 13;
constexpr int PIN_IN2 = 14;
constexpr int PIN_ENB = 21;         // L298N ENB (PWM)  - remove the ENB jumper on the board
constexpr int PIN_IN3 = 47;
constexpr int PIN_IN4 = 42;
constexpr int PIN_BUTTON = 1;       // push-to-talk, other leg to GND (internal pull-up)
constexpr int PIN_BUMPER = 2;       // normally-open microswitch to GND (internal pull-up)
constexpr int PIN_SERVO_L = 46;     // left arm servo signal  - power the servos from their OWN 5V supply, common GND
constexpr int PIN_SERVO_R = 3;      // right arm servo signal (3 and 46 are strapping pins: fine for a servo signal, which only
                                    //  listens; never add a pull-up on them or a driver that pulls them the wrong way at reset)

// ============================================================================
//  TUNING
// ============================================================================
constexpr uint32_t MIC_RATE = 16000;
constexpr uint32_t SPK_RATE = 24000;              // streamed audio from the bridge (OpenAI 'pcm' output)
constexpr uint32_t CLIP_RATE = 16000;             // stored clips: 16 kHz IMA ADPCM, 4:1 (see bridge/adpcm.py)
constexpr size_t ADPCM_BLOCK = 256;               // 4-byte header + 252 data bytes
constexpr size_t ADPCM_SAMPLES = 505;             // samples decoded from one block
constexpr size_t CLIP_ID_LEN = 12;
constexpr int MIC_SHIFT = 14;                     // 32-bit slot -> int16. 16 = unity, 14 = +12 dB (INMP441 is quiet)
constexpr size_t MIC_READ_FRAMES = 256;           // 16 ms per read
constexpr size_t MIC_PACKET_SAMPLES = 1024;       // one WebSocket frame = 64 ms
constexpr uint32_t MAX_REC_MS = 15000;
constexpr size_t PLAY_CHUNK_FRAMES = 512;         // 21 ms per playback step
constexpr uint32_t RX_STALL_MS = 150000;            // no audio bytes for this long -> abort download
constexpr uint32_t WAIT_TIMEOUT_MS = 40000;       // bridge never answered
constexpr uint32_t WAIT_AFTER_PLAY_MS = 10000;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t EYE_FRAME_MS = 50;
constexpr uint32_t EYE_SLEEP_MS = 10UL * 60UL * 1000UL;   // eyes go to sleep after 10 idle minutes (OLED wear)
constexpr uint32_t WIFI_RETRY_MS = 20000;
constexpr uint8_t DEFAULT_VOLUME = 95;

constexpr uint32_t PWM_FREQ = 8000;
constexpr uint8_t PWM_BITS = 8;
constexpr int MOTOR_MIN_PWM = 90;                 // below this the wheels stall
constexpr int MOTOR_MAX_PWM = 200;                // hard speed cap (3 ft robot: tipping!)
constexpr int RAMP_STEP = 8;                      // PWM counts per 10 ms -> ~0.25 s to full speed
constexpr uint32_t MOVE_MAX_MS = 1500;            // no single command may run longer than this

constexpr uint32_t SERVO_FREQ = 50;               // standard hobby servo: 20 ms frame
constexpr uint8_t SERVO_BITS = 14;
constexpr int SERVO_MIN_US = 500;                 // pulse width at 0 deg / 180 deg (narrow these if a servo buzzes at the ends)
constexpr int SERVO_MAX_US = 2500;
constexpr uint32_t WAVE_RAISE_MS = 450;
constexpr uint32_t WAVE_PERIOD_MS = 400;          // one left-right swing
constexpr uint32_t WAVE_LOWER_MS = 550;
constexpr uint8_t WAVE_DEFAULT_CYCLES = 3;
constexpr uint8_t WAVE_MAX_CYCLES = 10;
constexpr uint32_t SERVO_RELEASE_MS = 500;        // after the arms are down the PWM stops: no buzzing, no idle current

// ============================================================================
//  STATE
// ============================================================================

static State g_state = ST_BOOT;
static Mood g_mood = M_BOOT;
static uint32_t g_moodHoldUntil = 0;
static uint32_t g_lastActivity = 0;
static uint32_t g_waitDeadline = 0;

static WiFiMulti wifiMulti;
static WebSocketsClient ws;
static Preferences prefs;
static bool g_wsUp = false;
static bool g_bridgeReachable = false;

static bool g_micOk = false, g_spkOk = false;
static uint8_t g_volume = DEFAULT_VOLUME;
static float g_level = 0.0f;                      // 0..1 loudness of what is being spoken (drives the eyes)

static RxMode g_rxMode = RX_NONE;
static bool g_fsOk = false;
static File g_clipFile;                           // open while a clip is being uploaded or played
static char g_clipId[CLIP_ID_LEN + 4] = {0};
static size_t g_clipExpected = 0, g_clipPos = 0;
static bool g_playFromFlash = false;
static uint8_t g_blk[ADPCM_BLOCK];

static uint8_t *g_buf = nullptr;                  // downloaded speech
static size_t g_bufSize = 0;
static size_t g_rxExpected = 0, g_rxPos = 0;
static uint32_t g_rxLastMs = 0;
static size_t g_playPos = 0, g_playLen = 0;

static int16_t g_pkt[MIC_PACKET_SAMPLES];
static size_t g_pktLen = 0;
static uint32_t g_recStart = 0;
static int32_t g_recPeak = 0;
static size_t g_dropSamples = 0;
static float g_dcX = 0, g_dcY = 0;
static uint32_t g_micTimeouts = 0;

static inline int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void eyesTick(uint32_t now);
static void motorsStopNow();

// ============================================================================
//  STATE / MOOD HELPERS
// ============================================================================
static void setMood(Mood m, uint32_t holdMs = 0) {
  if (m != g_mood) LOG("STATE", "eyes: %s -> %s", MOOD_NAMES[g_mood], MOOD_NAMES[m]);
  g_mood = m;
  g_moodHoldUntil = holdMs ? millis() + holdMs : 0;
}

static void enterState(State s, int moodOverride = -1) {
  if (s != g_state) LOG("STATE", "%s -> %s", STATE_NAMES[g_state], STATE_NAMES[s]);
  g_state = s;
  if (moodOverride >= 0) {
    setMood((Mood)moodOverride);
  } else {
    switch (s) {
      case ST_OFFLINE:   setMood(M_SAD); break;
      case ST_IDLE:      if (!(g_moodHoldUntil && (int32_t)(millis() - g_moodHoldUntil) < 0)) setMood(M_IDLE); break;
      case ST_RECORDING: setMood(M_LISTEN); break;
      case ST_WAITING:
      case ST_RECEIVING: setMood(M_THINK); break;
      case ST_PLAYING:   setMood(M_SPEAK); break;
      default: break;
    }
  }
  if (s == ST_WAITING) g_waitDeadline = millis() + WAIT_TIMEOUT_MS;
  g_lastActivity = millis();
}

static const char *resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "power-on";
    case ESP_RST_SW:       return "software reset";
    case ESP_RST_PANIC:    return "PANIC (crash)";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "other watchdog";
    case ESP_RST_BROWNOUT: return "BROWNOUT - check the 5V supply / capacitors";
    case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
    default:               return "other";
  }
}

// ============================================================================
//  PWM wrappers (LEDC API differs between core 2.x and 3.x)
// ============================================================================
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void pwmInit(int pin, uint8_t) { ledcAttach(pin, PWM_FREQ, PWM_BITS); }
static void pwmWrite(int pin, uint8_t, uint32_t duty) { ledcWrite(pin, duty); }
#else
static void pwmInit(int pin, uint8_t ch) { ledcSetup(ch, PWM_FREQ, PWM_BITS); ledcAttachPin(pin, ch); }
static void pwmWrite(int, uint8_t ch, uint32_t duty) { ledcWrite(ch, duty); }
#endif

// ============================================================================
//  DEBOUNCED INPUTS
// ============================================================================

static Debounced btn = {PIN_BUTTON, false, false, 0};
static Debounced bump = {PIN_BUMPER, false, false, 0};

// returns true when the debounced state changed
static bool pollInput(Debounced &d, uint32_t now) {
  bool r = (digitalRead(d.pin) == LOW);
  if (r != d.raw) { d.raw = r; d.since = now; }
  if (r != d.stable && (now - d.since) >= DEBOUNCE_MS) { d.stable = r; return true; }
  return false;
}

// ============================================================================
//  MOTORS  (L298N)  - ramped, capped, auto-stopping
// ============================================================================
static int g_curL = 0, g_curR = 0, g_tgtL = 0, g_tgtR = 0;
static uint32_t g_moveUntil = 0, g_lastRamp = 0, g_lastBumpMsg = 0;

static void driveMotor(int in1, int in2, int en, uint8_t ch, int speed, bool invert) {
  if (invert) speed = -speed;
  if (speed > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (speed < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  else                { digitalWrite(in1, LOW);  digitalWrite(in2, LOW); }
  pwmWrite(en, ch, (uint32_t)imin(abs(speed), 255));
}

static void applyMotors() {
  driveMotor(PIN_IN1, PIN_IN2, PIN_ENA, 0, g_curL, INVERT_LEFT_MOTOR);
  driveMotor(PIN_IN3, PIN_IN4, PIN_ENB, 1, g_curR, INVERT_RIGHT_MOTOR);
}

static void motorsStopNow() {
  g_tgtL = g_tgtR = g_curL = g_curR = 0;
  applyMotors();
}

static int stepToward(int cur, int tgt) {
  if (cur < tgt) return imin(cur + RAMP_STEP, tgt);
  if (cur > tgt) return imax(cur - RAMP_STEP, tgt);
  return cur;
}

static void motorTick(uint32_t now) {
  if ((g_tgtL != 0 || g_tgtR != 0) && (int32_t)(now - g_moveUntil) >= 0) g_tgtL = g_tgtR = 0;   // command finished
  if (now - g_lastRamp < 10) return;
  g_lastRamp = now;
  if (bump.stable && g_tgtL > 0 && g_tgtR > 0) {                     // driving forward into something
    motorsStopNow();
    if (now - g_lastBumpMsg > 3000) {
      g_lastBumpMsg = now;
      LOG("WARN", "bumper hit while moving forward - stopped");
      if (g_wsUp) ws.sendTXT("BUMP");
    }
    return;
  }
  int nl = stepToward(g_curL, g_tgtL), nr = stepToward(g_curR, g_tgtR);
  if (nl != g_curL || nr != g_curR) { g_curL = nl; g_curR = nr; applyMotors(); }
}

static void moveCommand(char d, uint32_t ms, int pct, bool force) {
  if (d == 'S') { motorsStopNow(); LOG("INFO", "motors: stop"); return; }
  if (!force && g_state != ST_IDLE) { LOG("WARN", "move '%c' ignored (state %s)", d, STATE_NAMES[g_state]); return; }
  ms = (uint32_t)imin((int32_t)ms, (int32_t)MOVE_MAX_MS);
  pct = clampi(pct, 0, 100);
  if (ms == 0 || pct == 0) return;
  const int pwm = MOTOR_MIN_PWM + ((MOTOR_MAX_PWM - MOTOR_MIN_PWM) * pct) / 100;
  switch (d) {
    case 'F': if (bump.stable) { LOG("WARN", "forward refused: bumper is pressed"); return; } g_tgtL = pwm;  g_tgtR = pwm;  break;
    case 'B': g_tgtL = -pwm; g_tgtR = -pwm; break;
    case 'L': g_tgtL = -pwm; g_tgtR = pwm;  break;
    case 'R': g_tgtL = pwm;  g_tgtR = -pwm; break;
    default:  LOG("WARN", "unknown move '%c'", d); return;
  }
  g_moveUntil = millis() + ms;
  LOG("INFO", "motors: %c for %lu ms at pwm %d", d, (unsigned long)ms, pwm);
}

// ============================================================================
//  ARMS  (two hobby servos)  - non-blocking wave: raise -> swing N times -> lower.
//  The PWM is only running while the arms move; afterwards it is switched off so the
//  servos neither buzz nor draw current, and nothing twitches at boot.
// ============================================================================
constexpr uint8_t SERVO_CH_L = 2, SERVO_CH_R = 3;       // core 2.x LEDC channels (0/1 belong to the motors); core 3.x picks its own
constexpr uint32_t SERVO_PERIOD_US = 1000000UL / SERVO_FREQ;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void servoPwmInit(int pin, uint8_t) { ledcAttach(pin, SERVO_FREQ, SERVO_BITS); }
static void servoPwmWrite(int pin, uint8_t, uint32_t duty) { ledcWrite(pin, duty); }
#else
static void servoPwmInit(int pin, uint8_t ch) { ledcSetup(ch, SERVO_FREQ, SERVO_BITS); ledcAttachPin(pin, ch); }
static void servoPwmWrite(int, uint8_t ch, uint32_t duty) { ledcWrite(ch, duty); }
#endif

static bool g_waving = false;
static bool g_servoDriven = false;                      // PWM currently running
static uint8_t g_waveCycles = WAVE_DEFAULT_CYCLES;
static uint32_t g_waveStart = 0, g_servoReleaseAt = 0, g_servoLastStep = 0;

static void servoWrite(int pin, uint8_t ch, int deg, bool invert) {
  deg = clampi(deg, 0, 180);
  if (invert) deg = 180 - deg;
  const uint32_t us = SERVO_MIN_US + ((uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * deg) / 180;
  servoPwmWrite(pin, ch, (us << SERVO_BITS) / SERVO_PERIOD_US);
}

static void armsWrite(int deg) {
  servoWrite(PIN_SERVO_L, SERVO_CH_L, deg, INVERT_LEFT_SERVO);
  servoWrite(PIN_SERVO_R, SERVO_CH_R, deg, INVERT_RIGHT_SERVO);
  g_servoDriven = true;
}

static void armsRelease() {
  servoPwmWrite(PIN_SERVO_L, SERVO_CH_L, 0);
  servoPwmWrite(PIN_SERVO_R, SERVO_CH_R, 0);
  g_servoDriven = false;
}

static void waveStart(uint8_t cycles) {
  if (g_waving) return;                                 // already waving: do not restart mid-swing
  g_waveCycles = (uint8_t)clampi(cycles, 1, WAVE_MAX_CYCLES);
  g_waveStart = millis();
  g_servoLastStep = 0;
  g_waving = true;
  LOG("INFO", "arms: waving x%u", g_waveCycles);
}

// Interrupted (button pressed): skip the rest of the swinging and lower the arms.
static void waveCancel() {
  if (!g_waving) return;
  const uint32_t swingEnd = WAVE_RAISE_MS + (uint32_t)g_waveCycles * WAVE_PERIOD_MS;
  if (millis() - g_waveStart < swingEnd) g_waveStart = millis() - swingEnd;
}

static void servoTick(uint32_t now) {
  if (!g_waving) {
    if (g_servoDriven && (int32_t)(now - g_servoReleaseAt) >= 0) armsRelease();
    return;
  }
  if (now - g_servoLastStep < 15) return;               // ~66 updates/s is plenty for a servo
  g_servoLastStep = now;
  const uint32_t t = now - g_waveStart;
  const uint32_t swingMs = (uint32_t)g_waveCycles * WAVE_PERIOD_MS;
  float deg;
  if (t < WAVE_RAISE_MS) {
    const float k = (float)t / WAVE_RAISE_MS;
    deg = ARM_DOWN_DEG + (ARM_UP_DEG - ARM_DOWN_DEG) * (k * k * (3.0f - 2.0f * k));       // smoothstep
  } else if (t < WAVE_RAISE_MS + swingMs) {
    deg = ARM_UP_DEG + WAVE_SWING_DEG * sinf(2.0f * PI * (float)(t - WAVE_RAISE_MS) / WAVE_PERIOD_MS);
  } else if (t < WAVE_RAISE_MS + swingMs + WAVE_LOWER_MS) {
    const float k = (float)(t - WAVE_RAISE_MS - swingMs) / WAVE_LOWER_MS;
    deg = ARM_UP_DEG + (ARM_DOWN_DEG - ARM_UP_DEG) * (k * k * (3.0f - 2.0f * k));
  } else {
    armsWrite(ARM_DOWN_DEG);
    g_waving = false;
    g_servoReleaseAt = now + SERVO_RELEASE_MS;          // give the arms time to settle, then stop the PWM
    return;
  }
  armsWrite((int)(deg + 0.5f));
}

static void initServos() {
  servoPwmInit(PIN_SERVO_L, SERVO_CH_L);
  servoPwmInit(PIN_SERVO_R, SERVO_CH_R);
  armsRelease();                                        // duty 0: no pulses until the first wave
}

// ============================================================================
//  EYES  (two SSD1306, one per I2C bus, so identical 0x3C addresses are fine)
// ============================================================================
#if EYES_64X48
U8G2_SSD1306_64X48_ER_F_HW_I2C eyeL(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_64X48_ER_F_2ND_HW_I2C eyeR(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C eyeL(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C eyeR(U8G2_R0, U8X8_PIN_NONE);
#endif
static bool g_eyeLOk = false, g_eyeROk = false;

struct EyeAnim {
  float gx = 0, gy = 0, tx = 0, ty = 0;   // gaze current / target (-1..1)
  float open = 0.0f;                      // eyelid 0..1 (starts closed: eyes open at boot)
  uint32_t blinkStart = 0, nextBlink = 1500, nextGaze = 800;
  Mood lastMood = M_COUNT;
} eye;
static uint32_t g_eyeLastFrame = 0;
constexpr uint32_t BLINK_MS = 170;

static void safeRBox(U8G2 &d, int x, int y, int w, int h, int r) {
  if (w < 2 || h < 1) return;
  if (h < 4) { d.drawBox(x, y, w, h); return; }
  r = imin(r, imin((h - 1) / 2, (w - 1) / 2));
  if (r < 1) d.drawBox(x, y, w, h); else d.drawRBox(x, y, w, h, r);
}

static void updateEyeAnim(uint32_t now) {
  if (g_mood != eye.lastMood) {
    eye.lastMood = g_mood;
    uint8_t contrast = (g_mood == M_SLEEP) ? 8 : 190;
    if (g_eyeLOk) eyeL.setContrast(contrast);
    if (g_eyeROk) eyeR.setContrast(contrast);
  }
  switch (g_mood) {
    case M_IDLE:
    case M_HAPPY:
    case M_BOOT:
      if (now >= eye.nextGaze) {
        eye.tx = random(-100, 101) / 100.0f * 0.8f;
        eye.ty = random(-60, 61) / 100.0f * 0.5f;
        eye.nextGaze = now + random(1500, 4000);
      }
      break;
    case M_THINK: { float t = now / 450.0f; eye.tx = 0.65f * cosf(t); eye.ty = -0.55f + 0.15f * sinf(t * 1.7f); break; }
    case M_CONFUSED: eye.tx = -0.5f; eye.ty = 0.25f; break;
    case M_SAD: eye.tx = 0; eye.ty = 0.7f; break;
    default: eye.tx = 0; eye.ty = 0; break;
  }
  eye.gx += (eye.tx - eye.gx) * 0.35f;
  eye.gy += (eye.ty - eye.gy) * 0.35f;

  float target = 0.95f;
  switch (g_mood) {
    case M_LISTEN:   target = 1.0f; break;
    case M_THINK:    target = 0.75f; break;
    case M_SPEAK:    target = 0.85f + 0.15f * g_level; break;
    case M_SAD:      target = 0.45f; break;
    case M_SLEEP:    target = 0.06f; break;
    case M_CONFUSED: target = 0.9f; break;
    default: break;
  }
  if (g_mood != M_SLEEP && g_mood != M_ERROR) {
    if (now >= eye.nextBlink) { eye.blinkStart = now; eye.nextBlink = now + random(2500, 6000); }
    uint32_t bt = now - eye.blinkStart;
    if (bt < BLINK_MS) target *= (1.0f - sinf(PI * bt / BLINK_MS));
  }
  eye.open += (target - eye.open) * 0.6f;
}

static void drawEye(U8G2 &d, bool left) {
  const int W = d.getDisplayWidth(), H = d.getDisplayHeight();
  const int cx = W / 2, cy = H / 2;
  int ew = (W * 78) / 100, eh = (H * 72) / 100;
  if (g_mood == M_CONFUSED && left) { ew = ew * 80 / 100; eh = eh * 70 / 100; }
  d.clearBuffer();
  d.setDrawColor(1);

  if (g_mood == M_ERROR) {                                        // X eyes
    const int a = imin(W, H) / 3;
    for (int o = -1; o <= 1; o++) {
      d.drawLine(cx - a + o, cy - a, cx + a + o, cy + a);
      d.drawLine(cx - a + o, cy + a, cx + a + o, cy - a);
    }
    d.sendBuffer();
    return;
  }

  const int h = imax(2, (int)(eh * eye.open));
  const int ox = (int)(eye.gx * W * 0.06f), oy = (int)(eye.gy * H * 0.06f);
  const int x = cx - ew / 2 + ox, y = cy - h / 2 + oy;
  safeRBox(d, x, y, ew, h, imin(ew, h) / 2);

  if (g_mood == M_HAPPY) {                                        // happy arch: cut away the lower part
    d.setDrawColor(0);
    d.drawBox(x - 1, cy + oy + h / 8, ew + 2, h);
    d.setDrawColor(1);
  } else if (h > 6 && g_mood != M_SLEEP) {                        // pupil + highlight
    int pr = (int)(imin(ew, h) * (0.24f + (g_mood == M_LISTEN ? 0.06f : 0.0f) + (g_mood == M_SPEAK ? 0.06f * g_level : 0.0f)));
    pr = imax(2, pr);
    const int px = cx + ox + (int)(eye.gx * ew * 0.18f), py = cy + oy + (int)(eye.gy * eh * 0.18f);
    d.setDrawColor(0);
    d.drawDisc(px, py, pr);
    d.setDrawColor(1);
    d.drawDisc(px - pr / 3, py - pr / 3, imax(1, pr / 4));
  }
  d.sendBuffer();
}

static void eyesTick(uint32_t now) {
  if (g_state == ST_IDLE && g_mood == M_IDLE && (now - g_lastActivity) > EYE_SLEEP_MS) setMood(M_SLEEP);
  if (g_moodHoldUntil && (int32_t)(now - g_moodHoldUntil) >= 0) {       // a timed expression (happy/confused/error) ran out
    g_moodHoldUntil = 0;
    if (g_state == ST_IDLE) setMood(M_IDLE);
    else if (g_state == ST_OFFLINE) setMood(M_SAD);
    else if (g_state == ST_WAITING) setMood(M_THINK);
  }
  const uint32_t interval = (g_state == ST_RECEIVING) ? 250 : EYE_FRAME_MS;   // keep the CPU free while downloading speech
  if (now - g_eyeLastFrame < interval) return;
  g_eyeLastFrame = now;
  updateEyeAnim(now);
  if (g_eyeLOk) drawEye(eyeL, true);
  if (g_eyeROk) drawEye(eyeR, false);
}

static bool probeI2C(TwoWire &bus, uint8_t addr) {
  for (int i = 0; i < 3; i++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) return true;
    delay(10);
  }
  return false;
}

static void initEyes() {
  Wire.setPins(PIN_OLED_L_SDA, PIN_OLED_L_SCL);
  Wire1.setPins(PIN_OLED_R_SDA, PIN_OLED_R_SCL);
  eyeL.setI2CAddress(OLED_ADDR_LEFT << 1);
  eyeR.setI2CAddress(OLED_ADDR_RIGHT << 1);
  eyeL.setBusClock(400000);
  eyeR.setBusClock(400000);
  eyeL.begin();
  eyeR.begin();
  g_eyeLOk = probeI2C(Wire, OLED_ADDR_LEFT);
  g_eyeROk = probeI2C(Wire1, OLED_ADDR_RIGHT);
  LOG(g_eyeLOk ? "INIT" : "ERROR", "left eye OLED (0x%02X on SDA%d/SCL%d): %s", OLED_ADDR_LEFT, PIN_OLED_L_SDA, PIN_OLED_L_SCL, g_eyeLOk ? "OK" : "NOT FOUND - check wiring/address");
  LOG(g_eyeROk ? "INIT" : "ERROR", "right eye OLED (0x%02X on SDA%d/SCL%d): %s", OLED_ADDR_RIGHT, PIN_OLED_R_SDA, PIN_OLED_R_SCL, g_eyeROk ? "OK" : "NOT FOUND - check wiring/address");
}

// ============================================================================
//  AUDIO  (legacy I2S driver: works on core 2.x; core 3.x prints a harmless deprecation warning)
// ============================================================================
static void ampEnable(bool on) { digitalWrite(PIN_SPK_SD, on ? HIGH : LOW); }

static bool initMic() {
  i2s_config_t c = {};
  c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  c.sample_rate = MIC_RATE;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;        // INMP441 delivers 24 bits inside a 32-bit slot
  c.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;         // L/R pin tied to GND
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  c.dma_buf_count = 8;
  c.dma_buf_len = 256;
  c.use_apll = false;
  c.tx_desc_auto_clear = false;
  c.fixed_mclk = 0;
  if (i2s_driver_install(I2S_NUM_0, &c, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t p = {};
  p.mck_io_num = I2S_PIN_NO_CHANGE;
  p.bck_io_num = PIN_MIC_SCK;
  p.ws_io_num = PIN_MIC_WS;
  p.data_out_num = I2S_PIN_NO_CHANGE;
  p.data_in_num = PIN_MIC_SD;
  if (i2s_set_pin(I2S_NUM_0, &p) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

static bool initSpeaker() {
  i2s_config_t c = {};
  c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  c.sample_rate = SPK_RATE;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  c.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;        // we send identical L/R frames: full level whatever the amp's SD mode
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  c.dma_buf_count = 8;
  c.dma_buf_len = 512;                                  // 8 x 512 frames = ~170 ms of cushion at 24 kHz
  c.use_apll = false;
  c.tx_desc_auto_clear = true;                          // underrun = silence, not a repeating buzz
  c.fixed_mclk = 0;
  if (i2s_driver_install(I2S_NUM_1, &c, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t p = {};
  p.mck_io_num = I2S_PIN_NO_CHANGE;
  p.bck_io_num = PIN_SPK_BCLK;
  p.ws_io_num = PIN_SPK_LRC;
  p.data_out_num = PIN_SPK_DIN;
  p.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(I2S_NUM_1, &p) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_NUM_1);
  return true;
}

static bool allocBuffer() {
  if (psramFound()) {
    const size_t tries[] = {2u * 1024 * 1024, 1024u * 1024, 512u * 1024, 256u * 1024};
    for (size_t t : tries) {
      g_buf = (uint8_t *)heap_caps_malloc(t, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (g_buf) { g_bufSize = t; break; }
    }
  } else {
    LOG("WARN", "no PSRAM detected: Tools > PSRAM must be OPI (N8R8/N16R8) or QSPI (R2). Falling back to a tiny buffer.");
    const size_t tries[] = {96u * 1024, 64u * 1024, 32u * 1024};
    for (size_t t : tries) {
      g_buf = (uint8_t *)heap_caps_malloc(t, MALLOC_CAP_8BIT);
      if (g_buf) { g_bufSize = t; break; }
    }
  }
  g_bufSize &= ~(size_t)1;
  return g_buf != nullptr;
}

// write interleaved stereo 16-bit frames; resumes after partial writes so L/R alignment is never lost
static bool writeFrames(const int16_t *stereo, size_t frames) {
  const uint8_t *p = (const uint8_t *)stereo;
  size_t total = frames * 4, off = 0;
  const uint32_t t0 = millis();
  while (off < total) {
    size_t w = 0;
    i2s_write(I2S_NUM_1, p + off, total - off, &w, pdMS_TO_TICKS(50));
    off += w;
    if (millis() - t0 > 400) { LOG("ERROR", "I2S TX stalled"); return false; }
  }
  return true;
}

static void writeSilence(uint32_t ms) {
  static int16_t z[256 * 2];
  memset(z, 0, sizeof(z));
  size_t frames = (SPK_RATE / 1000) * ms;
  while (frames) {
    size_t n = frames > 256 ? 256 : frames;
    if (!writeFrames(z, n)) return;
    frames -= n;
  }
}

static void beep(uint16_t freq, uint16_t ms) {
  if (!g_spkOk) return;
  ampEnable(true);
  delay(25);
  writeSilence(30);
  static int16_t buf[256 * 2];
  const size_t frames = (SPK_RATE / 1000) * ms;
  const size_t ramp = SPK_RATE / 200;                   // 5 ms fade in/out: no clicks
  const float inc = 2.0f * PI * freq / SPK_RATE;
  const float amp = 9000.0f * g_volume / 100.0f;
  float phase = 0;
  size_t done = 0;
  while (done < frames) {
    size_t n = imin(256, (int32_t)(frames - done));
    for (size_t i = 0; i < n; i++) {
      size_t g = done + i;
      float env = imin((int32_t)g, (int32_t)(frames - g)) < (int32_t)ramp ? (float)imin((int32_t)g, (int32_t)(frames - g)) / ramp : 1.0f;
      int16_t v = (int16_t)(sinf(phase) * amp * env);
      phase += inc;
      buf[2 * i] = buf[2 * i + 1] = v;
    }
    if (!writeFrames(buf, n)) break;
    done += n;
  }
  writeSilence(200);                                    // lets the tone drain before the amp is switched off
  ampEnable(false);
}

static void beepError() { beep(330, 150); delay(60); beep(220, 250); }

// ---- stored clips: LittleFS + IMA ADPCM ------------------------------------------
static const int16_t ADPCM_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73,
    80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494,
    544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499,
    2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t ADPCM_INDEX[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

// Decode one 256-byte block into 505 samples. Each block is self-contained, so a bad block cannot
// corrupt the rest of the clip.
static void adpcmDecodeBlock(const uint8_t *in, int16_t *out) {
  int32_t pred = (int16_t)(in[0] | (in[1] << 8));
  int32_t index = clampi(in[2], 0, 88);
  size_t o = 0;
  out[o++] = (int16_t)pred;
  for (size_t i = 4; i < ADPCM_BLOCK; i++) {
    for (int half = 0; half < 2; half++) {
      const uint8_t code = half ? (in[i] >> 4) : (in[i] & 0x0F);
      const int32_t step = ADPCM_STEP[index];
      int32_t delta = step >> 3;
      if (code & 4) delta += step;
      if (code & 2) delta += step >> 1;
      if (code & 1) delta += step >> 2;
      pred = clampi((code & 8) ? pred - delta : pred + delta, -32768, 32767);
      index = clampi(index + ADPCM_INDEX[code], 0, 88);
      out[o++] = (int16_t)pred;
    }
  }
}

static bool validClipId(const char *id) {
  size_t n = strlen(id);
  if (n == 0 || n > CLIP_ID_LEN) return false;
  for (size_t i = 0; i < n; i++) if (!isxdigit((int)id[i])) return false;   // hex only: nothing can escape /clips
  return true;
}

static void clipPath(const char *id, char *out, size_t n) { snprintf(out, n, "/clips/%s.adp", id); }

static size_t fsFreeBytes() { return g_fsOk ? (LittleFS.totalBytes() - LittleFS.usedBytes()) : 0; }

static void sendClipList() {
  if (!g_wsUp) return;
  if (!g_fsOk) { ws.sendTXT("CLIPS_END"); return; }
  File dir = LittleFS.open("/clips");
  char line[600];
  size_t len = 0, count = 0;
  strcpy(line, "CLIPS:");
  len = strlen(line);
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      const char *nm = strrchr(f.name(), '/');
      nm = nm ? nm + 1 : f.name();
      char id[CLIP_ID_LEN + 8];
      snprintf(id, sizeof(id), "%s", nm);
      char *dot = strstr(id, ".adp");
      if (!dot) { f.close(); continue; }
      *dot = 0;
      if (len + strlen(id) + 2 >= sizeof(line)) { ws.sendTXT(line); strcpy(line, "CLIPS:"); len = strlen(line); }
      len += snprintf(line + len, sizeof(line) - len, "%s,", id);
      count++;
      f.close();
    }
    dir.close();
  }
  if (len > 6) ws.sendTXT(line);
  ws.sendTXT("CLIPS_END");
  LOG("INFO", "stored clips: %u  (flash free %u KB of %u KB)", (unsigned)count, (unsigned)(fsFreeBytes() / 1024),
      (unsigned)(g_fsOk ? LittleFS.totalBytes() / 1024 : 0));
}

static void abortClipRx(const char *why) {
  if (g_clipFile) { g_clipFile.close(); }
  if (g_clipId[0]) {
    char path[48];
    clipPath(g_clipId, path, sizeof(path));
    LittleFS.remove(path);                                   // never leave a half-written clip behind
    LOG("WARN", "clip %s upload aborted: %s", g_clipId, why);
    if (g_wsUp) { char m[40]; snprintf(m, sizeof(m), "CLIP_ERR:%s", g_clipId); ws.sendTXT(m); }
  }
  g_clipId[0] = 0;
  g_clipExpected = g_clipPos = 0;
  g_rxMode = RX_NONE;
}

static void beginClipRx(const char *id, size_t bytes) {
  if (!g_fsOk || !validClipId(id) || bytes == 0 || (bytes % ADPCM_BLOCK)) {
    LOG("ERROR", "clip upload rejected (fs=%d id='%s' %u bytes)", (int)g_fsOk, id, (unsigned)bytes);
    char m[40]; snprintf(m, sizeof(m), "CLIP_ERR:%s", id); ws.sendTXT(m);
    return;
  }
  if (bytes + 16384 > fsFreeBytes()) {
    LOG("WARN", "not enough flash for clip %s (%u KB needed, %u KB free)", id, (unsigned)(bytes / 1024), (unsigned)(fsFreeBytes() / 1024));
    char m[40]; snprintf(m, sizeof(m), "CLIP_ERR:%s", id); ws.sendTXT(m);
    return;
  }
  snprintf(g_clipId, sizeof(g_clipId), "%s", id);
  char path[48];
  clipPath(g_clipId, path, sizeof(path));
  g_clipFile = LittleFS.open(path, "w");
  if (!g_clipFile) { LOG("ERROR", "cannot create %s", path); abortClipRx("open failed"); return; }
  g_clipExpected = bytes;
  g_clipPos = 0;
  g_rxMode = RX_CLIP;
}

static void endClipRx() {
  if (g_rxMode != RX_CLIP) return;
  const bool ok = (g_clipPos == g_clipExpected) && g_clipFile;
  if (g_clipFile) g_clipFile.close();
  char m[40];
  if (ok) {
    snprintf(m, sizeof(m), "CLIP_OK:%s", g_clipId);
    LOG("INFO", "clip %s stored (%u KB, %u KB free)", g_clipId, (unsigned)(g_clipPos / 1024), (unsigned)(fsFreeBytes() / 1024));
    ws.sendTXT(m);
    g_clipId[0] = 0;
    g_clipExpected = g_clipPos = 0;
    g_rxMode = RX_NONE;
  } else {
    abortClipRx("short upload");
  }
}

static void deleteClip(const char *id) {
  if (!g_fsOk || !validClipId(id)) return;
  char path[48];
  clipPath(id, path, sizeof(path));
  if (LittleFS.remove(path)) LOG("INFO", "clip %s deleted (%u KB free)", id, (unsigned)(fsFreeBytes() / 1024));
}

// ---- playback -------------------------------------------------------------------
static void audioStart(uint32_t rate) {
  i2s_set_sample_rates(I2S_NUM_1, rate);                 // streamed audio is 24 kHz, stored clips are 16 kHz
  ampEnable(true);
  delay(25);
  i2s_zero_dma_buffer(I2S_NUM_1);
  writeSilence(40);                                     // amp wake-up pop lands on silence
}

static void startPlayback() {
  g_playPos = 0;
  g_playLen = g_rxExpected;
  g_level = 0;
  g_playFromFlash = false;
  audioStart(SPK_RATE);
  enterState(ST_PLAYING);
}

// Play a clip straight out of flash: no network transfer, works even if the bridge later disappears.
static bool startClipPlayback(const char *id) {
  if (!g_fsOk || !validClipId(id)) return false;
  char path[48];
  clipPath(id, path, sizeof(path));
  g_clipFile = LittleFS.open(path, "r");
  if (!g_clipFile || g_clipFile.size() < ADPCM_BLOCK) {
    if (g_clipFile) g_clipFile.close();
    LOG("WARN", "clip %s not on this robot", id);
    return false;
  }
  g_playLen = g_clipFile.size();
  g_playPos = 0;
  g_level = 0;
  g_playFromFlash = true;
  audioStart(CLIP_RATE);
  enterState(ST_PLAYING);
  LOG("INFO", "playing clip %s from flash (%u KB, %.1f s)", id, (unsigned)(g_playLen / 1024),
      (g_playLen / (float)ADPCM_BLOCK) * ADPCM_SAMPLES / CLIP_RATE);
  return true;
}

// PLAYED:ERR carries a short reason code so the bridge's own log shows why, without needing a
// second serial monitor open on the robot. Kept as one word (no spaces/colons) - see bridge.py's
// on_text() for the matching parser.
static void sendPlayedErr(const char *why) {
  if (!g_wsUp) return;
  char m[64];
  snprintf(m, sizeof(m), "PLAYED:ERR:%s", why);
  ws.sendTXT(m);
}

static void finishPlayback(bool ok, const char *why) {
  writeSilence(200);
  ampEnable(false);
  if (g_playFromFlash && g_clipFile) g_clipFile.close();
  g_playFromFlash = false;
  g_level = 0;
  if (g_wsUp) { if (ok) ws.sendTXT("PLAYED:OK"); else sendPlayedErr(why[0] ? why : "unknown"); }
  enterState(ST_WAITING, ok ? M_HAPPY : M_CONFUSED);
  g_waitDeadline = millis() + WAIT_AFTER_PLAY_MS;
  if (ok) LOG("INFO", "playback finished");
  else LOG("INFO", "playback aborted: %s", why[0] ? why : "unknown");
}

static void stopPlaybackNow() {                         // visitor pressed the button: cut the audio
  if (g_playFromFlash && g_clipFile) g_clipFile.close();
  g_playFromFlash = false;
  i2s_zero_dma_buffer(I2S_NUM_1);
  ampEnable(false);
  g_level = 0;
  LOG("INFO", "playback interrupted by button");
}

static void playClipStep() {
  if (g_playPos >= g_playLen) { finishPlayback(true, ""); return; }
  static int16_t mono[ADPCM_SAMPLES];
  static int16_t stereo[ADPCM_SAMPLES * 2];
  const size_t n = g_clipFile.read(g_blk, ADPCM_BLOCK);
  if (n != ADPCM_BLOCK) { LOG("ERROR", "clip read failed"); finishPlayback(false, "clip-read-failed"); return; }
  g_playPos += n;
  adpcmDecodeBlock(g_blk, mono);
  int64_t sumsq = 0;
  for (size_t i = 0; i < ADPCM_SAMPLES; i++) {
    int32_t v = clampi(((int32_t)mono[i] * g_volume) / 100, -32768, 32767);
    stereo[2 * i] = stereo[2 * i + 1] = (int16_t)v;
    sumsq += (int64_t)v * v;
  }
  if (!writeFrames(stereo, ADPCM_SAMPLES)) { finishPlayback(false, "i2s-stalled"); return; }
  const float rms = sqrtf((float)(sumsq / (int64_t)ADPCM_SAMPLES));
  g_level += (clampi((int32_t)rms, 0, 9000) / 9000.0f - g_level) * 0.4f;
}

static void playStep() {
  if (g_playFromFlash) { playClipStep(); return; }
  if (g_playPos >= g_playLen) { finishPlayback(true, ""); return; }
  static int16_t stereo[PLAY_CHUNK_FRAMES * 2];
  size_t frames = (g_playLen - g_playPos) / 2;
  if (frames > PLAY_CHUNK_FRAMES) frames = PLAY_CHUNK_FRAMES;
  const int16_t *src = (const int16_t *)(g_buf + g_playPos);
  int64_t sumsq = 0;
  for (size_t i = 0; i < frames; i++) {
    int32_t v = ((int32_t)src[i] * g_volume) / 100;
    v = clampi(v, -32768, 32767);
    stereo[2 * i] = stereo[2 * i + 1] = (int16_t)v;
    sumsq += (int64_t)v * v;
  }
  if (!writeFrames(stereo, frames)) { finishPlayback(false, "i2s-stalled"); return; }
  g_playPos += frames * 2;
  float rms = sqrtf((float)(sumsq / (int64_t)(frames ? frames : 1)));
  float lvl = clampi((int32_t)(rms), 0, 9000) / 9000.0f;
  g_level += (lvl - g_level) * 0.4f;
}

// ---- recording ------------------------------------------------------------------
static void flushPacket() {
  if (g_pktLen == 0) return;
  if (!g_wsUp || !ws.sendBIN((uint8_t *)g_pkt, g_pktLen * 2)) LOG("WARN", "mic packet not sent");
  g_pktLen = 0;
}

static void startRecording(uint32_t now) {
  i2s_zero_dma_buffer(I2S_NUM_0);
  g_pktLen = 0;
  g_recPeak = 0;
  g_recStart = now;
  g_dcX = g_dcY = 0;
  g_dropSamples = MIC_RATE / 10;                        // ignore the first 100 ms (button clack)
  g_micTimeouts = 0;
  ws.sendTXT("PTT:1");
  enterState(ST_RECORDING);
  LOG("INFO", "recording...");
}

static void stopRecording(uint32_t now, const char *why) {
  flushPacket();
  ws.sendTXT("PTT:0");
  LOG("INFO", "recording stopped (%s): %lu ms, peak %ld%s", why, (unsigned long)(now - g_recStart), (long)g_recPeak,
      g_recPeak < 30 ? "  <-- SILENT: check INMP441 wiring / L-R pin to GND" : "");
  enterState(ST_WAITING);
}

static void recordStep(uint32_t now) {
  if (!g_wsUp) { enterState(ST_OFFLINE); return; }
  if (now - g_recStart > MAX_REC_MS) { stopRecording(now, "15 s limit"); return; }
  static int32_t raw[MIC_READ_FRAMES];
  size_t br = 0;
  esp_err_t e = i2s_read(I2S_NUM_0, raw, sizeof(raw), &br, pdMS_TO_TICKS(50));
  if (e != ESP_OK || br == 0) {
    if (++g_micTimeouts == 20) LOG("ERROR", "microphone delivers no data (SCK/WS/SD wiring? 3.3V?)");
    return;
  }
  const size_t n = br / 4;
  for (size_t i = 0; i < n; i++) {
    if (g_dropSamples) { g_dropSamples--; continue; }
    int32_t s = clampi(raw[i] >> MIC_SHIFT, -32768, 32767);
    float y = (float)s - g_dcX + 0.995f * g_dcY;        // DC blocker: removes mic offset and low rumble
    g_dcX = (float)s;
    g_dcY = y;
    int32_t o = clampi((int32_t)y, -32768, 32767);
    if (abs(o) > g_recPeak) g_recPeak = abs(o);
    g_pkt[g_pktLen++] = (int16_t)o;
    if (g_pktLen == MIC_PACKET_SAMPLES) flushPacket();
  }
}

// ============================================================================
//  WEBSOCKET
// ============================================================================
static void abortDownload(const char *why) {
  LOG("WARN", "download aborted: %s (%u/%u bytes)", why, (unsigned)g_rxPos, (unsigned)g_rxExpected);
  sendPlayedErr(why);
  g_rxExpected = g_rxPos = 0;
  enterState(ST_WAITING, M_CONFUSED);
  setMood(M_CONFUSED, 1500);
}

static void beginRx(size_t n) {
  if (g_state == ST_RECORDING) return;
  g_rxMode = RX_TTS;                  // stale answer for an old question; the bridge cancels it itself
  if (!g_buf) {
    LOG("ERROR", "cannot accept audio: no speech buffer allocated");
    sendPlayedErr("no-buffer");
    return;
  }
  if (n == 0 || (n & 1)) {
    LOG("ERROR", "cannot accept audio: bad length %u bytes", (unsigned)n);
    sendPlayedErr("bad-length");
    return;
  }
  if (n > g_bufSize) {
    LOG("ERROR", "cannot accept %u bytes of audio (buffer only %u bytes - check Tools > PSRAM)", (unsigned)n, (unsigned)g_bufSize);
    sendPlayedErr("too-large");
    return;
  }
  g_rxExpected = n;
  g_rxPos = 0;
  g_rxLastMs = millis();
  enterState(ST_RECEIVING);
}

static void endRx() {
  g_rxMode = RX_NONE;
  if (g_state != ST_RECEIVING) return;
  if (g_rxPos != g_rxExpected) { abortDownload("length mismatch"); return; }
  LOG("INFO", "speech received: %u bytes (%.1f s)", (unsigned)g_rxPos, g_rxPos / 2.0f / SPK_RATE);
  startPlayback();
}

static void setVolume(int v) {
  g_volume = (uint8_t)clampi(v, 0, 100);
  prefs.putUChar("vol", g_volume);
  LOG("INFO", "volume %u%%", g_volume);
}

static void handleText(const char *m) {
  if (strncmp(m, "STATE:", 6) == 0) {
    const char *s = m + 6;
    g_lastActivity = millis();
    if (g_state == ST_RECORDING) return;
    if (!strcmp(s, "idle")) {
      if (g_state == ST_WAITING || g_state == ST_RECEIVING || g_state == ST_OFFLINE || g_state == ST_IDLE) enterState(ST_IDLE);
    } else if (!strcmp(s, "happy"))    setMood(M_HAPPY, 1500);
    else if (!strcmp(s, "confused"))   setMood(M_CONFUSED, 1500);
    else if (!strcmp(s, "error"))    { setMood(M_ERROR, 2000); beepError(); }
    else if (!strcmp(s, "thinking") && g_state == ST_WAITING) setMood(M_THINK);
    else if (!strcmp(s, "speaking") && g_state == ST_WAITING) setMood(M_SPEAK);
  } else if (strncmp(m, "TTS_BEGIN:", 10) == 0) {
    beginRx((size_t)strtoul(m + 10, nullptr, 10));
  } else if (strcmp(m, "TTS_END") == 0) {
    endRx();
  } else if (strncmp(m, "CLIP_BEGIN:", 11) == 0) {
    char id[CLIP_ID_LEN + 8] = {0};
    unsigned long bytes = 0;
    if (sscanf(m + 11, "%16[^:]:%lu", id, &bytes) == 2) beginClipRx(id, (size_t)bytes);
    else LOG("WARN", "bad CLIP_BEGIN: %s", m);
  } else if (strcmp(m, "CLIP_END") == 0) {
    endClipRx();
  } else if (strncmp(m, "CLIP_DEL:", 9) == 0) {
    deleteClip(m + 9);
  } else if (strncmp(m, "PLAY:", 5) == 0) {
    if (g_state == ST_RECORDING) return;
    if (!startClipPlayback(m + 5)) sendPlayedErr("clip-missing");   // bridge streams the audio instead
  } else if (strncmp(m, "MOVE:", 5) == 0) {
    char d = 0;
    unsigned long ms = 0;
    int pct = 0;
    if (sscanf(m + 5, "%c:%lu:%d", &d, &ms, &pct) == 3) moveCommand(d, ms, pct, false);
    else LOG("WARN", "bad MOVE message: %s", m);
  } else if (strcmp(m, "WAVE") == 0 || strncmp(m, "WAVE:", 5) == 0) {
    if (g_state == ST_RECORDING) return;
    const int n = m[4] == ':' ? atoi(m + 5) : WAVE_DEFAULT_CYCLES;
    waveStart((uint8_t)clampi(n > 0 ? n : WAVE_DEFAULT_CYCLES, 1, WAVE_MAX_CYCLES));
  } else if (strncmp(m, "VOL:", 4) == 0) {
    setVolume(atoi(m + 4));
  } else {
    LOG("WARN", "unknown message: %.40s", m);
  }
}

static void onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      g_wsUp = true;
      LOG("RECOVERY", "bridge connected: %s:%u", BRIDGE_HOST, BRIDGE_PORT);
      char hello[72];
      snprintf(hello, sizeof(hello), "HELLO:%u:%u:%u", (unsigned)g_bufSize, (unsigned)SPK_RATE,
               (unsigned)(fsFreeBytes() / 1024));
      ws.sendTXT(hello);
      sendClipList();
      enterState(ST_IDLE, M_HAPPY);
      setMood(M_HAPPY, 1500);
      break;
    }
    case WStype_DISCONNECTED:
      if (g_wsUp) LOG("WARN", "bridge connection lost");
      g_wsUp = false;
      g_bridgeReachable = false;
      motorsStopNow();
      if (g_state == ST_PLAYING) stopPlaybackNow();
      if (g_rxMode == RX_CLIP) abortClipRx("disconnected");
      enterState(ST_OFFLINE);
      break;
    case WStype_TEXT: {
      char msg[80];
      size_t n = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
      memcpy(msg, payload, n);
      msg[n] = 0;
      handleText(msg);
      break;
    }
    case WStype_BIN:
      if (g_rxMode == RX_CLIP) {
        if (g_clipFile && g_clipPos + length <= g_clipExpected && g_clipFile.write(payload, length) == length) {
          g_clipPos += length;
        } else {
          abortClipRx("write failed");
        }
        break;
      }
      if (g_state == ST_RECEIVING) {
        size_t room = g_rxExpected - g_rxPos;
        size_t n = length < room ? length : room;
        if (n < length) LOG("WARN", "extra audio bytes dropped");
        memcpy(g_buf + g_rxPos, payload, n);
        g_rxPos += n;
        g_rxLastMs = millis();
      }
      break;
    case WStype_ERROR:
      // the library doesn't hand us a message, only that something went wrong on the socket -
      // surfacing it at all beats the silent no-op this used to be while a download quietly stalls
      LOG("ERROR", "WebSocket error event (state=%s rx=%u/%u)", STATE_NAMES[g_state], (unsigned)g_rxPos, (unsigned)g_rxExpected);
      break;
    default: break;
  }
}

// ============================================================================
//  WI-FI
// ============================================================================
static bool connectWifi(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                                 // modem sleep adds latency and audio jitter
  WiFi.setAutoReconnect(true);
  static bool added = false;
  if (!added) {
    wifiMulti.addAP(WIFI_SSID_1, WIFI_PASS_1);
    if (strlen(WIFI_SSID_2)) wifiMulti.addAP(WIFI_SSID_2, WIFI_PASS_2);
    added = true;
  }
  const uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (wifiMulti.run(3000) == WL_CONNECTED) {
      LOG("INIT", "Wi-Fi '%s'  IP %s  RSSI %d dBm", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      return true;
    }
    LOG("WARN", "Wi-Fi not connected yet...");
    eyesTick(millis());
  }
  return false;
}

static void wifiWatchdog(uint32_t now) {
  static uint32_t lastCheck = 0, lostSince = 0, lastWeak = 0;
  static bool wasUp = false;
  if (now - lastCheck < 1000) return;
  lastCheck = now;
  const bool up = (WiFi.status() == WL_CONNECTED);
  if (up && !wasUp) LOG("RECOVERY", "Wi-Fi up  IP %s  RSSI %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  if (!up && wasUp) { LOG("WARN", "Wi-Fi lost"); motorsStopNow(); lostSince = now; }
  if (!up) {
    if (!lostSince) lostSince = now;
    if (now - lostSince > WIFI_RETRY_MS) {
      LOG("RECOVERY", "re-scanning known networks");
      motorsStopNow();                                  // the call below blocks for a few seconds
      wifiMulti.run(4000);
      lostSince = millis();
    }
  } else {
    lostSince = 0;
    if (WiFi.RSSI() < -80 && now - lastWeak > 30000) { lastWeak = now; LOG("WARN", "weak Wi-Fi signal (%d dBm): move the router closer", WiFi.RSSI()); }
  }
  wasUp = up;
}

// The WebSocket library blocks ~5 s per connection attempt; when the bridge is down that would freeze the eyes,
// so probe the port first (700 ms) and only let the library connect when the bridge answers.
static void probeBridge(uint32_t now) {
  static uint32_t lastProbe = 0;
  if (g_wsUp || WiFi.status() != WL_CONNECTED || now - lastProbe < 3000) return;
  lastProbe = now;
  WiFiClient c;
  g_bridgeReachable = c.connect(BRIDGE_HOST, BRIDGE_PORT, 700);
  c.stop();
}

// ============================================================================
//  BUTTON / SERIAL / TIMEOUTS
// ============================================================================
static void onPress(uint32_t now) {
  g_lastActivity = now;
  motorsStopNow();
  waveCancel();
  if (g_mood == M_SLEEP) setMood(M_IDLE);
  if (!g_wsUp || !g_micOk) {
    LOG("WARN", "button pressed but %s", !g_wsUp ? "the bridge is not connected" : "the microphone is not working");
    setMood(M_ERROR, 1200);
    beepError();
    return;
  }
  if (g_state == ST_PLAYING) stopPlaybackNow();
  if (g_rxMode == RX_CLIP) abortClipRx("visitor pressed the button");   // bridge retries when idle
  g_rxExpected = g_rxPos = 0;
  g_rxMode = RX_NONE;
  startRecording(now);
}

static void onRelease(uint32_t now) {
  if (g_state == ST_RECORDING) stopRecording(now, "button released");
}

static void checkTimeouts(uint32_t now) {
  if (g_state == ST_RECEIVING && now - g_rxLastMs > RX_STALL_MS) abortDownload("stalled");
  if (g_state == ST_WAITING && (int32_t)(now - g_waitDeadline) >= 0) {
    LOG("WARN", "bridge did not finish answering in time");
    setMood(M_CONFUSED, 1500);
    enterState(ST_IDLE);
  }
}

static void printInfo() {
  LOG("INFO", "state=%s eyes=%s wifi=%s rssi=%d ws=%s buf=%uKB heap=%uKB psram=%uKB vol=%u%% mic=%s spk=%s eyeL=%s eyeR=%s fs=%s free=%uKB",
      STATE_NAMES[g_state], MOOD_NAMES[g_mood], WiFi.status() == WL_CONNECTED ? "up" : "DOWN", WiFi.RSSI(), g_wsUp ? "up" : "DOWN",
      (unsigned)(g_bufSize / 1024), (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024), g_volume,
      g_micOk ? "ok" : "FAIL", g_spkOk ? "ok" : "FAIL", g_eyeLOk ? "ok" : "FAIL", g_eyeROk ? "ok" : "FAIL",
      g_fsOk ? "ok" : "FAIL", (unsigned)(fsFreeBytes() / 1024));
}

static void micMeter() {
  if (!g_micOk) { LOG("ERROR", "mic not initialised"); return; }
  LOG("INFO", "mic meter for 4 s - speak, clap, then stay silent to see the noise floor (sets MIN_RMS on the bridge)");
  static int32_t raw[MIC_READ_FRAMES];
  const uint32_t t0 = millis();
  uint32_t lastPrint = t0;
  int64_t sumsq = 0;
  size_t cnt = 0;
  int32_t peak = 0;
  float dx = 0, dy = 0;
  while (millis() - t0 < 4000) {
    size_t br = 0;
    if (i2s_read(I2S_NUM_0, raw, sizeof(raw), &br, pdMS_TO_TICKS(50)) != ESP_OK || !br) continue;
    for (size_t i = 0; i < br / 4; i++) {
      int32_t s = clampi(raw[i] >> MIC_SHIFT, -32768, 32767);
      float y = (float)s - dx + 0.995f * dy;
      dx = (float)s;
      dy = y;
      int32_t o = (int32_t)y;
      sumsq += (int64_t)o * o;
      cnt++;
      if (abs(o) > peak) peak = abs(o);
    }
    if (millis() - lastPrint >= 250) {
      lastPrint = millis();
      LOG("INFO", "mic rms=%5ld peak=%5ld", (long)sqrtf((float)(sumsq / (int64_t)(cnt ? cnt : 1))), (long)peak);
      sumsq = 0; cnt = 0; peak = 0;
    }
    eyesTick(millis());
  }
}

static void handleSerial() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    switch (c) {
      case '?': LOG("INFO", "? help | i info | e eye mood | m mic meter | t beep | + - volume | f b l r s motors | w wave | c clip list | C erase clips"); break;
      case 'c': sendClipList(); break;
      case 'w': waveStart(WAVE_DEFAULT_CYCLES); break;
      case 'C':
        if (g_fsOk) {
          File d = LittleFS.open("/clips");
          char names[40][24];
          int n = 0;
          for (File f = d.openNextFile(); f && n < 40; f = d.openNextFile()) { snprintf(names[n++], 24, "%s", f.name()); f.close(); }
          d.close();
          for (int k = 0; k < n; k++) { char pth[48]; snprintf(pth, sizeof(pth), "/clips/%s", strrchr(names[k], '/') ? strrchr(names[k], '/') + 1 : names[k]); LittleFS.remove(pth); }
          LOG("INFO", "erased %d clips - reconnect the bridge to upload them again", n);
        }
        break;
      case 'i': printInfo(); break;
      case 'e': setMood((Mood)((g_mood + 1) % M_COUNT)); break;
      case 'm': micMeter(); break;
      case 't': beep(880, 180); beep(1320, 180); break;
      case '+': setVolume(g_volume + 10); break;
      case '-': setVolume((int)g_volume - 10); break;
      case 'f': moveCommand('F', 700, 50, true); break;
      case 'b': moveCommand('B', 700, 50, true); break;
      case 'l': moveCommand('L', 450, 50, true); break;
      case 'r': moveCommand('R', 450, 50, true); break;
      case 's': moveCommand('S', 0, 0, true); break;
      default: break;
    }
  }
}

// ============================================================================
//  SETUP / LOOP
// ============================================================================
void setup() {
  // 1) Motors OFF before anything else (pull-downs on the IN/EN lines cover the reset window before this runs)
  const int outs[] = {PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4, PIN_SPK_SD};
  for (int p : outs) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pwmInit(PIN_ENA, 0);
  pwmInit(PIN_ENB, 1);
  motorsStopNow();
  initServos();
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUMPER, INPUT_PULLUP);

  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  Serial.setTxTimeoutMs(0);                             // never block when no USB host is attached
#endif
  delay(300);
  const esp_reset_reason_t rr = esp_reset_reason();
  LOG("BOOT", "ORACLE v%s  ESP32-S3  %u MHz  reset reason: %s", FW_VERSION, (unsigned)getCpuFrequencyMhz(), resetReasonStr(rr));
  if (rr == ESP_RST_BROWNOUT) LOG("WARN", "last reset was a BROWNOUT: motors/amp are pulling the 5V rail down");
  LOG("BOOT", "features: I2S mic %u Hz, I2S speaker %u Hz, 2 OLED eyes (%s), L298N 2 motors, 2 arm servos (SERVO%d/SERVO%d), WebSocket bridge %s:%u",
      (unsigned)MIC_RATE, (unsigned)SPK_RATE, EYES_64X48 ? "64x48" : "128x64", PIN_SERVO_L, PIN_SERVO_R, BRIDGE_HOST, BRIDGE_PORT);

  if (prefs.begin("oracle", false)) g_volume = (uint8_t)clampi(prefs.getUChar("vol", DEFAULT_VOLUME), 0, 100);
  else LOG("WARN", "NVS unavailable - volume will not persist");
  randomSeed(esp_random());

  g_fsOk = LittleFS.begin(true);                        // true = format on first boot
  if (g_fsOk && !LittleFS.exists("/clips")) LittleFS.mkdir("/clips");
  LOG(g_fsOk ? "INIT" : "WARN", "clip storage: %s", g_fsOk ? "LittleFS mounted" : "unavailable - answers will stream over Wi-Fi");
  if (g_fsOk) LOG("INIT", "flash filesystem: %u KB total, %u KB free", (unsigned)(LittleFS.totalBytes() / 1024),
                  (unsigned)(fsFreeBytes() / 1024));

  initEyes();
  enterState(ST_BOOT);
  for (int i = 0; i < 12; i++) { eyesTick(millis()); delay(EYE_FRAME_MS); }   // eyes open

  g_micOk = initMic();
  LOG(g_micOk ? "INIT" : "ERROR", "microphone I2S (SCK%d WS%d SD%d): %s", PIN_MIC_SCK, PIN_MIC_WS, PIN_MIC_SD, g_micOk ? "OK" : "FAILED");
  g_spkOk = initSpeaker();
  LOG(g_spkOk ? "INIT" : "ERROR", "speaker I2S (BCLK%d LRC%d DIN%d): %s", PIN_SPK_BCLK, PIN_SPK_LRC, PIN_SPK_DIN, g_spkOk ? "OK" : "FAILED");
  const bool bufOk = allocBuffer();
  LOG(bufOk ? "INIT" : "ERROR", "speech buffer: %u KB (%s)", (unsigned)(g_bufSize / 1024), psramFound() ? "PSRAM" : "internal RAM");
  if (!bufOk) g_spkOk = false;
  if (g_spkOk) { beep(660, 90); }                       // audible power-up chirp: an unexpected one at the venue means a reset happened

  if (!connectWifi(20000)) LOG("WARN", "Wi-Fi not available at boot - will keep retrying");
  ws.begin(BRIDGE_HOST, BRIDGE_PORT, "/");
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(2000);
  ws.enableHeartbeat(10000, 3000, 2);                   // detects a half-dead link within ~16 s
  enterState(ST_OFFLINE);
  LOG("INFO", "ready - type ? for bench commands");
}

void loop() {
  const uint32_t now = millis();
  handleSerial();
  wifiWatchdog(now);
  probeBridge(now);
  if (WiFi.status() == WL_CONNECTED && (g_wsUp || g_bridgeReachable)) ws.loop();

  if (pollInput(bump, now) && bump.stable) LOG("INFO", "bumper pressed");
  if (pollInput(btn, now)) { if (btn.stable) onPress(now); else onRelease(now); }
  motorTick(now);
  servoTick(now);
  checkTimeouts(now);

  switch (g_state) {
    case ST_RECORDING: recordStep(now); break;
    case ST_PLAYING:   playStep(); break;
    default: break;
  }
  eyesTick(millis());
  if (g_state != ST_RECORDING && g_state != ST_PLAYING) delay(1);
}
