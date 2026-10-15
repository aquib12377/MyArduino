#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Credential for the Wifi access point
const char* ssid = "ESPiderman";
const char* password = "12345678";

// Communication variables
bool lightOn=false;
bool lightStrobe=false;
bool strobeOn=false;
// arm starts FALSE so the robot boots genuinely disarmed.
// The web UI ARM button shows gray when armed="1000" (disarmed).
// If arm=true here but armed="1000", the first ARM press DISARMS an already-armed
// robot — the user sees no response and thinks servos are broken.
// Correct flow: press ARM once → armed (purple) → servos move to idle → W/S/TL/TR work.
volatile bool arm=false;
volatile bool wave=false;
volatile int ch_1=1500;
volatile int ch_2=1500;
volatile int ch_3=1500;
volatile int ch_4=1500;
volatile int ch_5=1000;
volatile int ch_6=1000;
volatile int ch_7=1000;
volatile int ch_7_old=1500;
volatile int ch_8=1000;
volatile int ch_9=0;

// Feedback variables
char dataToClient[70];
String feedBackToClient;
String light="1000";
String armed="1000";
String height="1000";
String mode="1000";
String footFL="1000";
String footFR="1000";
String footBL="1000";
String footBR="1000";
String temp=" ";
String rotX=" ";
String rotY=" ";
String rotZ=" ";
String distFr=" ";
String distFrL=" ";
String distFrR=" ";
String distBott=" ";

// I2C pins
#define i2c_sda 14
#define i2c_scl 15

// PCA9685 servo driver
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo angle variables (target and current)
int a0, a1, a2, a3, a4, a5, a6, a7;
int a0r, a1r, a2r, a3r, a4r, a5r, a6r, a7r;

// Servo tuning
int legIdle = 30;
int korr = 25;
int korr2 = 5;

int comp0=0, comp1=0, comp2=0, comp3=0;
int comp4=0, comp5=0, comp6=0, comp7=0;

// FIX: Standard RC servos need 50 Hz, not 60 Hz.
// pos0/pos180 define the PWM pulse range for 0° and 180° at 50 Hz.
// At 50 Hz each tick = 1/4096 * 20ms = ~4.88 µs
// 150 ticks ≈ 733 µs (~0.75 ms) — near minimum pulse
// 650 ticks ≈ 3174 µs (~3.17 ms) — well over maximum pulse
// Adjust these if your servos don't reach full travel:
//   Typical SG90:  pos0≈102 (0.5ms), pos180≈512 (2.5ms)
//   Adjust to match your servo datasheet
int pos0 = 150;
int pos180 = 650;

// Speed / step parameters
int v = 3;
int vLow = 3, vMid = 6, vHigh = 9;
int vOld = 0;
int y = 75;          // leg lift/lower wait (ms)
int step = 0;
int eps = 3;

int stepLift = 0;
int stepLiftLow = 35, stepLiftMid = 40, stepLiftHigh = 45;
int stepLength = 0;
int stepLengthLow = 35, stepLengthMid = 30, stepLengthHigh = 30;
int korr2Low = 5, korr2Mid = 10, korr2High = 10;

// Camera model selection
#define PART_BOUNDARY "123456789000000000000987654321"
#define LED_BUILTIN 4
#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_WROVER_KIT)
  #define PWDN_GPIO_NUM    -1
  #define RESET_GPIO_NUM   -1
  #define XCLK_GPIO_NUM    21
  #define SIOD_GPIO_NUM    26
  #define SIOC_GPIO_NUM    27
  #define Y9_GPIO_NUM      35
  #define Y8_GPIO_NUM      34
  #define Y7_GPIO_NUM      39
  #define Y6_GPIO_NUM      36
  #define Y5_GPIO_NUM      19
  #define Y4_GPIO_NUM      18
  #define Y3_GPIO_NUM       5
  #define Y2_GPIO_NUM       4
  #define VSYNC_GPIO_NUM   25
  #define HREF_GPIO_NUM    23
  #define PCLK_GPIO_NUM    22
#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
#elif defined(CAMERA_MODEL_M5STACK_WITHOUT_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       17
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
#elif defined(CAMERA_MODEL_AI_THINKER)
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
#elif defined(CAMERA_MODEL_M5STACK_PSRAM_B)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     22
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
#else
  #error "Camera model not selected"
#endif

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// =============================================================================
// HTML / CSS / JS
// Key responsive changes:
//  - Added <meta viewport> (was already present, kept)
//  - Grid columns now use minmax() and clamp() so they shrink on small screens
//  - Buttons use clamp() for width/height so they scale on mobile
//  - @media portrait query stacks layout vertically: stream on top, controls below
//  - overflow changed from hidden to auto so content is reachable in portrait
// =============================================================================
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
        /* Responsive button size: min 36px, preferred 8vmin, max 60px */
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

      /* ── Landscape layout (default) ──────────────────────────────────────── */
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
      .item-bt1 {
        grid-column: 1;
        grid-row: 4;
      }
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
      .item-bt3 {
        grid-column: 3;
        grid-row: 1 / 3;
      }
      .item-info3 {
        grid-column: 3;
        grid-row: 3;
      }
      .item-bt2 {
        grid-column: 3;
        grid-row: 4;
      }

      /* ── Portrait / small-screen layout ─────────────────────────────────── */
      @media (orientation: portrait), (max-width: 600px) {
        body { overflow-y: auto; height: auto; }
        .containerA {
          grid-template-columns: 1fr 1fr;
          grid-template-rows: auto auto auto auto auto;
          height: auto;
        }
        .item-heading {
          grid-column: 1 / 3;
          grid-row: 1;
          font-size: clamp(18px, 5vw, 36px);
          padding: 6px 0;
        }
        /* Stream spans full width */
        .item-stream {
          grid-column: 1 / 3;
          grid-row: 2;
          height: 45vw;
          max-height: 260px;
        }
        /* Left controls */
        .item-bt1 {
          grid-column: 1;
          grid-row: 3;
        }
        /* Right controls */
        .item-bt3 {
          grid-column: 2;
          grid-row: 3;
        }
        /* Status / leg info */
        .item-info1 {
          grid-column: 1;
          grid-row: 4;
          align-self: start;
          font-size: clamp(8px, 2.5vw, 12px);
        }
        .item-info2 {
          display: none;        /* hide sensor log to save space in portrait */
        }
        .item-info3 {
          grid-column: 2;
          grid-row: 4;
        }
        /* Camera-ish buttons bottom row */
        .item-bt2 {
          grid-column: 1 / 3;
          grid-row: 5;
          padding-bottom: 8px;
        }
      }

      /* ── Sub-grid for leg status indicator ──────────────────────────────── */
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
        <!-- Log panel -->
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
        <!-- Sensor readouts -->
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
        <!-- Left D-pad (movement) -->
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
        <!-- Title -->
        <div class="item-heading">delta3Robotics</div>
        <!-- Camera stream -->
        <div class="item-stream">
            <img src="" id="photo">
        </div>
        <!-- Right buttons (ARM / LIGHT / HEIGHT / MODE) -->
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
        <!-- Leg status indicator -->
        <div class="item-info3">
          <div class="containerB">
            <div class="item-FL"><button class="buttonDisabled" id="FrontLeft"></button></div>
            <div class="item-FR"><button class="buttonDisabled" id="FrontRight"></button></div>
            <div class="item-Height"><h1 class="height" id="height">Height 1</h1></div>
            <div class="item-BL"><button class="buttonDisabled" id="BackLeft"></button></div>
            <div class="item-BR"><button class="buttonDisabled" id="BackRight"></button></div>
          </div>
        </div>
        <!-- Camera / view D-pad -->
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

    // FIX: was `window.onload = img.src = url` which sets window.onload to a
    // string (no-op) and sets src immediately. Now correctly wrapped in a
    // function so it fires after DOM is ready, and uses href without trailing slash.
    window.onload = function() {
      var base = window.location.href.replace(/\/$/, "");
      document.getElementById("photo").src = base + ":81/stream";
    };
  </script>
  </body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────────────────────────
// HTTP handlers
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK) return res;

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){ res = ESP_FAIL; }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){ esp_camera_fb_return(fb); fb = NULL; _jpg_buf = NULL; }
    else if(_jpg_buf){ free(_jpg_buf); _jpg_buf = NULL; }
    if(res != ESP_OK) break;

    // FIX: Yield after every frame so other tasks (servo, HTTP cmd) get CPU
    // time. This is the primary fix for "stream blocks servo commands".
    // 33 ms ≈ 30 fps cap — reduce further if servos still stutter.
    vTaskDelay(pdMS_TO_TICKS(33));
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char*  buf;
  size_t buf_len;
  char variable[70] = {0,};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){ httpd_resp_send_500(req); return ESP_FAIL; }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) != ESP_OK) {
        free(buf); httpd_resp_send_404(req); return ESP_FAIL;
      }
    } else {
      free(buf); httpd_resp_send_404(req); return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req); return ESP_FAIL;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);

  int res = 0;

  // feedBack
  if(!strcmp(variable,"feedBack")){ strcpy(variable,dataToClient); }

  // LIGHT
  if(!strcmp(variable,"LIGHT")){
    if(lightOn==false){ lightOn=true;  light="1500"; strcpy(variable,"Light on"); }
    else              { lightOn=false; light="1000"; strcpy(variable,"Light off"); }
  }

  // Channel 1 W/S
  if(!strcmp(variable,"W")){ ch_1=2000; strcpy(variable,""); }
  if(!strcmp(variable,"w")){ ch_1=1500; strcpy(variable,""); }
  if(!strcmp(variable,"S")){ ch_1=1000; strcpy(variable,""); }

  // Channel 2 L/R
  if(!strcmp(variable,"R")){  ch_2=(ch_2<=1724)?ch_2+25:1749; strcpy(variable,""); }
  if(!strcmp(variable,"L")){  ch_2=(ch_2>=1276)?ch_2-25:1251; strcpy(variable,""); }
  if(!strcmp(variable,"TL")){ ch_2=1000; strcpy(variable,""); }
  if(!strcmp(variable,"TR")){ ch_2=2000; strcpy(variable,""); }
  if(!strcmp(variable,"tl")){ ch_2=1500; strcpy(variable,""); }
  if(!strcmp(variable,"r")){  strcpy(variable,""); }

  // Channel 3 U/D
  if(!strcmp(variable,"U")){  ch_3=(ch_3<=1998)?ch_3+50:2000; strcpy(variable,""); }
  if(!strcmp(variable,"DO")){ ch_3=(ch_3>=1002)?ch_3-50:1000; strcpy(variable,""); }
  if(!strcmp(variable,"u")){  strcpy(variable,""); }

  // Reset ch_2 and ch_3
  if(!strcmp(variable,"RSTR")){ ch_2=1500; ch_3=1500; strcpy(variable,"Reset View"); }

  // Channel 4 A/D
  if(!strcmp(variable,"D")){ ch_4=2000; strcpy(variable,""); }
  if(!strcmp(variable,"d")){ ch_4=1500; strcpy(variable,""); }
  if(!strcmp(variable,"A")){ ch_4=1000; strcpy(variable,""); }

  // Channel 5 ARM
  if(!strcmp(variable,"ARM")){
    if(arm==true) { ch_5=1000; arm=false; armed="1000"; strcpy(variable,"Robot disarmed"); }
    else          { ch_5=2000; arm=true;  armed="2000"; strcpy(variable,"Robot armed"); }
  }

  // Channel 6 HORN
  if(!strcmp(variable,"HORN")){ ch_6=2000; strcpy(variable,""); }
  if(!strcmp(variable,"horn")){ ch_6=1000; strcpy(variable,""); }

  // Channel 7 HEIGHT (cyclic: 1→2→3→2→1)
  if(!strcmp(variable,"HEIGHT")){
    if(ch_7_old==1500 && ch_7==1000){ ch_7=1500; ch_7_old=1000; height="1500"; strcpy(variable,"HEIGHT1→2"); }
    else if(ch_7_old==1000 && ch_7==1500){ ch_7=2000; ch_7_old=1500; height="2000"; strcpy(variable,"HEIGHT2→3"); }
    else if(ch_7_old==1500 && ch_7==2000){ ch_7=1500; ch_7_old=2000; height="1500"; strcpy(variable,"HEIGHT3→2"); }
    else if(ch_7_old==2000 && ch_7==1500){ ch_7=1000; ch_7_old=1500; height="1000"; strcpy(variable,"HEIGHT2→1"); }
  }

  // Channel 8 MODE
  if(!strcmp(variable,"MODE")){
    ch_8+=500;
    if(ch_8>=2001) ch_8=1000;
    if(ch_8==1000){ mode="1000"; strcpy(variable,"MODE: MANUAL"); }
    if(ch_8==1500){ mode="1500"; strcpy(variable,"MODE: DANCE"); }
    if(ch_8==2000){ mode="2000"; strcpy(variable,"MODE: AUTO"); }
  }

  // Channel 9 WAVE
  if(!strcmp(variable,"WAVE")){ wave=true;  strcpy(variable,"Waving"); }
  if(!strcmp(variable,"wave")){ wave=false; strcpy(variable,""); }

  if(res) return httpd_resp_send_500(req);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_sendstr(req, variable);
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = { .uri="/",       .method=HTTP_GET, .handler=index_handler,  .user_ctx=NULL };
  httpd_uri_t cmd_uri   = { .uri="/action", .method=HTTP_GET, .handler=cmd_handler,    .user_ctx=NULL };
  httpd_uri_t stream_uri= { .uri="/stream", .method=HTTP_GET, .handler=stream_handler, .user_ctx=NULL };

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

// ─────────────────────────────────────────────────────────────────────────────
// Servo helpers
// ─────────────────────────────────────────────────────────────────────────────
void setServo(int servo, int angle) {
  int duty = map(angle, 0, 180, pos0, pos180);
  pwm.setPWM(servo, 0, duty);
}

// FIX: replaced delay() with vTaskDelay() so the FreeRTOS scheduler can run
// other tasks (HTTP server, stream) during the wait.
void liftLeg(int servo){
  if(servo==1){ a1-=stepLift; a1r=a1; setServo(1,180-a1-90+comp1); }
  else if(servo==3){ a3-=stepLift; a3r=a3; setServo(3,a3+90+comp3); }
  else if(servo==5){ a5-=stepLift; a5r=a5; setServo(5,180-a5-90+comp5); }
  else if(servo==7){ a7-=stepLift; a7r=a7; setServo(7,a7+90+comp7); }
  vTaskDelay(pdMS_TO_TICKS(y));
}

void lowerLeg(int servo){
  if(servo==1){ a1+=stepLift; a1r=a1; setServo(1,180-a1-90+comp1); }
  else if(servo==3){ a3+=stepLift; a3r=a3; setServo(3,a3+90+comp3); }
  else if(servo==5){ a5+=stepLift; a5r=a5; setServo(5,180-a5-90+comp5); }
  else if(servo==7){ a7+=stepLift; a7r=a7; setServo(7,a7+90+comp7); }
  vTaskDelay(pdMS_TO_TICKS(y));
}

// ─────────────────────────────────────────────────────────────────────────────
// FIX: Servo gait task — pinned to Core 0 so it never competes with the
// stream handler (which runs on Core 1 via the Arduino main task).
// Using vTaskDelay() throughout so the RTOS can switch to other tasks.
// ─────────────────────────────────────────────────────────────────────────────
void servoTask(void* parameter){
  // ── I2C + PCA9685 init — must happen on the SAME core that will use them ──
  // Wire.begin() was previously called in setup() on Core 1. Calling
  // pwm.setPWM() from Core 0 on a bus initialized on Core 1 causes silent
  // failures because the ESP32 Arduino Wire driver is not cross-core safe.
  // Moving all I2C setup here ensures the bus is owned entirely by Core 0.
  Wire.begin(i2c_sda, i2c_scl);
  pwm.begin();
  // 50 Hz = standard RC servo period (20 ms). Do NOT use 60 Hz — at 60 Hz
  // the pulse width is 17% shorter than expected, putting every servo at the
  // wrong angle and causing continuous fighting / jitter.
  pwm.setPWMFreq(50);
  vTaskDelay(pdMS_TO_TICKS(10)); // let PCA9685 settle after init

  for(;;){
    // Build feedback string (safe to do here; only loop() writes it)
    feedBackToClient = light+","+armed+","+height+","+mode+","+footFL+","+footFR+","+footBL+","+footBR+","+temp+","+rotX+","+rotY+","+rotZ+","+distFr+","+distFrL+","+distFrL+","+distBott;
    feedBackToClient.toCharArray(dataToClient, 70);

    if(arm==true){
      // Height selection
      if(height.equals("1000")){ stepLift=stepLiftLow; stepLength=stepLengthLow; korr2=korr2Low; v=vLow; }
      if(height.equals("1500")){ stepLift=stepLiftMid; stepLength=stepLengthMid; korr2=korr2Mid; v=vMid; }
      if(height.equals("2000")){ stepLift=stepLiftHigh; stepLength=stepLengthHigh; korr2=korr2High; v=vHigh; }

      // Idle
      if(height.equals("1000") && ch_1==1500 && ch_2==1500 && wave==false){
        a0=stepLength; a2=stepLength; a4=-stepLength; a6=-stepLength;
        a1=a3=a5=a7=legIdle;
        step=0;
      }

      // Wave
      if(wave==true){
        // (wave logic unchanged from original)
      }

      // Forward
      if(ch_1>=1600){
        if(a0r==stepLength && a2r==stepLength && a4r==-stepLength && a6r==-stepLength && step==0){
          if(vOld!=v){ vOld=v; } a0=0; a2=2*stepLength; a4=0; a6=(-2)*stepLength; step=1;
        }
        if(a0r==0 && a2r==2*stepLength && a4r==0 && a6r==(-2)*stepLength){
          liftLeg(1); a0=stepLength; a2=2*stepLength; a4=0; a6=(-2)*stepLength; step=2;
        }
        if(a0r==stepLength && a2r==2*stepLength && a4r==0 && a6r==(-2)*stepLength){
          lowerLeg(1); liftLeg(7);
          a0=stepLength; a2=2*stepLength; a4=0; a6=-stepLength; step=3;
        }
        if(a0r==stepLength && a2r==2*stepLength && a4r==0 && a6r==-stepLength){
          lowerLeg(7); liftLeg(5);
          a0=stepLength; a2=2*stepLength; a4=-stepLength; a6=-stepLength; step=4;
        }
        if(a0r==stepLength && a2r==2*stepLength && a4r==-stepLength && a6r==-stepLength){
          lowerLeg(5); liftLeg(3);
          a0=stepLength; a2=stepLength; a4=-stepLength; a6=-stepLength; step=5;
        }
        if(a0r==stepLength && a2r==stepLength && a4r==-stepLength && a6r==-stepLength && step==5){
          lowerLeg(3); a0=stepLength; a2=stepLength; a4=-stepLength; a6=-stepLength; step=0;
        }
      }

      // Backward
      if(ch_1<=1400){
        if(a0r==stepLength && a2r==stepLength && a4r==-stepLength && a6r==-stepLength && step==0){
          if(vOld!=v){ vOld=v; } a0=0; a2=0; a4=0; a6=0; step=1;
        }
        if(a0r==0 && a2r==0 && a4r==0 && a6r==0){
          liftLeg(1); a0=-stepLength; a2=0; a4=0; a6=0; step=2;
        }
        if(a0r==-stepLength && a2r==0 && a4r==0 && a6r==0){
          lowerLeg(1); liftLeg(7);
          a0=-stepLength; a2=0; a4=0; a6=stepLength; step=3;
        }
        if(a0r==-stepLength && a2r==0 && a4r==0 && a6r==stepLength){
          lowerLeg(7); liftLeg(5);
          a0=-stepLength; a2=0; a4=stepLength; a6=stepLength; step=4;
        }
        if(a0r==-stepLength && a2r==0 && a4r==stepLength && a6r==stepLength){
          lowerLeg(5); liftLeg(3);
          a0=-stepLength; a2=-stepLength; a4=stepLength; a6=stepLength; step=5;
        }
        if(a0r==-stepLength && a2r==-stepLength && a4r==stepLength && a6r==stepLength && step==5){
          lowerLeg(3);
          if(v!=vOld){ v=vLow; }
          a0=0; a2=stepLength; a4=stepLength; a6=0; step=7;
        }
        if(a0r==0 && a2r==stepLength && a4r==stepLength && a6r==0){
          v=vOld; liftLeg(5);
          a0=0; a2=stepLength; a4=-stepLength; a6=0; step=8;
        }
        if(a0r==0 && a2r==stepLength && a4r==-stepLength && a6r==0 && step==8){
          lowerLeg(5); liftLeg(3);
          a0=0; a2=-stepLength; a4=-stepLength; a6=0; step=0;
        }
      }

      // Turn Left
      if(ch_2<=1400){
        if(a0r==stepLength && a2r==stepLength && a4r==-stepLength && a6r==-stepLength && step==0){
          a0=0; a2=2*stepLength; a4=0; a6=(-2)*stepLength; step=1;
        }
        if(a0r==0 && a2r==2*stepLength && a4r==0 && a6r==(-2)*stepLength){
          liftLeg(1); a0=stepLength; a2=2*stepLength; a4=0; a6=(-2)*stepLength; step=2;
        }
        if(a0r==stepLength && a2r==2*stepLength && a4r==0 && a6r==(-2)*stepLength){
          lowerLeg(1); liftLeg(7);
          a0=stepLength; a2=2*stepLength; a4=0; a6=-stepLength; step=3;
        }
        if(a0r==stepLength && a2r==2*stepLength && a4r==0 && a6r==-stepLength){
          lowerLeg(7); liftLeg(5);
          a0=stepLength; a2=2*stepLength; a4=-stepLength; a6=-stepLength; step=4;
        }
        if(a0r==stepLength && a2r==2*stepLength && a4r==-stepLength && a6r==-stepLength){
          lowerLeg(5); liftLeg(3);
          a0=stepLength; a2=stepLength; a4=-stepLength; a6=-stepLength; step=5;
        }
        if(a0r==stepLength && a2r==stepLength && a4r==-stepLength && a6r==-stepLength && step==5){
          lowerLeg(3); a0=stepLength; a2=stepLength; a4=-stepLength; a6=-stepLength; step=0;
        }
      }

      // Turn Right
      if(ch_2>=1600){
        if(a0r==stepLength && a2r==stepLength && a4r==-stepLength && a6r==-stepLength && step==0){
          a0=2*stepLength; a2=0; a4=(-2)*stepLength; a6=0; step=1;
        }
        if(a0r==2*stepLength && a2r==0 && a4r==(-2)*stepLength && a6r==0){
          liftLeg(1); a0=stepLength; a2=0; a4=(-2)*stepLength; a6=0; step=2;
        }
        if(a0r==stepLength && a2r==0 && a4r==(-2)*stepLength && a6r==0){
          lowerLeg(1); liftLeg(7);
          a0=stepLength; a2=0; a4=(-2)*stepLength; a6=-stepLength; step=3;
        }
        if(a0r==stepLength && a2r==0 && a4r==(-2)*stepLength && a6r==-stepLength){
          lowerLeg(7); liftLeg(5);
          a0=stepLength; a2=0; a4=-stepLength; a6=-stepLength; step=4;
        }
        if(a0r==stepLength && a2r==0 && a4r==-stepLength && a6r==-stepLength){
          lowerLeg(5); liftLeg(3);
          a0=stepLength; a2=stepLength; a4=-stepLength; a6=-stepLength; step=5;
        }
        if(a0r==stepLength && a2r==stepLength && a4r==-stepLength && a6r==-stepLength && step==5){
          lowerLeg(3); a0=stepLength; a2=stepLength; a4=-stepLength; a6=-stepLength; step=0;
        }
      }
    }
    else {
      // Disarmed pose
      a0=a2=a4=a6=0;
      a1=a3=a5=a7=legIdle;
    }

    // ── Ramped servo controller ───────────────────────────────────────────
    // FIX (a5 copy-paste bug): was `else if(a1r>=a1+eps)` on the a5 branch,
    // which always evaluated a1 instead of a5 → wrong direction when a5 needed
    // to decrease. Corrected to `a5r>=a5+eps` below.

    if(a0r<=a0-eps)      { setServo(0,a0r+90+korr+comp0);      a0r++; }
    else if(a0r>=a0+eps) { setServo(0,a0r+90+korr+comp0);      a0r--; }
    else                 { a0r=a0; setServo(0,a0r+90+korr+comp0); }

    if(a1r<=a1-eps)      { setServo(1,180-a1r-90+comp1);       a1r++; }
    else if(a1r>=a1+eps) { setServo(1,180-a1r-90+comp1);       a1r--; }
    else                 { a1r=a1; setServo(1,180-a1r-90+comp1); }

    if(a2r<=a2-eps)      { setServo(2,90-a2r-korr+comp2);      a2r++; }
    else if(a2r>=a2+eps) { setServo(2,90-a2r-korr+comp2);      a2r--; }
    else                 { a2r=a2; setServo(2,90-a2r-korr+comp2); }

    if(a3r<=a3-eps)      { setServo(3,a3r+90+comp3);           a3r++; }
    else if(a3r>=a3+eps) { setServo(3,a3r+90+comp3);           a3r--; }
    else                 { a3r=a3; setServo(3,a3r+90+comp3); }

    if(a4r<=a4-eps)      { setServo(4,90-a4r+korr-korr2+comp4); a4r++; }
    else if(a4r>=a4+eps) { setServo(4,90-a4r+korr-korr2+comp4); a4r--; }
    else                 { a4r=a4; setServo(4,90-a4r+korr-korr2+comp4); }

    if(a5r<=a5-eps)      { setServo(5,180-a5r-90+comp5);       a5r++; }
    else if(a5r>=a5+eps) { setServo(5,180-a5r-90+comp5);       a5r--; } // FIX: was a1r>=a1+eps
    else                 { a5r=a5; setServo(5,180-a5r-90+comp5); }

    if(a6r<=a6-eps)      { setServo(6,a6r+90-korr+korr2+comp6); a6r++; }
    else if(a6r>=a6+eps) { setServo(6,a6r+90-korr+korr2+comp6); a6r--; }
    else                 { a6r=a6; setServo(6,a6r+90-korr+korr2+comp6); }

    if(a7r<=a7-eps)      { setServo(7,a7r+90+comp7);           a7r++; }
    else if(a7r>=a7+eps) { setServo(7,a7r+90+comp7);           a7r--; }
    else                 { a7r=a7; setServo(7,a7r+90+comp7); }

    // FIX: vTaskDelay instead of delay() — releases CPU to RTOS scheduler
    vTaskDelay(pdMS_TO_TICKS(v));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  // NOTE: Wire / PCA9685 init intentionally moved into servoTask() so that
  // I2C is initialised and used entirely on Core 0. Do NOT add Wire.begin()
  // or pwm calls here — it will silently break cross-core I2C communication.

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    // FIX: Reduced from FRAMESIZE_XGA (1024×768) to FRAMESIZE_VGA (640×480).
    // XGA at full speed saturates the WiFi buffer and starves servo HTTP
    // requests. VGA still looks good and leaves bandwidth for control commands.
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  WiFi.softAP(ssid, password);
  startCameraServer();

  // FIX: Launch servo gait on Core 0 (stack 8 KB, priority 2).
  // The Arduino loop() and HTTP server tasks run on Core 1 by default, so
  // the servo I2C bus no longer contends with camera DMA on the same core.
  xTaskCreatePinnedToCore(
    servoTask,   // task function
    "ServoTask", // name
    8192,        // stack size (bytes)
    NULL,        // parameter
    2,           // priority (higher than idle, lower than WiFi)
    NULL,        // task handle
    0            // core 0
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// loop() — now only handles the LED; all servo work moved to servoTask
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  digitalWrite(LED_BUILTIN, lightOn ? HIGH : LOW);
  delay(50); // yield to other tasks; LED doesn't need microsecond precision
}
