// ============================================================
// ESP32 + GM75 (UART Trigger) + MFRC522 + SMTP HTML Report
// NEW BARCODE FORMAT:  C{n}/T{t}/{TOKEN}
//   - Example: C1/T10/DSFD
//   - n = current index (1..32), t = total pieces (1..32)
//   - TOKEN = 2..6 alphabetic chars ONLY (batch/series ID), must be same across the set
// Features:
//   - Auto-size: expectedTotal taken from code's T{t}
//   - Ignores stray "31" junk from scanner
//   - Start/Stop armed gating + auto-retrigger
//   - RFID tap to finalize -> HTML email with report
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>  // Install "MFRC522" by GithubCommunity
#include <WiFi.h>
#include <ESP_Mail_Client.h>  // Install "ESP Mail Client" by Mobizt
struct Operator {
  uint8_t uid[7];
  uint8_t uidLen;
  const char* name;
  const char* email;
};
// ---------------- Wi-Fi ----------------
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";

// ---------------- SMTP (example: Gmail SMTP with app password) -------------
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define SMTP_USER "aquibansari12377@gmail.com"
#define SMTP_PASS "mmqe unbw dpqc xjmd"
#define MAIL_TO_FALLBACK "sujit.jadhav@galaxysurfactants.com"  // CC / fallback

SMTPSession smtp;
const char* LOGO_URL = "https://www.galaxysurfactants.com/images/galaxy-surfactants.png";  // <-- change this

// ---------------- RC522 Pins (ESP32 VSPI defaults shown) -------------------
/*
 * VSPI default: SCK=18, MISO=19, MOSI=23
 * Choose SS(SDA) and RST below:
 */
static const int RC522_SS = 5;    // SDA/SS
static const int RC522_RST = 22;  // RST

MFRC522 mfrc522(RC522_SS, RC522_RST);

// ---------------- GM75 UART ----------------
static const int UART_NUM = 2;
static const int PIN_RX = 16;  // GM75 TX -> ESP32 RX2
static const int PIN_TX = 17;  // ESP32 TX2 -> GM75 RX
static const uint32_t BAUD = 9600;
HardwareSerial GM(UART_NUM);

// ---------------- Buttons / Options ----------------
static const bool USE_BUTTONS = true;
static const int PIN_BTN_START = 27;  // INPUT_PULLUP; press -> GND
static const int PIN_BTN_STOP = 26;   // INPUT_PULLUP; press -> GND
static const bool LOG_FRAMES_HEX = false;

// Debounce & timing
static const uint32_t DEBOUNCE_MS = 200;
static const uint32_t IDLE_TIMEOUT_MS = 80;      // idle gap to end a text chunk
static const uint32_t ACCEPT_COOLDOWN_MS = 200;  // avoid immediate re-triggers

uint32_t lastStartPress = 0, lastStopPress = 0;
uint32_t lastAcceptMs = 0;

// Text framing (scanner provides ASCII, no CR/LF)
static const size_t BUF_CAP = 256;
char txtBuf[BUF_CAP];
size_t tlen = 0;
uint32_t lastByteMs = 0;

uint8_t expectedSequence[32];   // will be filled dynamically or with a fixed plan
uint8_t expectedLen = 0;        // how many entries are meaningful
bool    enforceSequence = true; // set false if you ever want to allow any order

// next position we expect (1-based position inside expectedSequence)
uint8_t nextExpectedPos = 1;

// helper: fill expectedSequence when first valid code locks the batch
// if you always want 1..expectedTotal in order, leave this as ascending default
void initExpectedSequence(uint8_t total) {
  expectedLen = total;
  for (uint8_t i = 0; i < total; ++i) expectedSequence[i] = i + 1; // 1,2,3,...,total
  nextExpectedPos = 1;
}

// Binary frame capture (0x7E ...)
static const size_t FBUF_CAP = 128;
uint8_t frmBuf[FBUF_CAP];
size_t flen = 0;
bool inFrame = false;

// Dedup of RAW strings (optional; we also dedup by index)
static const size_t SEEN_MAX = 96;
String seen[SEEN_MAX];
size_t seenCount = 0;
size_t seenHead = 0;

// GM75 Commands
const uint8_t CMD_START[] = { 0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD };
const uint8_t CMD_STOP[] = { 0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x00, 0xAB, 0xCD };

// Armed gating
volatile bool armed = false;

// -------- Project state (dynamic expectedTotal up to 32) --------
uint8_t expectedTotal = 0;  // set from first valid code (T{t})
uint64_t scannedMask = 0;   // bit (n-1) = scanned
uint32_t firstScanTs = 0, lastScanTs = 0;
uint8_t rawCount = 0;
String batchToken = "";  // TOKEN (2..6 letters), enforced across the set
String rawList[32];      // up to 32 pieces
uint32_t rawTs[32];
// Incorrect/Unexpected scans (failed parse, token/total mismatch, duplicates if you want)
static const size_t BAD_CAP = 64;
String badList[BAD_CAP];
uint32_t badTs[BAD_CAP];
size_t badCount = 0;

// Operator mapping (RFID UID -> name/email)

// Example entry (replace with your card UIDs)
Operator operators[] = {
  { { 0x63, 0x5A, 0x1B, 0x1A }, 4, "Sagar Ovhal - EMP-CODE 4188  Assessor email id - ", "jadhavsujit20@gmail.com" },
};
const size_t operatorsCount = sizeof(operators) / sizeof(operators[0]);

// ------------------------------------------------------------
// Utils
// ------------------------------------------------------------
void hexDump(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (i) Serial.print(' ');
    if (p[i] < 16) Serial.print('0');
    Serial.print(p[i], HEX);
  }
}
void clearText() {
  tlen = 0;
}

void seenInsert(const String& s) {
  if (seenCount < SEEN_MAX) seen[seenCount++] = s;
  else {
    seen[seenHead] = s;
    seenHead = (seenHead + 1) % SEEN_MAX;
  }
}
bool seenContains(const String& s) {
  size_t n = (seenCount < SEEN_MAX) ? seenCount : SEEN_MAX;
  for (size_t i = 0; i < n; ++i)
    if (seen[i] == s) return true;
  return false;
}

void sendStart() {
  clearText();
  armed = true;
  GM.write(CMD_START, sizeof(CMD_START));
  if (LOG_FRAMES_HEX) {
    Serial.print("[GM75] TX START: ");
    hexDump(CMD_START, sizeof(CMD_START));
    Serial.println();
  }
}
void sendStop() {
  armed = false;
  GM.write(CMD_STOP, sizeof(CMD_STOP));
  clearText();
  if (LOG_FRAMES_HEX) {
    Serial.print("[GM75] TX STOP : ");
    hexDump(CMD_STOP, sizeof(CMD_STOP));
    Serial.println();
  }
  delay(5);
  while (GM.available()) (void)GM.read();
}

// ----- Human-readable names for TOKENs -----
struct TokenName {
  const char* token;
  const char* name;
};
TokenName TOKEN_NAME_MAP[] = {
  // Uppercase TOKEN  ->  Display name
  { "CHKHGTV", "Pigeon" },
  // {"DSFD", "Some Product"},   // add more as needed
};
const size_t TOKEN_NAME_MAP_LEN = sizeof(TOKEN_NAME_MAP) / sizeof(TOKEN_NAME_MAP[0]);

String resolveTokenName(const String& tokenUpper) {
  for (size_t i = 0; i < TOKEN_NAME_MAP_LEN; ++i) {
    if (tokenUpper.equalsIgnoreCase(TOKEN_NAME_MAP[i].token)) return String(TOKEN_NAME_MAP[i].name);
  }
  // Fallback: just show the token if unknown
  return tokenUpper;
}

// Pretty label for “<Product> Box n out of t”
String prettyLabel(uint8_t index /*1..expectedTotal*/) {
  String name = resolveTokenName(batchToken);  // batchToken already uppercased
  String s;
  // s.reserve(48);
  // s += name;
  // s += " Box ";
  // s += String(index);
  // s += " out of ";
  // s += String(expectedTotal ? expectedTotal : 0);

  if(index == 1)
  {
    s = "Close V2";
  }
  else if(index == 2)
  {
    s = "Open V7";
  }
  else if(index== 3)
  {
    s = "Close V1";
  }
  else if(index == 4)
  {
    s = "Open V3";
  }
  else if(index == 5)
  {
    s = "Close V7";
  }
  else if(index == 6)
  {
    s = "Close V3";
  }
  else if(index == 7)
  {
    s = "Open V3";
  }
  return s;
}
bool isCompetent() {
  return (expectedTotal > 0) && (rawCount == expectedTotal) && (badCount == 0);
}


// ------------------------------------------------------------
// Barcode parsing for: C{n}/T{t}/{TOKEN}
// Robust against leading/trailing "31" junk
// ------------------------------------------------------------
static inline bool isAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool isOnly31(const String& s) {
  return s.length() == 2 && s[0] == '3' && s[1] == '1';
}

// Extract clean "C..../T..../TOKEN" substring from noisy chunk
bool extractCFormat(const String& in, String& out) {
  int cpos = in.indexOf('C');
  if (cpos < 0) return false;

  // Expect digits after C
  int i = cpos + 1;
  if (i >= (int)in.length() || !isdigit(in[i])) return false;
  while (i < (int)in.length() && isdigit(in[i])) i++;
  if (i >= (int)in.length() || in[i] != '/') return false;
  i++;  // skip '/'

  // Expect 'T' then digits
  if (i >= (int)in.length() || in[i] != 'T') return false;
  i++;
  if (i >= (int)in.length() || !isdigit(in[i])) return false;
  while (i < (int)in.length() && isdigit(in[i])) i++;
  if (i >= (int)in.length() || in[i] != '/') return false;
  i++;  // skip '/'

  // TOKEN: 2..6 alphabetic chars only
  int tokenStart = i;
  int tokenLen = 0;
  while (i < (int)in.length() && isAlpha(in[i]) && tokenLen < 6) {
    i++;
    tokenLen++;
  }
  if (tokenLen < 2) return false;

  // Slice exact substring
  out = in.substring(cpos, tokenStart + tokenLen);
  return true;
}

// Parse numbers and token from clean code
bool parseBarcode(const String& noisy, uint8_t& outN, uint8_t& outTotal, String& outTokenUpper) {
  if (isOnly31(noisy)) return false;

  String s = noisy;
  s.trim();
  String raw;
  if (!extractCFormat(s, raw)) return false;

  // raw is: C{n}/T{t}/{TOKEN}
  int i = 1;  // after 'C'
  int nStart = i;
  while (i < (int)raw.length() && isdigit(raw[i])) i++;
  if (i >= (int)raw.length() || raw[i] != '/') return false;
  String nStr = raw.substring(nStart, i);
  i++;  // '/'

  if (i >= (int)raw.length() || raw[i] != 'T') return false;
  i++;  // 'T'

  int tStart = i;
  while (i < (int)raw.length() && isdigit(raw[i])) i++;
  if (i >= (int)raw.length() || raw[i] != '/') return false;
  String tStr = raw.substring(tStart, i);
  i++;  // '/'

  String token = raw.substring(i);
  if (token.length() < 2 || token.length() > 6) return false;
  for (size_t k = 0; k < token.length(); ++k)
    if (!isAlpha(token[k])) return false;

  int nVal = nStr.toInt();
  int tVal = tStr.toInt();
  if (nVal < 1 || nVal > 32 || tVal < 1 || tVal > 32) return false;

  // Uppercase token for consistency
  outTokenUpper = token;
  outTokenUpper.toUpperCase();

  outN = (uint8_t)nVal;
  outTotal = (uint8_t)tVal;
  return true;
}

// ------------------------------------------------------------
// Batch / progress
// ------------------------------------------------------------
void resetBatch() {
  expectedTotal = 0;
  scannedMask = 0;
  firstScanTs = 0;
  lastScanTs = 0;
  rawCount = 0;
  batchToken = "";
  for (uint8_t i = 0; i < 32; ++i) {
    rawList[i] = "";
    rawTs[i] = 0;
  }
  for (size_t i = 0; i < BAD_CAP; ++i) {
    badList[i] = "";
    badTs[i] = 0;
  }
  badCount = 0;
}
void recordBad(const String& raw) {
  if (raw.length() == 0) return;
  if (badCount < BAD_CAP) {
    badList[badCount] = raw;
    badTs[badCount] = millis();
    badCount++;
  }
}


bool markScanned(uint8_t n, const String& raw) {
  if (n < 1 || n > 32) return false;
  uint64_t bit = (1ULL << (n - 1));
  if (scannedMask & bit) return false;  // already counted

  scannedMask |= bit;
  if (!firstScanTs) firstScanTs = millis();
  lastScanTs = millis();
  rawList[n - 1] = raw;
  rawTs[n - 1] = lastScanTs;
  rawCount++;
  return true;
}

bool isComplete() {
  if (expectedTotal == 0) return false;
  uint64_t want = (expectedTotal == 64) ? ~0ULL : ((1ULL << expectedTotal) - 1ULL);
  return scannedMask == want;
}

String missingList() {
  String s;
  if (expectedTotal == 0) return s;
  for (uint8_t i = 1; i <= expectedTotal; ++i) {
    if (!(scannedMask & (1ULL << (i - 1)))) {
      if (s.length()) s += ", ";
      s += String(i);
    }
  }
  return s;
}

// ------------------------------------------------------------
// HTML report
// ------------------------------------------------------------
String buildHtmlReport(const String& opName, const String& opEmail, const String& opUidStr) {
  // Compute completion %
  uint8_t total = expectedTotal ? expectedTotal : 0;
  uint8_t done = (uint8_t)rawCount;
  uint8_t pct = (total > 0) ? (uint8_t)((done * 100UL) / total) : 0;
  bool competent = isCompetent();

  // Build missing list
  String missing;
  if (total > 0) {
    for (uint8_t i = 1; i <= total; ++i) {
      if (!(scannedMask & (1ULL << (i - 1)))) {
        if (missing.length()) missing += ", ";
        missing += String(i);
      }
    }
  }

  String html;
  html.reserve(8000);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Softener Backwashing Activity Assessment Report</title>");
  html += F("<style>");
  html += F(".header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:12px}");
  html += F(".brand{display:flex;align-items:center;gap:10px}");
  html += F(".logo{height:40px;width:auto;display:block;border-radius:8px}");
  html += F("@media (prefers-color-scheme:dark){.logo{opacity:.95}}");  // tweak if needed
  html += F(".badge-good{display:inline-block;padding:2px 8px;border-radius:999px;background:#059669;color:#fff;font-size:12px}");
  html += F(".badge-bad{display:inline-block;padding:2px 8px;border-radius:999px;background:#b91c1c;color:#fff;font-size:12px}");
  html += F("@media (prefers-color-scheme:dark){.badge-good{background:#10b981}.badge-bad{background:#ef4444}}");
  html += F(".sub-raw{opacity:.6;font-size:11px;display:block;margin-top:2px}");

  // ---- Base + Dark mode
  html += F("*,*::before,*::after{box-sizing:border-box}html,body{margin:0;padding:0}");
  html += F("body{font-family:Inter,Segoe UI,Arial,Helvetica,sans-serif;line-height:1.45;background:#fafafa;color:#1b1b1b}");
  html += F("@media (prefers-color-scheme:dark){body{background:#0f1115;color:#e5e7eb}}");
  // ---- Container
  html += F(".wrap{max-width:920px;margin:24px auto;padding:0 16px}");
  // ---- Header
  html += F(".header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:12px}");
  html += F(".h-title{font-size:22px;font-weight:800;letter-spacing:.2px}");
  html += F(".sub{opacity:.75;font-size:13px}");
  // ---- Info bar
  html += F(".infobar{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px;margin:12px 0 18px}");
  html += F(".info{padding:10px 12px;border:1px solid rgba(0,0,0,.08);border-radius:12px;background:#fff;box-shadow:0 1px 1px rgba(0,0,0,.04)}");
  html += F("@media (prefers-color-scheme:dark){.info{background:#121621;border-color:rgba(255,255,255,.08)}}");
  html += F(".info b{display:block;font-size:12px;opacity:.7;margin-bottom:4px}");
  html += F(".mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}");
  // ---- Progress
  html += F(".card{padding:14px;border:1px solid rgba(0,0,0,.08);border-radius:14px;background:#fff;box-shadow:0 2px 8px rgba(0,0,0,.05);margin:16px 0}");
  html += F("@media (prefers-color-scheme:dark){.card{background:#111827;border-color:rgba(255,255,255,.08)}}");
  html += F(".prow{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:10px}");
  html += F(".pbar{width:100%;height:12px;border-radius:999px;background:linear-gradient(90deg,#5eead4,#22c55e);box-shadow:inset 0 0 0 1px rgba(0,0,0,.08)}");
  html += F(".pwrap{width:100%;height:12px;border-radius:999px;background:rgba(0,0,0,.08);overflow:hidden}");
  html += F("@media (prefers-color-scheme:dark){.pwrap{background:rgba(255,255,255,.12)}}");
  // ---- Chips grid
  html += F(".chips{display:grid;grid-template-columns:repeat(auto-fill,minmax(44px,1fr));gap:8px;margin:14px 0 6px}");
  html += F(".chip{border-radius:12px;padding:8px 0;text-align:center;font-weight:700;border:1px solid;border-color:transparent}");
  html += F(".ok{background:#ecfdf5;color:#065f46;border-color:#a7f3d0}");
  html += F(".miss{background:#fef2f2;color:#991b1b;border-color:#fecaca}");
  html += F("@media (prefers-color-scheme:dark){.ok{background:#062e26;color:#9ff0d7;border-color:#0f4a3d}.miss{background:#2a1313;color:#fecaca;border-color:#7f1d1d}}");
  // ---- Missing pill
  html += F(".missing{margin:10px 0 2px;font-size:13px;opacity:.85}");
  html += F(".badge{display:inline-block;padding:2px 8px;border-radius:999px;background:#111827;color:#fff;font-size:12px}");
  html += F("@media (prefers-color-scheme:dark){.badge{background:#f3f4f6;color:#111827}}");
  // ---- Table
  html += F(".tbl{width:100%;border-collapse:separate;border-spacing:0;margin-top:14px;overflow:hidden;border-radius:12px}");
  html += F("thead th{position:sticky;top:0;background:#f3f4f6;font-size:12px;text-transform:uppercase;letter-spacing:.04em;color:#374151}");
  html += F("@media (prefers-color-scheme:dark){thead th{background:#0b1020;color:#cbd5e1}}");
  html += F("th,td{padding:10px 12px;border-bottom:1px solid rgba(0,0,0,.06)}");
  html += F("@media (prefers-color-scheme:dark){th,td{border-color:rgba(255,255,255,.08)}}");
  html += F("tbody tr:nth-child(odd){background:rgba(0,0,0,.02)}");
  html += F("@media (prefers-color-scheme:dark){tbody tr:nth-child(odd){background:rgba(255,255,255,.03)}}");
  html += F(".td-mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:12px;word-break:break-all}");
  // ---- Footer
  html += F(".foot{margin:16px 0 8px;font-size:12px;opacity:.7}");
  html += F("</style></head><body><div class='wrap'>");

  // Header
  // Header (with logo)
  html += F("<div class='header'>");

  // Left side: Logo + Title
  html += F("<div class='brand'>");
  html += F("<img class='logo' src='");
  if (LOGO_URL && LOGO_URL[0] != '\0') {
    html += LOGO_URL;
  } else {
    // fallback: tiny transparent PNG (1x1) so layout stays consistent
    html += F("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAuwB9m1g1m0AAAAASUVORK5CYII=");
  }
  html += F("' alt='Logo'>");
  html += F("<div class='h-title'>Softener Backwashing Activity Assessment Report</div>");
  html += F("</div>");  // .brand

  // Right side: firmware tag
  html += F("<div class='sub'>FW 1.2.0</div>");
  html += F("</div>");  // .header


  // Info bar
  html += F("<div class='infobar'>");
  html += F("<div class='info'><b>Status</b>");
  html += resolveTokenName(batchToken);  // product name
  html += F(" — ");
  html += String(done) + F(" / ") + String(total) + F(" complete — ");
  html += (isCompetent() ? F("<span class='badge-good'>Competent</span></div>")
                         : F("<span class='badge-bad'>In-Competent</span></div>"));


  html += F("<div class='info'><b>Department</b>");
  html +=  String("WTP");
  html += F("</div><div class='info'><b>Operator</b>");
  html += opName + F(" &lt;") + opEmail + F("&gt;</div>");
  html += F("</div>");

  // Progress
  html += F("<div class='card'>");
  html += F("<div class='prow'><div><b>Progress</b> — ");
  html += String(pct) + F("%</div><div class='badge'>");
  html += String(done) + F("/") + String(total) + F("</div></div>");
  html += F("<div class='pwrap'><div class='pbar' style='width:");
  html += String(pct);
  html += F("%'></div></div>");
  html += F("</div>");

  // Chips grid
  html += F("<div class='card'><b>Pieces</b>");
  html += F("<div class='chips'>");
  for (uint8_t i = 1; i <= total; ++i) {
    bool got = scannedMask & (1ULL << (i - 1));

    html += F("<tr>");

    // # column
    html += F("<td>");
    html += String(i);
    html += F("</td>");

    // Item column (human-readable)
    html += F("<td>");
    html += prettyLabel(i);  // e.g., "Pigeon Box 1 out of 1"
    if (got) {
      // optional: show the raw code underneath, muted
      html += F("<span class='sub-raw td-mono'>");
      html += rawList[i - 1];
      html += F("</span>");
    }
    html += F("</td>");

    // Timestamp column
    html += F("<td>");
    html += got ? (String((rawTs[i - 1]/1000)) + "seconds") : String("-");
    html += F("</td>");

    // Status column
    html += F("<td>");
    html += got ? F("<span class='badge'>OK</span>")
                : F("<span class='badge'>Missing</span>");
    html += F("</td>");

    html += F("</tr>");
  }

  html += F("</div>");

  // if (badCount > 0) {
  //   html += F("<div class='card'>");
  //   html += F("<b>Incorrect / Unexpected Scans</b>");
  //   html += F("<table class='tbl'><thead><tr>");
  //   html += F("<th style='width:64px'>#</th>");
  //   html += F("<th>Raw</th>");
  //   html += F("<th style='width:180px'>Timestamp (ms)</th>");
  //   html += F("</tr></thead><tbody>");
  //   for (size_t i = 0; i < badCount; ++i) {
  //     html += F("<tr><td>");
  //     html += String(i + 1);
  //     html += F("</td><td class='td-mono'>");
  //     html += badList[i];
  //     html += F("</td><td>");
  //     html += String(badTs[i]);
  //     html += F("</td></tr>");
  //   }
  //   html += F("</tbody></table></div>");
  // }
  if (total && missing.length()) {
    html += F("<div class='missing'><b>Missing:</b> ");
    html += missing;
    html += F("</div>");
  }
  // html += F("</div>");

  // // Table
  // html += F("<div class='card'>");
  // html += F("<table class='tbl'><thead><tr>");
  // html += F("<th style='width:64px'>#</th>");
  // html += F("<th>Item</th>");
  // html += F("<th style='width:180px'>Timestamp (ms)</th>");
  // html += F("<th style='width:110px'>Status</th>");
  // html += F("</tr></thead><tbody>");
  // for (uint8_t i = 1; i <= total; ++i) {
  //   bool got = scannedMask & (1ULL << (i - 1));
  //   html += F("<tr><td>");
  //   html += String(i);
  //   html += F("</td><td class='td-mono'>");
  //   html += got ? rawList[i - 1] : String("-");
  //   html += F("</td><td>");
  //   // NEW: human-readable label
  //   html += got ? prettyLabel(i) : String("-");
  //   // (Optional) also show the raw code in a muted, small line under it:
  //   if (got) {
  //     html += F("<span class='sub-raw td-mono'>");
  //     html += rawList[i - 1];
  //     html += F("</span>");
  //   }
  //   html += F("</td><td>");
  //   html += got ? F("<span class='badge'>OK</span>") : F("<span class='badge'>Missing</span>");
  //   html += F("</td></tr>");
  // }
  // html += F("</tbody></table></div>");

  // // Footer
  // html += F("<div class='foot'>First scan: ");
  // html += String(firstScanTs);
  // html += F(" ms &nbsp;•&nbsp; Last scan: ");
  // html += String(lastScanTs);
  // html += F(" ms</div>");

  html += F("</div></body></html>");
  return html;
}


// ------------------------------------------------------------
// Email send
// ------------------------------------------------------------
bool sendEmailReport(const String& subject, const String& htmlBody, const String& toEmail, const String& ccEmail) {
  smtp.debug(0);

  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = SMTP_USER;
  config.login.password = SMTP_PASS;
  config.login.user_domain = F("local");

  SMTP_Message message;
  message.sender.name = F("Scanner Report");
  message.sender.email = SMTP_USER;
  message.subject = subject;
  message.addRecipient(F("To"), toEmail.c_str());
  if (ccEmail.length()) message.addRecipient(F("CC"), ccEmail.c_str());

  message.text.content = F("Your email client does not support HTML.");
  message.text.charSet = F("utf-8");
  message.html.content = htmlBody.c_str();
  message.html.charSet = F("utf-8");

  bool ok = smtp.connect(&config);
  if (!ok) {
    Serial.printf("[SMTP] Connect failed: %s\n", smtp.errorReason().c_str());
    return false;
  }
  ok = MailClient.sendMail(&smtp, &message, true /* close */);
  if (!ok) {
    Serial.printf("[SMTP] Send failed: %s\n", smtp.errorReason().c_str());
    return false;
  }
  Serial.println("[SMTP] Report sent.");
  return true;
}

// ------------------------------------------------------------
// RFID helpers
// ------------------------------------------------------------
bool sameUid(const MFRC522::Uid& a, const uint8_t* b, uint8_t blen) {
  if (a.size != blen) return false;
  for (uint8_t i = 0; i < blen; i++)
    if (a.uidByte[i] != b[i]) return false;
  return true;
}
String uidToHex(const MFRC522::Uid& u) {
  String s;
  for (uint8_t i = 0; i < u.size; i++) {
    if (i) s += ":";
    uint8_t v = u.uidByte[i];
    if (v < 16) s += "0";
    s += String(v, HEX);
  }
  s.toUpperCase();
  return s;
}
const Operator* resolveOperator(const MFRC522::Uid& uid) {
  for (size_t i = 0; i < operatorsCount; i++)
    if (sameUid(uid, operators[i].uid, operators[i].uidLen)) return &operators[i];
  return nullptr;
}

// ------------------------------------------------------------
// Handle a completed chunk
// ------------------------------------------------------------
static const size_t RAW_LOG_MIN = 4;  // only log meaningful chunks

void handleBarcodeLine(const String& code) {
  if (code.length() == 0 || isOnly31(code)) return;
  if (seenContains(code)) {
    // (Optional) treat exact raw duplicates as “bad”; uncomment if you want to see them in report
    // recordBad("[DUPLICATE] " + code);
    return;
  }

  uint8_t n = 0, total = 0;
  String token;

  // A) Hard parse failure -> incorrect QR
  if (!parseBarcode(code, n, total, token)) {
    recordBad("[PARSE_FAIL] " + code);
    return;
  }

  // B) First valid locks batch; else validate total/token
if (expectedTotal == 0) {
  expectedTotal = total;
  if (expectedTotal > 32) expectedTotal = 32;
  batchToken = token;
  Serial.printf("[BATCH] total=%u, token=%s\n", expectedTotal, batchToken.c_str());

  // initialize expected sequence *after* we know total
  initExpectedSequence(expectedTotal);
} else {
  if (total != expectedTotal) {
    Serial.printf("[BATCH] Mismatch total (got %u, expect %u)\n", total, expectedTotal);
    recordBad("[TOTAL_MISMATCH] " + code);
    return;
  }
  if (token != batchToken) {
    Serial.printf("[BATCH] Token mismatch (got %s, expect %s)\n", token.c_str(), batchToken.c_str());
    recordBad("[TOKEN_MISMATCH] " + code);
    return;
  }
}

// ---- NEW: Strict sequence gate ----
if (enforceSequence) {
  if (nextExpectedPos == 0 || nextExpectedPos > expectedLen) {
    //recordBad("[SEQ_STATE_ERROR] " + code);
    return;
  }
  uint8_t expectedIndexNow = expectedSequence[nextExpectedPos - 1]; // sequence is 0-based
  if (n != expectedIndexNow) {
    // don't count it; mark incorrect
    String msg = "[SEQUENCE_MISMATCH] expected C" + String(expectedIndexNow) + " got C" + String(n) + " (" + code + ")";
    Serial.println(msg);
    //recordBad(msg);
    return;
  }
}

// C) Duplicate index -> incorrect
if (!markScanned(n, code)) {
  recordBad("[DUP_INDEX] " + code);
  return;
}

// advance sequence pointer only when we successfully accepted the *expected* piece
if (enforceSequence && nextExpectedPos <= expectedLen) {
  nextExpectedPos++;
}


  seenInsert(code);
  lastAcceptMs = millis();


  Serial.printf("BARCODE #%u accepted. Progress: %u/%u\n", n, rawCount, expectedTotal);
}


void processCompletedBarcode() {
  if (tlen == 0) return;

  size_t start = 0, end = tlen;
  while (start < end && (txtBuf[start] == ' ' || txtBuf[start] == '\t')) start++;
  while (end > start && (txtBuf[end - 1] == ' ' || txtBuf[end - 1] == '\t')) end--;

  String chunk;
  if (end > start) {
    chunk.reserve(end - start);
    for (size_t i = start; i < end; ++i) chunk += txtBuf[i];
  }
  tlen = 0;

  // Log useful chunks (skip bare "31")
  if (!isOnly31(chunk) && chunk.length() >= RAW_LOG_MIN) {
    Serial.print("[RAW] ");
    for (size_t i = 0; i < chunk.length(); ++i) {
      uint8_t b = (uint8_t)chunk[i];
      if (b < 0x10) Serial.print('0');
      Serial.print(b, HEX);
      Serial.print(' ');
    }
    Serial.print("  |  ");
    Serial.println(chunk);
  }
  if (isOnly31(chunk)) return;  // ignore noise cleanly

  handleBarcodeLine(chunk);

  // Re-trigger if armed and cooled down
  if (armed && (millis() - lastAcceptMs >= ACCEPT_COOLDOWN_MS)) {
    delay(10);
    GM.write(CMD_START, sizeof(CMD_START));
  }
}

// ------------------------------------------------------------
// Finalize on RFID
// ------------------------------------------------------------
void tryFinalizeAndEmail(const MFRC522::Uid& uid) {
  String uidHex = uidToHex(uid);
  const Operator* op = resolveOperator(uid);

  // if (!isComplete()) {
  //   String miss = missingList();
  //   Serial.printf("[FINALIZE] Not complete. Missing: [%s]\n", miss.c_str());
  //   return;
  // }

  String opName = op ? String(op->name) : String("Unknown");
  String opEmail = op ? String(op->email) : String(MAIL_TO_FALLBACK);

  String subject = "Softener Backwashing Activity Assessment Report " + batchToken + " (" + String(rawCount) + "/" + String(expectedTotal) + ")";

  if (!isCompetent()) {
    subject += " [IN-COMPETENT]";
    if (rawCount != expectedTotal) subject += " [MISSING=" + String(expectedTotal - rawCount) + "]";
    if (badCount > 0) subject += " [INCORRECT=" + String(badCount) + "]";
  } else {
    subject += " [COMPETENT]";
  }
  String html = buildHtmlReport(opName, opEmail, uidHex);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[NET] Wi-Fi not connected. Cannot send.");
    return;
  }

  Serial.println("[EMAIL] Sending...");
  bool ok = sendEmailReport(subject, html, opEmail, MAIL_TO_FALLBACK);
  if (ok) {
    Serial.println("[FINALIZE] Email sent. Resetting batch.");
    resetBatch();
    sendStop();
  }
}

// ------------------------------------------------------------
// Setup / Loop
// ------------------------------------------------------------
void setup() {
  delay(200);
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[ESP32 <-> GM75] New format Cn/Tt/TOKEN + RFID + Email (auto-size)");

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; ++i) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Not connected (will still scan; email requires Wi-Fi).");
  }

  // Buttons
  if (USE_BUTTONS) {
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_BTN_STOP, INPUT_PULLUP);
  }

  // GM75 UART
  GM.begin(BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  Serial.printf("UART2 @ %lu 8N1  (RX=%d, TX=%d)\n", (unsigned long)BAUD, PIN_RX, PIN_TX);
  Serial.println("Controls: 't' start, 's' stop.");

  // RC522
  SPI.begin();  // VSPI defaults
  mfrc522.PCD_Init();
  Serial.println("[RC522] Ready. Tap card to finalize when complete.");

  resetBatch();
}

void loop() {
  // Console controls
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 't' || c == 'T') sendStart();
    if (c == 's' || c == 'S') sendStop();
  }

  // Buttons
  if (USE_BUTTONS) {
    uint32_t now = millis();
    if (digitalRead(PIN_BTN_START) == LOW && (now - lastStartPress > DEBOUNCE_MS)) {
      lastStartPress = now;
      sendStart();
    }
    if (digitalRead(PIN_BTN_STOP) == LOW && (now - lastStopPress > DEBOUNCE_MS)) {
      lastStopPress = now;
      sendStop();
    }
  }

  // GM75 stream
  while (GM.available() > 0) {
    int bi = GM.read();
    if (bi < 0) break;
    uint8_t b = (uint8_t)bi;

    // Binary frame?
    if (!inFrame && b == 0x7E) {
      inFrame = true;
      flen = 0;
      frmBuf[flen++] = b;
      continue;
    }
    if (inFrame) {
      if (flen < FBUF_CAP) frmBuf[flen++] = b;
      if (GM.available() == 0) {
        if (LOG_FRAMES_HEX) {
          Serial.print("[GM75] RX FRAME: ");
          hexDump(frmBuf, flen);
          Serial.println();
        }
        inFrame = false;
        flen = 0;
      }
      continue;
    }

    // ASCII text path
    lastByteMs = millis();
    if (b == '\r' || b == '\n') continue;
    if (b >= 0x20 && b <= 0x7E) {
      if (tlen < BUF_CAP) txtBuf[tlen++] = (char)b;
      else {
        processCompletedBarcode();
        txtBuf[0] = (char)b;
        tlen = 1;
      }
    }
  }

  // Idle gap -> chunk complete
  if (tlen > 0 && (millis() - lastByteMs) >= IDLE_TIMEOUT_MS) {
    processCompletedBarcode();
  }

  // RFID finalize
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.print("[RC522] UID: ");
    Serial.println(uidToHex(mfrc522.uid));
    tryFinalizeAndEmail(mfrc522.uid);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  delay(1);
}
