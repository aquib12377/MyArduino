/*****************************************************************
 *  ESP32-CAM  →  RGB565 BMP  →  Google Drive
 *  -------------------------------------------------------------
 *  © 2025-06-05  ChatGPT-o3 • Public Domain
 *****************************************************************/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "Base64.h"

/*-------------------------- Board pins (AI-Thinker) ------------*/
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define FLASH_LED_PIN   4                         /* on-board LED   */

/*-------------------------- Wi-Fi & Script IDs ------------------*/
const char* ssid     = "MyProject";
const char* password = "12345678";

String  myDeploymentID   = "AKfycbxSKwE997wUM4pgJfyHCIev2gs516gzs42E7LsUTd6Gm57JJ4CY6Lb5tzzv5_egcSL7Pg";
String  myMainFolderName = "ESP32-CAM";

constexpr uint32_t SEND_INTERVAL_MS = 20000;     /* 20 s           */
bool LED_Flash_ON = true;

WiFiClientSecure client;

/*--------------------------  helpers ---------------------------*/
static inline uint32_t pad4(uint32_t x){ return (x + 3U) & ~3U; }

/*  Base-64 stream, stripping CR/LF so Apps Script can decode  */
void streamB64(WiFiClientSecure &cli,
               const uint8_t    *buf,
               size_t            len)
{
  constexpr size_t CHUNK = 3 * 900;               /* 2700 raw → 3600 b64     */
  static   char b64[(CHUNK * 4 / 3) + 4];

  while (len) {
    size_t n   = len > CHUNK ? CHUNK : len;
    size_t out = base64_encode(b64, (char *)buf, n);
    for (size_t i = 0; i < out; ++i) {            /* copy, but skip \r \n    */
      char c = b64[i];
      if (c != '\n' && c != '\r') cli.write(c);
    }
    buf += n; len -= n;
    delay(1);                                     /* keep Wi-Fi alive        */
  }
}

/*--------------------------  one POST upload -------------------*/
void sendFrameAsBmp()
{
  /* 1)  Capture frame ----------------------------------------- */
  if (LED_Flash_ON){ digitalWrite(FLASH_LED_PIN, HIGH); delay(80); }
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb){ Serial.println("Capture failed"); ESP.restart(); }
  if (LED_Flash_ON) digitalWrite(FLASH_LED_PIN, LOW);

  const uint16_t W = fb->width,  H = fb->height;
  const uint32_t rowSize   = pad4(W * 2U);
  const uint32_t pixelsLen = rowSize * H;
  const uint32_t fileLen   = 14 + 40 + 12 + pixelsLen;     /* 66 B header */

  /* 2)  Build 66-byte BMP header ------------------------------ */
  uint8_t hdr[66] = {0};
  hdr[0]='B'; hdr[1]='M';
  hdr[ 2]= fileLen        &0xFF;
  hdr[ 3]=(fileLen >> 8 ) &0xFF;
  hdr[ 4]=(fileLen >>16 ) &0xFF;
  hdr[ 5]=(fileLen >>24 ) &0xFF;
  hdr[10]= 66;                                  /* pixel offset         */

  hdr[14]=40;                                   /* BITMAPINFOHEADER     */
  hdr[18]= W &0xFF; hdr[19]= W>>8;
  hdr[22]=(-H)&0xFF; hdr[23]=(-H)>>8;           /* negative ⇒ top-down  */
  hdr[26]=1;  hdr[28]=16;                       /* 16 bpp               */
  hdr[30]=3;                                    /* BI_BITFIELDS         */
  hdr[34]= pixelsLen        &0xFF;
  hdr[35]=(pixelsLen >>  8) &0xFF;
  hdr[36]=(pixelsLen >> 16) &0xFF;
  hdr[37]=(pixelsLen >> 24) &0xFF;

  /* RGB565 colour masks */
  hdr[54]=0x00; hdr[55]=0xF8; hdr[56]=0x00; hdr[57]=0x00;  // R mask 0xF800
  hdr[58]=0xE0; hdr[59]=0x07; hdr[60]=0x00; hdr[61]=0x00;  // G mask 0x07E0
  hdr[62]=0x1F; hdr[63]=0x00; hdr[64]=0x00; hdr[65]=0x00;  // B mask 0x001F

  /* 3)  Open TLS connection ---------------------------------- */
  client.setInsecure();
  if (!client.connect("script.google.com", 443)){
    Serial.println("TLS connect failed"); esp_camera_fb_return(fb); return;
  }

  /* 4)  HTTP request headers (no Content-Length) -------------- */
  String url = "/macros/s/" + myDeploymentID + "/exec?folder=" + myMainFolderName;
  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: script.google.com");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();                              /* blank line = end-headers */

  /* 5)  Stream header + pixel rows --------------------------- */
  streamB64(client, hdr, sizeof(hdr));

  const uint8_t *row = fb->buf;
  const uint32_t rowNoPad = W * 2U;
  const uint8_t  pad[3]   = {0,0,0};

  for (uint16_t y = 0; y < H; ++y, row += rowNoPad){
    streamB64(client, row, rowNoPad);
    if (rowSize > rowNoPad)
      streamB64(client, pad, rowSize - rowNoPad);
  }
  esp_camera_fb_return(fb);
  client.flush();
  client.stop();                                /* close = end-of-body   */

  Serial.println("BMP sent (" + String(fileLen) + " bytes raw).");
}

/*--------------------------  Setup ----------------------------*/
void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);     /* disable brown-out   */
  Serial.begin(115200); delay(100);
  pinMode(FLASH_LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status()!=WL_CONNECTED){ delay(250); Serial.print('.'); }
  Serial.println("\nWi-Fi OK");

  camera_config_t cfg{};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0=Y2_GPIO_NUM; cfg.pin_d1=Y3_GPIO_NUM; cfg.pin_d2=Y4_GPIO_NUM;
  cfg.pin_d3=Y5_GPIO_NUM; cfg.pin_d4=Y6_GPIO_NUM; cfg.pin_d5=Y7_GPIO_NUM;
  cfg.pin_d6=Y8_GPIO_NUM; cfg.pin_d7=Y9_GPIO_NUM;
  cfg.pin_xclk=XCLK_GPIO_NUM; cfg.pin_pclk=PCLK_GPIO_NUM;
  cfg.pin_vsync=VSYNC_GPIO_NUM; cfg.pin_href=HREF_GPIO_NUM;
  cfg.pin_sscb_sda=SIOD_GPIO_NUM; cfg.pin_sscb_scl=SIOC_GPIO_NUM;
  cfg.pin_pwdn=PWDN_GPIO_NUM; cfg.pin_reset=RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 10000000;
  cfg.pixel_format = PIXFORMAT_RGB565;
  cfg.frame_size   = FRAMESIZE_SVGA;             /* 800×600             */
  cfg.fb_count     = 1;

  if (esp_camera_init(&cfg)!=ESP_OK){ Serial.println("Cam init fail"); while(true); }
}

/*--------------------------  Loop -----------------------------*/
void loop()
{
  static uint32_t last=0;
  if (millis()-last > SEND_INTERVAL_MS){
    last = millis();
    sendFrameAsBmp();
  }
}
