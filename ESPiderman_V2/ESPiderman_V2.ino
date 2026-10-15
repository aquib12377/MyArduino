#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ═══════════════════════════════════════════════════════════════════════
//  WiFi AP Credentials
// ═══════════════════════════════════════════════════════════════════════
static const char* ssid     = "ESPiderman";
static const char* password = "12345678";

// ═══════════════════════════════════════════════════════════════════════
//  I2C Pins (non-default to avoid camera pin conflicts on AI-Thinker)
// ═══════════════════════════════════════════════════════════════════════
static constexpr uint8_t I2C_SDA = 14;
static constexpr uint8_t I2C_SCL = 15;

// ═══════════════════════════════════════════════════════════════════════
//  Communication / State Variables
// ═══════════════════════════════════════════════════════════════════════
static bool lightOn  = false;
static bool arm      = true;
static bool wave     = false;

static int ch_1 = 1500;   // Forward / Backward
static int ch_2 = 1500;   // Left / Right (turn & look)
static int ch_3 = 1500;   // Up / Down (camera tilt)
static int ch_5 = 1000;   // Arm toggle
static int ch_6 = 1000;   // Horn
static int ch_7 = 1000;   // Height state
static int ch_7_old = 1500;
static int ch_8 = 1000;   // Mode

// Feedback data buffer (increased from 70 to prevent overflow)
static char dataToClient[150];

// Feedback state strings
static String light   = "1000";
static String armed   = "1000";
static String height  = "1000";
static String mode    = "1000";
static String footFL  = "1000";
static String footFR  = "1000";
static String footBL  = "1000";
static String footBR  = "1000";
static String temp    = " ";
static String rotX    = " ";
static String rotY    = " ";
static String rotZ    = " ";
static String distFr  = " ";
static String distFrL = " ";
static String distFrR = " ";
static String distBott = " ";

// ═══════════════════════════════════════════════════════════════════════
//  Servo Configuration
// ═══════════════════════════════════════════════════════════════════════
//  Leg layout:
//    Leg 1 (Front Right) → Ch 0 (hip), Ch 1 (knee)
//    Leg 2 (Front Left)  → Ch 2 (hip), Ch 3 (knee)
//    Leg 3 (Back Left)   → Ch 4 (hip), Ch 5 (knee)
//    Leg 4 (Back Right)  → Ch 6 (hip), Ch 7 (knee)
// ═══════════════════════════════════════════════════════════════════════
static constexpr int NUM_SERVOS = 8;

// PCA9685 pulse range for 0°–180°
static constexpr int PULSE_MIN = 150;
static constexpr int PULSE_MAX = 650;

// Per-servo calibration compensation
//   comp[6] = 20 fixes the back-right hip angle constraint issue:
//   shifts working range from (a6r + 70) to (a6r + 90), preventing
//   the servo from clipping at 0° during turn-left gaits.
static int comp[NUM_SERVOS] = { 0, 0, 0, 0, 0, 0, 20, 0 };

// Per-servo direction: +1 = direct, -1 = inverted
static const int8_t servoDir[NUM_SERVOS] = { +1, -1, -1, +1, -1, -1, +1, +1 };

// Posture offsets
static int korr  = 25;   // Idle posture offset (0 for calibration, 25 for use)
static int korr2 = 5;    // Center-of-gravity offset (0 for calibration, 5 for use)

// Outer leg idle angle
static constexpr int LEG_IDLE = 30;  // 0 for calibration, 30 for use

// Deadband to avoid servo jitter
static constexpr int EPS = 3;

// Servo speed delay (ms per step in ramped controller)
static int v    = 3;
static int vOld = 0;

// Delay for liftLeg / lowerLeg to complete movement (ms)
static constexpr int LIFT_DELAY = 75;

// Step counter for gait state machine
static int step = 0;

// ═══════════════════════════════════════════════════════════════════════
//  Height Profiles
// ═══════════════════════════════════════════════════════════════════════
struct HeightProfile {
  int stepLength;
  int stepLift;
  int speed;
  int korrCG;     // center-of-gravity offset
};

static const HeightProfile PROFILE_LOW  = { 35, 35, 3,  5 };
static const HeightProfile PROFILE_MID  = { 30, 40, 6, 10 };
static const HeightProfile PROFILE_HIGH = { 30, 45, 9, 10 };

// Active step dimensions (set per height)
static int stepLength = 0;
static int stepLift   = 0;

// ═══════════════════════════════════════════════════════════════════════
//  Servo Angle Arrays
//    aT[i] = target angle,  aR[i] = current angle (for ramped controller)
// ═══════════════════════════════════════════════════════════════════════
static int aT[NUM_SERVOS] = {0};
static int aR[NUM_SERVOS] = {0};

// Convenient aliases to keep gait logic readable
static int &a0 = aT[0], &a1 = aT[1], &a2 = aT[2], &a3 = aT[3];
static int &a4 = aT[4], &a5 = aT[5], &a6 = aT[6], &a7 = aT[7];
static int &a0r = aR[0], &a1r = aR[1], &a2r = aR[2], &a3r = aR[3];
static int &a4r = aR[4], &a5r = aR[5], &a6r = aR[6], &a7r = aR[7];

// ═══════════════════════════════════════════════════════════════════════
//  PCA9685 Driver
// ═══════════════════════════════════════════════════════════════════════
static Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// ═══════════════════════════════════════════════════════════════════════
//  Camera Model: AI-Thinker ESP32-CAM
// ═══════════════════════════════════════════════════════════════════════
#define CAMERA_MODEL_AI_THINKER
#define LED_BUILTIN 4

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ═══════════════════════════════════════════════════════════════════════
//  HTTP Streaming Constants
// ═══════════════════════════════════════════════════════════════════════
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static httpd_handle_t camera_httpd = NULL;
static httpd_handle_t stream_httpd = NULL;

// ═══════════════════════════════════════════════════════════════════════
//  Web UI HTML
// ═══════════════════════════════════════════════════════════════════════
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>
  <head>
    <title>delta3Robotics</title>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
    <style>
      :root{
        --back: #858080;
        --backLog: darkgrey;
        --activeData: purple;
        --font: black;
        --BtnBackground: gray;
        --btnBorder: black;
        --BtnBackgroundDis: rgb(87, 86, 86);
        --BtnLegAct: purple;
        --btn-size: clamp(36px, 8vmin, 60px);
        --btn-font: clamp(11px, 2.5vmin, 18px);
      }
      * { box-sizing: border-box; }
      body {
        font-family: Arial;
        text-align: center;
        background-color: var(--back);
        margin: 0;
        padding: 0;
        width: 100%;
        height: 100vh;
        overflow: hidden;
      }
      table { margin: 0; }
      td { padding: 2px; }
      .log {
        color: var(--activeData);
        font-size: clamp(9px, 2vmin, 13px);
        text-align: left;
        margin: 0 0 0 5px;
      }
      .var {
        color: var(--font);
        font-size: clamp(9px, 2vmin, 13px);
        text-align: left;
        margin: 0 0 0 5px;
      }
      .height {
        color: var(--activeData);
        font-size: clamp(12px, 3vmin, 17px);
        text-align: left;
        margin: 0 0 0 5px;
      }
      .button {
        color: var(--font);
        background-color: var(--BtnBackground);
        border: solid;
        border-color: var(--btnBorder);
        border-radius: 100%;
        width: var(--btn-size);
        height: var(--btn-size);
        text-align: center;
        text-decoration: none;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        font-size: var(--btn-font);
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
        touch-action: manipulation;
      }
      .button:active { border-color: var(--activeData); }
      .buttonDisabled {
        color: black;
        background-color: var(--back);
        border: none;
        border-radius: 100%;
        width: var(--btn-size);
        height: var(--btn-size);
        display: inline-flex;
        align-items: center;
        justify-content: center;
        font-size: var(--btn-font);
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
        touch-action: manipulation;
      }
      .buttondisabled:active { background-color: var(--back); }
      .buttonLeg {
        color: var(--font);
        background-color: var(--BtnLegAct);
        border: solid;
        border-radius: 100%;
        width: clamp(28px, 6vmin, 40px);
        height: clamp(28px, 6vmin, 40px);
        display: inline-flex;
        align-items: center;
        justify-content: center;
        font-size: var(--btn-font);
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
        touch-action: manipulation;
      }
      .buttonLeg:active { background-color: var(--activeData); }
      .containerA {
        display: grid;
        width: 100%;
        height: 100%;
        grid-template-columns: 20% 60% 20%;
        grid-template-rows: 10% 20% 35% 30%;
        column-gap: 4px;
        row-gap: 4px;
        justify-items: center;
        align-items: center;
      }
      .item-info1 {
        padding-left: 10px;
        grid-column: 1;
        grid-row: 1 / 3;
        justify-self: stretch;
        align-self: stretch;
        line-height: 140%;
        overflow: hidden;
      }
      .item-info2 {
        grid-column: 1;
        grid-row: 3;
        align-self: stretch;
        justify-self: stretch;
        overflow: hidden;
      }
      .item-bt1 { grid-column: 1; grid-row: 4; }
      .item-heading {
        grid-column: 2;
        grid-row: 1;
        justify-self: center;
        align-self: center;
        font-size: clamp(16px, 4vw, 60px);
        white-space: nowrap;
      }
      .item-stream {
        grid-column: 2;
        grid-row: 2 / 5;
        justify-self: stretch;
        align-self: stretch;
        overflow: hidden;
        display: flex;
        align-items: center;
        justify-content: center;
      }
      .item-bt3 { grid-column: 3; grid-row: 1 / 3; }
      .item-info3 { grid-column: 3; grid-row: 3; }
      .item-bt2 { grid-column: 3; grid-row: 4; }
      @media (orientation: portrait), (max-width: 600px) {
        body { overflow-y: auto; height: auto; }
        .containerA {
          grid-template-columns: 1fr 1fr;
          grid-template-rows: auto auto auto auto auto;
          height: auto;
        }
        .item-heading {
          grid-column: 1 / 3; grid-row: 1;
          font-size: clamp(18px, 5vw, 36px); padding: 6px 0;
        }
        .item-stream {
          grid-column: 1 / 3; grid-row: 2;
          height: 45vw; max-height: 260px;
        }
        .item-bt1 { grid-column: 1; grid-row: 3; }
        .item-bt3 { grid-column: 2; grid-row: 3; }
        .item-info1 {
          grid-column: 1; grid-row: 4;
          align-self: start; font-size: clamp(8px, 2.5vw, 12px);
        }
        .item-info2 { display: none; }
        .item-info3 { grid-column: 2; grid-row: 4; }
        .item-bt2 { grid-column: 1 / 3; grid-row: 5; padding-bottom: 8px; }
      }
      .containerB {
        display: grid;
        width: 100%;
        height: 100%;
        grid-template-columns: 50% 50%;
        grid-template-rows: 45% 10% 45%;
        justify-items: center;
        align-items: center;
      }
      .item-FL { grid-column:1; grid-row:1; }
      .item-FR { grid-column:2; grid-row:1; }
      .item-Height { grid-column:1/3; grid-row:2; }
      .item-BL { grid-column:1; grid-row:3; }
      .item-BR { grid-column:2; grid-row:3; }
      img {
        width: 100%;
        height: 100%;
        object-fit: contain;
        display: block;
      }
    </style>
  </head>
  <body>
    <div class="containerA">
        <div class="item-info1">
          <div><h1 class=log id="Row11"></h1></div>
          <div><h1 class=log id="Row10"></h1></div>
          <div><h1 class=log id="Row9" ></h1></div>
          <div><h1 class=log id="Row8" ></h1></div>
          <div><h1 class=log id="Row7" ></h1></div>
          <div><h1 class=log id="Row6" ></h1></div>
          <div><h1 class=log id="Row5" ></h1></div>
          <div><h1 class=log id="Row4" ></h1></div>
          <div><h1 class=log id="Row3" ></h1></div>
          <div><h1 class=log id="Row2" ></h1></div>
          <div><h1 class=log id="Row1" ></h1></div>
          <div><h1 class=log id="Row0" ></h1></div>
        </div>
        <div class="item-info2">
          <table>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logTemp"  ></h1></td><td><h1 class="var"></h1></td></tr>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logRotX"  ></h1></td><td><h1 class="var"></h1></td></tr>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logRotY"  ></h1></td><td><h1 class="var"></h1></td></tr>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logRotZ"  ></h1></td><td><h1 class="var"></h1></td></tr>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logDistFr" ></h1></td><td><h1 class="var"></h1></td></tr>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logDistFrL"></h1></td><td><h1 class="var"></h1></td></tr>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logDistFrR"></h1></td><td><h1 class="var"></h1></td></tr>
            <tr><td><h1 class="var"></h1></td><td><h1 class="log" id="logDistBot"></h1></td><td><h1 class="var"></h1></td></tr>
          </table>
        </div>
        <div class="item-bt1">
            <table>
                <tr>
                  <td><button class="buttonDisabled" ontouchstart="toggleCheckbox('A');"  ontouchend="toggleCheckbox('d');"></button></td>
                  <td><button class="button"         ontouchstart="toggleCheckbox('W');"  ontouchend="toggleCheckbox('w');">W</button></td>
                  <td><button class="buttonDisabled" ontouchstart="toggleCheckbox('D');"  ontouchend="toggleCheckbox('d');"></button></td>
                </tr>
                <tr>
                  <td><button class="button"         ontouchstart="toggleCheckbox('TL');" ontouchend="toggleCheckbox('tl');">TL</button></td>
                  <td><button class="buttonDisabled" style="background-color:gray;"></button></td>
                  <td><button class="button"         ontouchstart="toggleCheckbox('TR');" ontouchend="toggleCheckbox('tl');">TR</button></td>
                </tr>
                <tr>
                  <td><button class="buttonDisabled"></button></td>
                  <td><button class="button"         ontouchstart="toggleCheckbox('S');"  ontouchend="toggleCheckbox('w');">S</button></td>
                  <td><button class="buttonDisabled"></button></td>
                </tr>
            </table>
        </div>
        <div class="item-heading">delta3Robotics</div>
        <div class="item-stream">
            <img src="" id="photo">
        </div>
        <div class="item-bt3">
            <table>
                <tr>
                  <td><button id="wave" class="button" ontouchstart="toggleCheckbox('WAVE');" ontouchend="toggleCheckbox('wave');">wave</button></td>
                  <td><button class="buttonDisabled"></button></td>
                  <td><button id="ARM" class="button" onclick="toggleCheckbox('ARM');">ARM</button></td>
                </tr>
                <tr>
                  <td><button id="LIGHT" class="button" ontouchstart="toggleCheckbox('LIGHT'); toggleLIGHT();" style="font-size:clamp(10px,2.5vmin,15px);">LIGHT</button></td>
                  <td><button class="buttonDisabled"></button></td>
                  <td><button id="HEIGHT" class="button" ontouchstart="toggleCheckbox('HEIGHT');" style="font-size:clamp(8px,2vmin,11px);">HEIGHT</button></td>
                </tr>
                <tr>
                  <td><button class="buttonDisabled" ontouchstart="toggleCheckbox('HORN');" ontouchend="toggleCheckbox('horn');"></button></td>
                  <td><button class="buttonDisabled"></button></td>
                  <td><button id="MODE" class="buttonDisabled" ontouchstart="toggleCheckbox('MODE');" style="font-size:clamp(9px,2vmin,13px);"></button></td>
                </tr>
            </table>
        </div>
        <div class="item-info3">
          <div class="containerB">
            <div class="item-FL"><button class="buttonDisabled" id="FrontLeft"></button></div>
            <div class="item-FR"><button class="buttonDisabled" id="FrontRight"></button></div>
            <div class="item-Height"><h1 class="height" id="height">Height 1</h1></div>
            <div class="item-BL"><button class="buttonDisabled" id="BackLeft"></button></div>
            <div class="item-BR"><button class="buttonDisabled" id="BackRight"></button></div>
          </div>
        </div>
        <div class="item-bt2">
          <table>
            <tr>
              <td><button class="buttonDisabled"></button></td>
              <td><button class="button" ontouchstart="toggleCheckbox('U');"    ontouchend="toggleCheckbox('u');">U</button></td>
              <td><button class="buttonDisabled"></button></td>
            </tr>
            <tr>
              <td><button class="button" ontouchstart="toggleCheckbox('L');"    ontouchend="toggleCheckbox('r');">L</button></td>
              <td><button class="button" ontouchstart="toggleCheckbox('RSTR');">RST</button></td>
              <td><button class="button" ontouchstart="toggleCheckbox('R');"    ontouchend="toggleCheckbox('r');">R</button></td>
            </tr>
            <tr>
              <td><button class="buttonDisabled"></button></td>
              <td><button class="button" ontouchstart="toggleCheckbox('DO');"   ontouchend="toggleCheckbox('u');">D</button></td>
              <td><button class="buttonDisabled"></button></td>
            </tr>
          </table>
        </div>
    </div>
   <script>
    function toggleCheckbox(x) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/action?go=" + x, true);
      xhr.onload = function() {
        if (xhr.status == 200 && xhr.responseText !== "") {
          var Row0  = document.getElementById("Row0");
          var Row1  = document.getElementById("Row1");
          var Row2  = document.getElementById("Row2");
          var Row3  = document.getElementById("Row3");
          var Row4  = document.getElementById("Row4");
          var Row5  = document.getElementById("Row5");
          var Row6  = document.getElementById("Row6");
          var Row7  = document.getElementById("Row7");
          var Row8  = document.getElementById("Row8");
          var Row9  = document.getElementById("Row9");
          var Row10 = document.getElementById("Row10");
          var Row11 = document.getElementById("Row11");
          Row11.innerText = Row10.innerText;
          Row10.innerText = Row9.innerText;
          Row9.innerText  = Row8.innerText;
          Row8.innerText  = Row7.innerText;
          Row7.innerText  = Row6.innerText;
          Row6.innerText  = Row5.innerText;
          Row5.innerText  = Row4.innerText;
          Row4.innerText  = Row3.innerText;
          Row3.innerText  = Row2.innerText;
          Row2.innerText  = Row1.innerText;
          Row1.innerText  = Row0.innerText;
          Row0.innerText  = xhr.responseText;
        }
      };
      xhr.send();
    }
    var timer = setInterval(getFeedBack, 200);
    function getFeedBack() {
      var xhfr = new XMLHttpRequest();
      xhfr.open("GET", "/action?go=feedBack", true);
      xhfr.onload = function(){
        if (xhfr.status == 200){
          var text = xhfr.responseText;
          var textarray = text.split(",");
          var lightBtn = document.getElementById("LIGHT");
          if(textarray[0]=="1000") lightBtn.style.backgroundColor="gray";
          if(textarray[0]=="1500") lightBtn.style.backgroundColor="purple";
          if(textarray[0]=="2000") lightBtn.style.backgroundColor="green";
          var armedBtn = document.getElementById("ARM");
          if(textarray[1]=="1000") armedBtn.style.backgroundColor="gray";
          if(textarray[1]=="2000") armedBtn.style.backgroundColor="purple";
          var heightLog = document.getElementById("height");
          var heightBtn = document.getElementById("HEIGHT");
          if(textarray[2]=="1000"){ heightLog.innerText="Height 1"; heightBtn.style.backgroundColor="gray"; }
          if(textarray[2]=="1500"){ heightLog.innerText="Height 2"; heightBtn.style.backgroundColor="purple"; }
          if(textarray[2]=="2000"){ heightLog.innerText="Height 3"; heightBtn.style.backgroundColor="green"; }
          var modeBtn = document.getElementById("MODE");
          if(textarray[3]=="1000") modeBtn.style.backgroundColor="gray";
          if(textarray[3]=="1500") modeBtn.style.backgroundColor="purple";
          if(textarray[3]=="2000") modeBtn.style.backgroundColor="green";
          var legFLBtn = document.getElementById("FrontLeft");
          if(textarray[4]=="1000") legFLBtn.style.backgroundColor="gray";
          if(textarray[4]=="2000") legFLBtn.style.backgroundColor="purple";
          var legFRBtn = document.getElementById("FrontRight");
          if(textarray[5]=="1000") legFRBtn.style.backgroundColor="gray";
          if(textarray[5]=="2000") legFRBtn.style.backgroundColor="purple";
          var legBLBtn = document.getElementById("BackLeft");
          if(textarray[6]=="1000") legBLBtn.style.backgroundColor="gray";
          if(textarray[6]=="2000") legBLBtn.style.backgroundColor="purple";
          var legBRBtn = document.getElementById("BackRight");
          if(textarray[7]=="1000") legBRBtn.style.backgroundColor="gray";
          if(textarray[7]=="2000") legBRBtn.style.backgroundColor="purple";
          document.getElementById("logTemp"  ).innerText = textarray[8];
          document.getElementById("logRotX"  ).innerText = textarray[9];
          document.getElementById("logRotY"  ).innerText = textarray[10];
          document.getElementById("logRotZ"  ).innerText = textarray[11];
          document.getElementById("logDistFr" ).innerText = textarray[12];
          document.getElementById("logDistFrL").innerText = textarray[13];
          document.getElementById("logDistFrR").innerText = textarray[14];
          document.getElementById("logDistBot").innerText = textarray[15];
        }
      };
      xhfr.send();
    }
    window.onload = function() {
      var base = window.location.href.replace(/\/$/, "");
      document.getElementById("photo").src = base + ":81/stream";
    };
  </script>
  </body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════════════════
//  Servo Helpers
// ═══════════════════════════════════════════════════════════════════════

// Compute the per-servo base offset (depends on runtime korr/korr2)
static int getServoOffset(int ch) {
  switch (ch) {
    case 0: return 90 + korr;            // FR hip
    case 1: return 90;                   // FR knee
    case 2: return 90 - korr;            // FL hip
    case 3: return 90;                   // FL knee
    case 4: return 90 + korr - korr2;    // BL hip
    case 5: return 90;                   // BL knee
    case 6: return 90 - korr + korr2;    // BR hip
    case 7: return 90;                   // BR knee
    default: return 90;
  }
}

// Convert logical angle → physical servo angle
static inline int toPhysical(int ch, int angle) {
  return getServoOffset(ch) + servoDir[ch] * angle + comp[ch];
}

// Drive a single servo to a physical angle (0–180)
static void setServo(int servo, int angle) {
  int duty = map(angle, 0, 180, PULSE_MIN, PULSE_MAX);
  pwm.setPWM(servo, 0, duty);
}

// Instantly move a knee servo down by stepLift (used in gait transitions)
static void liftLeg(int ch) {
  aT[ch] -= stepLift;
  aR[ch]  = aT[ch];
  setServo(ch, toPhysical(ch, aR[ch]));
  delay(LIFT_DELAY);
}

// Instantly move a knee servo up by stepLift
static void lowerLeg(int ch) {
  aT[ch] += stepLift;
  aR[ch]  = aT[ch];
  setServo(ch, toPhysical(ch, aR[ch]));
  delay(LIFT_DELAY);
}

// Apply height profile to active gait parameters
static void applyProfile(const HeightProfile &p) {
  stepLength = p.stepLength;
  stepLift   = p.stepLift;
  korr2      = p.korrCG;
}

// Select speed by current height
static int speedForHeight() {
  if (height.equals("2000")) return PROFILE_HIGH.speed;
  if (height.equals("1500")) return PROFILE_MID.speed;
  return PROFILE_LOW.speed;
}

// ═══════════════════════════════════════════════════════════════════════
//  HTTP Handlers
// ═══════════════════════════════════════════════════════════════════════
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[ERROR] Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->width > 400) {
        if (fb->format != PIXFORMAT_JPEG) {
          bool ok = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!ok) {
            Serial.println("[ERROR] JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) break;
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char variable[150] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len <= 1) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  buf = (char *)malloc(buf_len);
  if (!buf) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK ||
      httpd_query_key_value(buf, "go", variable, sizeof(variable)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  // Flip camera vertically
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);

  // ── Process commands ──────────────────────────────────────────────

  // Feedback request
  if (!strcmp(variable, "feedBack")) {
    strcpy(variable, dataToClient);
  }

  // LIGHT toggle — FIX: was double-if (could never turn on)
  else if (!strcmp(variable, "LIGHT")) {
    if (!lightOn) {
      lightOn = true;
      strcpy(variable, "Light switched on");
      light = "1500";
    } else {
      lightOn = false;
      strcpy(variable, "Light switched off");
      light = "1000";
    }
  }

  // Forward / Backward (Ch 1)
  else if (!strcmp(variable, "W"))  { ch_1 = 2000; strcpy(variable, ""); }
  else if (!strcmp(variable, "w"))  { ch_1 = 1500; strcpy(variable, ""); }
  else if (!strcmp(variable, "S"))  { ch_1 = 1000; strcpy(variable, ""); }

  // Camera / Look (Ch 2)
  else if (!strcmp(variable, "R"))  { ch_2 = min(ch_2 + 25, 1749); strcpy(variable, ""); }
  else if (!strcmp(variable, "L"))  { ch_2 = max(ch_2 - 25, 1251); strcpy(variable, ""); }
  else if (!strcmp(variable, "TL")) { ch_2 = 1000; strcpy(variable, ""); }
  else if (!strcmp(variable, "TR")) { ch_2 = 2000; strcpy(variable, ""); }
  else if (!strcmp(variable, "tl")) { ch_2 = 1500; strcpy(variable, ""); }
  else if (!strcmp(variable, "r"))  { strcpy(variable, ""); }

  // Camera tilt (Ch 3)
  else if (!strcmp(variable, "U"))  { ch_3 = min(ch_3 + 50, 2000); strcpy(variable, ""); }
  else if (!strcmp(variable, "DO")) { ch_3 = max(ch_3 - 50, 1000); strcpy(variable, ""); }
  else if (!strcmp(variable, "u"))  { strcpy(variable, ""); }

  // Reset view
  else if (!strcmp(variable, "RSTR")) { ch_2 = 1500; ch_3 = 1500; strcpy(variable, "Reset View"); }

  // Strafe (Ch 4) — mostly unused
  else if (!strcmp(variable, "D"))  { strcpy(variable, ""); }
  else if (!strcmp(variable, "d"))  { strcpy(variable, ""); }
  else if (!strcmp(variable, "A"))  { strcpy(variable, ""); }

  // ARM toggle — FIX: was double-if (toggled twice per press)
  else if (!strcmp(variable, "ARM")) {
    if (arm) {
      ch_5 = 1000;
      strcpy(variable, "Robot disarmed");
      arm = false;
      armed = "1000";
    } else {
      ch_5 = 2000;
      strcpy(variable, "Robot armed");
      arm = true;
      armed = "2000";
    }
  }

  // Horn (Ch 6)
  else if (!strcmp(variable, "HORN")) { ch_6 = 2000; strcpy(variable, ""); }
  else if (!strcmp(variable, "horn")) { ch_6 = 1000; strcpy(variable, ""); }

  // HEIGHT cycling (Ch 7 state machine)
  else if (!strcmp(variable, "HEIGHT")) {
    // H1→H2
    if (ch_7_old == 1500 && ch_7 == 1000) {
      ch_7 = 1500; ch_7_old = 1000;
      strcpy(variable, "Changed from HEIGHT1 to HEIGHT2");
      height = "1500";
    }
    // H2→H3
    else if (ch_7_old == 1000 && ch_7 == 1500) {
      ch_7 = 2000; ch_7_old = 1500;
      strcpy(variable, "Changed from HEIGHT2 to HEIGHT3");
      height = "2000";
    }
    // H3→H2
    else if (ch_7_old == 1500 && ch_7 == 2000) {
      ch_7 = 1500; ch_7_old = 2000;
      strcpy(variable, "Changed from HEIGHT3 to HEIGHT2");
      height = "1500";
    }
    // H2→H1
    else if (ch_7_old == 2000 && ch_7 == 1500) {
      ch_7 = 1000; ch_7_old = 1500;
      strcpy(variable, "Changed from HEIGHT2 to HEIGHT1");
      height = "1000";
    }
  }

  // MODE cycling (Ch 8)
  else if (!strcmp(variable, "MODE")) {
    ch_8 += 500;
    if (ch_8 > 2000) ch_8 = 1000;
    if (ch_8 == 1000) { strcpy(variable, "MODE set to MANUAL");     mode = "1000"; }
    if (ch_8 == 1500) { strcpy(variable, "MODE set to DANCE");      mode = "1500"; }
    if (ch_8 == 2000) { strcpy(variable, "MODE set to AUTONOMOUS"); mode = "2000"; }
  }

  // Wave (Ch 9)
  else if (!strcmp(variable, "WAVE")) { wave = true;  strcpy(variable, "Waving"); }
  else if (!strcmp(variable, "wave")) { wave = false; strcpy(variable, ""); }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_sendstr(req, variable);
}

// ═══════════════════════════════════════════════════════════════════════
//  Start Camera Web Server
// ═══════════════════════════════════════════════════════════════════════
static void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri  = { .uri = "/",       .method = HTTP_GET, .handler = index_handler,  .user_ctx = NULL };
  httpd_uri_t cmd_uri    = { .uri = "/action",  .method = HTTP_GET, .handler = cmd_handler,    .user_ctx = NULL };
  httpd_uri_t stream_uri = { .uri = "/stream",  .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }

  config.server_port += 1;
  config.ctrl_port   += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  Setup
// ═══════════════════════════════════════════════════════════════════════
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout detector

  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println("[BOOT] ESPiderman V2 starting...");

  // I2C + PCA9685
  Wire.begin(I2C_SDA, I2C_SCL);
  pwm.begin();
  pwm.setPWMFreq(60);  // 60 Hz for standard servos
  Serial.println("[INIT] PCA9685 initialized at 60 Hz");

  // Built-in LED
  pinMode(LED_BUILTIN, OUTPUT);

  // Camera configuration
  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sscb_sda  = SIOD_GPIO_NUM;
  config.pin_sscb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_XGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    Serial.println("[INIT] PSRAM found — XGA, quality 10, 2 buffers");
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    Serial.println("[INIT] No PSRAM — SVGA, quality 12, 1 buffer");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed: 0x%x\n", err);
    return;
  }
  Serial.println("[INIT] Camera initialized");

  // WiFi AP
  WiFi.softAP(ssid, password);
  Serial.printf("[INIT] AP started — SSID: %s  IP: %s\n", ssid, WiFi.softAPIP().toString().c_str());

  // HTTP servers
  startCameraServer();
  Serial.println("[INIT] Web server on :80, stream on :81");
  Serial.println("[BOOT] Ready.");
}

// ═══════════════════════════════════════════════════════════════════════
//  Main Loop
// ═══════════════════════════════════════════════════════════════════════
void loop() {
  // ── LED control ───────────────────────────────────────────────────
  digitalWrite(LED_BUILTIN, lightOn ? HIGH : LOW);

  // ── Gait logic (armed) ────────────────────────────────────────────
  if (arm) {

    // ── Height 1 (Low) idle ──
    if (height.equals("1000") && ch_1 == 1500 && ch_2 == 1500 && !wave) {
      applyProfile(PROFILE_LOW);
      a1 = a3 = 40 + map(ch_3, 1000, 2000, -25, 25);
      a5 = a7 = 40 - map(ch_3, 1000, 2000, -25, 25);
    }
    // ── Height 2 (Mid) idle ──
    if (height.equals("1500") && ch_1 == 1500 && ch_2 == 1500 && !wave) {
      applyProfile(PROFILE_MID);
      a1 = a3 = 60 + map(ch_3, 1000, 2000, -25, 25);
      a5 = a7 = 60 - map(ch_3, 1000, 2000, -25, 25);
    }
    // ── Height 3 (High) idle ──
    if (height.equals("2000") && ch_1 == 1500 && ch_2 == 1500 && !wave) {
      applyProfile(PROFILE_HIGH);
      a1 = a3 = 90;
      if (a5 <= 90) {
        a5 = a7 = 90 - map(ch_3, 1000, 2000, -40, 40);
      }
    }

    // ── Stand idle (no forward/backward, slight look) ──
    if (ch_1 == 1500 && ch_2 != 2000 && ch_2 != 1000 && !wave) {
      a0 =  stepLength + map(ch_2, 1000, 2000, -85, 85);
      a2 =  stepLength - map(ch_2, 1000, 2000, -85, 85);
      a4 = -stepLength - map(ch_2, 1000, 2000, -85, 85);
      a6 = -stepLength + map(ch_2, 1000, 2000, -85, 85);
      step = 0;
    }

    // ── Wave gesture ──
    if (wave) {
      a3 = -85;
      if (a2r == stepLength && a3r == -85)  a2 = 0;
      if (a2r == 0          && a3r == -85)  a2 = stepLength;
    }

    // ══════════════════════════════════════════════════════════════════
    //  Walk Forward
    // ══════════════════════════════════════════════════════════════════
    if (ch_1 >= 1600) {
      ch_2 = 1500;
      ch_3 = 1500;

      if (a0r == stepLength && a2r == stepLength && a4r == -stepLength && a6r == -stepLength) {
        liftLeg(3);
        a0 = stepLength; a2 = 0; a4 = -stepLength; a6 = -stepLength;
        step = 1;
      }
      if (a0r == stepLength && a2r == 0 && a4r == -stepLength && a6r == -stepLength) {
        lowerLeg(3);
        liftLeg(5);
        a0 = stepLength; a2 = 0; a4 = 0; a6 = -stepLength;
        step = 2;
      }
      if (a0r == stepLength && a2r == 0 && a4r == 0 && a6r == -stepLength && step == 2) {
        lowerLeg(5);
        liftLeg(7);
        a0 = stepLength; a2 = 0; a4 = 0; a6 = stepLength;
        step = 3;
      }
      if (a0r == stepLength && a2r == 0 && a4r == 0 && a6r == stepLength) {
        lowerLeg(7);
        vOld = v;
        v = speedForHeight();
        a0 = 0; a2 = -stepLength; a4 = -stepLength; a6 = 0;
        step = 4;
      }
      if (a0r == 0 && a2r == -stepLength && a4r == -stepLength && a6r == 0) {
        v = vOld;
        liftLeg(3);
        a0 = 0; a2 = stepLength; a4 = -stepLength; a6 = 0;
        step = 5;
      }
      if (a0r == 0 && a2r == stepLength && a4r == -stepLength && a6r == 0) {
        lowerLeg(3);
        liftLeg(5);
        vOld = v;
        v = speedForHeight();
        a0 = 0; a2 = stepLength; a4 = stepLength; a6 = 0;
        step = 6;
      }
      if (a0r == 0 && a2r == stepLength && a4r == stepLength && a6r == 0) {
        v = vOld;
        lowerLeg(5);
        a0 = -stepLength; a2 = 0; a4 = 0; a6 = -stepLength;
        step = 7;
      }
      if (a0r == -stepLength && a2r == 0 && a4r == 0 && a6r == -stepLength) {
        liftLeg(1);
        a0 = stepLength; a2 = 0; a4 = 0; a6 = -stepLength;
        step = 8;
      }
      if (a0r == stepLength && a2r == 0 && a4r == 0 && a6r == -stepLength && step == 8) {
        lowerLeg(1);
        liftLeg(7);
        a0 = stepLength; a2 = 0; a4 = 0; a6 = stepLength;
        step = 0;
      }
    }

    // ══════════════════════════════════════════════════════════════════
    //  Walk Backward
    // ══════════════════════════════════════════════════════════════════
    if (ch_1 <= 1400) {
      ch_2 = 1500;
      ch_3 = 1500;

      if (a0r == stepLength && a2r == stepLength && a4r == -stepLength && a6r == -stepLength) {
        liftLeg(1);
        a0 = 0; a2 = stepLength; a4 = -stepLength; a6 = -stepLength;
        step = 1;
      }
      if (a0r == 0 && a2r == stepLength && a4r == -stepLength && a6r == -stepLength) {
        lowerLeg(1);
        liftLeg(7);
        a0 = 0; a2 = stepLength; a4 = -stepLength; a6 = 0;
        step = 2;
      }
      if (a0r == 0 && a2r == stepLength && a4r == -stepLength && a6r == 0 && step == 2) {
        lowerLeg(7);
        liftLeg(3);
        a0 = 0; a2 = -stepLength; a4 = -stepLength; a6 = 0;
        step = 3;
      }
      if (a0r == 0 && a2r == -stepLength && a4r == -stepLength && a6r == 0) {
        lowerLeg(3);
        vOld = v;
        v = speedForHeight();
        a0 = stepLength; a2 = 0; a4 = 0; a6 = stepLength;
        step = 4;
      }
      if (a0r == stepLength && a2r == 0 && a4r == 0 && a6r == stepLength) {
        liftLeg(1);
        v = vOld;
        a0 = -stepLength; a2 = 0; a4 = 0; a6 = stepLength;
        step = 5;
      }
      if (a0r == -stepLength && a2r == 0 && a4r == 0 && a6r == stepLength) {
        lowerLeg(1);
        liftLeg(7);
        a0 = -stepLength; a2 = 0; a4 = 0; a6 = -stepLength;
        step = 6;
      }
      if (a0r == -stepLength && a2r == 0 && a4r == 0 && a6r == -stepLength) {
        lowerLeg(7);
        vOld = v;
        v = speedForHeight();
        a0 = 0; a2 = stepLength; a4 = stepLength; a6 = 0;
        step = 7;
      }
      if (a0r == 0 && a2r == stepLength && a4r == stepLength && a6r == 0) {
        v = vOld;
        liftLeg(5);
        a0 = 0; a2 = stepLength; a4 = -stepLength; a6 = 0;
        step = 8;
      }
      if (a0r == 0 && a2r == stepLength && a4r == -stepLength && a6r == 0 && step == 8) {
        lowerLeg(5);
        liftLeg(3);
        a0 = 0; a2 = -stepLength; a4 = -stepLength; a6 = 0;
        step = 0;
      }
    }

    // ══════════════════════════════════════════════════════════════════
    //  Turn Left
    // ══════════════════════════════════════════════════════════════════
    if (ch_2 <= 1400) {
      if (a0r == stepLength && a2r == stepLength && a4r == -stepLength && a6r == -stepLength && step == 0) {
        a0 = 0; a2 = 2 * stepLength; a4 = 0; a6 = -2 * stepLength;
        step = 1;
      }
      if (a0r == 0 && a2r == 2 * stepLength && a4r == 0 && a6r == -2 * stepLength) {
        liftLeg(1);
        a0 = stepLength; a2 = 2 * stepLength; a4 = 0; a6 = -2 * stepLength;
        step = 2;
      }
      if (a0r == stepLength && a2r == 2 * stepLength && a4r == 0 && a6r == -2 * stepLength) {
        lowerLeg(1);
        liftLeg(7);
        a0 = stepLength; a2 = 2 * stepLength; a4 = 0; a6 = -stepLength;
        step = 3;
      }
      if (a0r == stepLength && a2r == 2 * stepLength && a4r == 0 && a6r == -stepLength) {
        lowerLeg(7);
        liftLeg(5);
        a0 = stepLength; a2 = 2 * stepLength; a4 = -stepLength; a6 = -stepLength;
        step = 4;
      }
      if (a0r == stepLength && a2r == 2 * stepLength && a4r == -stepLength && a6r == -stepLength) {
        lowerLeg(5);
        liftLeg(3);
        a0 = stepLength; a2 = stepLength; a4 = -stepLength; a6 = -stepLength;
        step = 5;
      }
      if (a0r == stepLength && a2r == stepLength && a4r == -stepLength && a6r == -stepLength && step == 5) {
        lowerLeg(3);
        a0 = stepLength; a2 = stepLength; a4 = -stepLength; a6 = -stepLength;
        step = 0;
      }
    }

    // ══════════════════════════════════════════════════════════════════
    //  Turn Right
    // ══════════════════════════════════════════════════════════════════
    if (ch_2 >= 1600) {
      if (a0r == stepLength && a2r == stepLength && a4r == -stepLength && a6r == -stepLength && step == 0) {
        a0 = 2 * stepLength; a2 = 0; a4 = -2 * stepLength; a6 = 0;
        step = 1;
      }
      if (a0r == 2 * stepLength && a2r == 0 && a4r == -2 * stepLength && a6r == 0) {
        liftLeg(1);
        a0 = stepLength; a2 = 0; a4 = -2 * stepLength; a6 = 0;
        step = 2;
      }
      if (a0r == stepLength && a2r == 0 && a4r == -2 * stepLength && a6r == 0) {
        lowerLeg(1);
        liftLeg(7);
        a0 = stepLength; a2 = 0; a4 = -2 * stepLength; a6 = -stepLength;
        step = 3;
      }
      if (a0r == stepLength && a2r == 0 && a4r == -2 * stepLength && a6r == -stepLength) {
        lowerLeg(7);
        liftLeg(5);
        a0 = stepLength; a2 = 0; a4 = -stepLength; a6 = -stepLength;
        step = 4;
      }
      if (a0r == stepLength && a2r == 0 && a4r == -stepLength && a6r == -stepLength) {
        lowerLeg(5);
        liftLeg(3);
        a0 = stepLength; a2 = stepLength; a4 = -stepLength; a6 = -stepLength;
        step = 5;
      }
      if (a0r == stepLength && a2r == stepLength && a4r == -stepLength && a6r == -stepLength && step == 5) {
        lowerLeg(3);
        a0 = stepLength; a2 = stepLength; a4 = -stepLength; a6 = -stepLength;
        step = 0;
      }
    }

  }
  // ── Disarmed pose ─────────────────────────────────────────────────
  else {
    a0 = a2 = 0;
    a4 = a6 = 0;
    a1 = a3 = a5 = a7 = LEG_IDLE;
  }

  // ── Pack feedback data ────────────────────────────────────────────
  // FIX: was using distFrL twice; now correctly uses distFrR
  String fb = light + "," + armed + "," + height + "," + mode + ","
            + footFL + "," + footFR + "," + footBL + "," + footBR + ","
            + temp + "," + rotX + "," + rotY + "," + rotZ + ","
            + distFr + "," + distFrL + "," + distFrR + "," + distBott;
  fb.toCharArray(dataToClient, sizeof(dataToClient));

  // ══════════════════════════════════════════════════════════════════
  //  Ramped Servo Controller (array-based — replaces 80+ lines)
  //
  //  For each servo:  if current != target (outside deadband),
  //  increment/decrement by 1 per loop.  This gives smooth motion
  //  with speed controlled by delay(v) at the end.
  //
  //  FIX: Original code had a copy-paste bug where servo 5's
  //  decrement branch checked a1r instead of a5r.  The array-based
  //  loop eliminates this class of bug entirely.
  // ══════════════════════════════════════════════════════════════════
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (aR[i] <= aT[i] - EPS) {
      aR[i]++;
    } else if (aR[i] >= aT[i] + EPS) {
      aR[i]--;
    } else {
      aR[i] = aT[i];
    }
    setServo(i, toPhysical(i, aR[i]));
  }

  delay(v);
}
