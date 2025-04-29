/*** ─────────────────────────────────────────────────────────────
 *  Heart-Sound Logger – ESP32 + INMP441 + OPTIONAL SD logging
 *  ─────────────────────────────────────────────────────────────
 *  • Set LOG_TO_SD = true  → CSV is written to /heartbeat.csv
 *  • Set LOG_TO_SD = false → no SD initialisation, no writes
 *  • Live graph on http://<ESP-IP> always works
 *  ------------------------------------------------------------*/

#include <WiFi.h>
#include <WebServer.h>
#include "driver/i2s.h"

#define LOG_TO_SD  true          // ← flip to false to disable SD card
#if LOG_TO_SD
  #include <SD.h>
  #include <SPI.h>
#endif

/* ---------- USER CONFIG ------------------- */
const char* ssid     = "MyProject";
const char* password = "12345678";

/* SD-card (SPI) */
#if LOG_TO_SD
  constexpr uint8_t SD_CS = 5;
  #define CSV_FILE "/heartbeat.csv"
  File csv;
#endif

/* I²S pin map for INMP441 */
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
constexpr int BCLK_PIN = 26;
constexpr int LRCK_PIN = 25;
constexpr int DIN_PIN  = 33;

/* Audio & logging parameters */
constexpr uint32_t SAMPLE_RATE        = 16000;
constexpr uint16_t BLOCK_MS           = 100;
constexpr uint16_t SAMPLES_PER_BLOCK  = SAMPLE_RATE * BLOCK_MS / 1000;
constexpr size_t   I2S_BYTES_PER_BLOCK = SAMPLES_PER_BLOCK * sizeof(int32_t);

/* Ring buffer for web graph */
constexpr size_t GRAPH_SIZE = 250;
struct Sample { uint32_t t; float amp; };
Sample  ring[GRAPH_SIZE];
volatile size_t ringHead = 0;

/* Globals */
WebServer server(80);

/* ---------- I²S helpers ------------------- */
void i2s_install()
{
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 512,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = BCLK_PIN,
    .ws_io_num    = LRCK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = DIN_PIN
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
}

/* ---------- SD helpers -------------------- */
#if LOG_TO_SD
void sd_begin()
{
  if (!SD.begin(SD_CS)) {
    Serial.println(F("ERROR: SD card not found; logging disabled."));
    while (true) delay(100);      // comment this line if you prefer to keep running
  }
  if (!SD.exists(CSV_FILE)) {
    csv = SD.open(CSV_FILE, FILE_WRITE);
    if (csv) { csv.println(F("timestamp_ms,rms")); csv.close(); }
  }
}
#endif

/* ---------- Web page (unchanged) ---------- */
const char pageIndex[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang=en> … … same HTML as before … </html>
)rawliteral";

void handleRoot() { server.send_P(200,"text/html",pageIndex); }

void handleData()
{
  String j="[";
  noInterrupts();
  size_t n = ringHead;
  interrupts();

  for (size_t i=0;i<GRAPH_SIZE;i++) {
    size_t idx=(n+i)%GRAPH_SIZE;
    float a=ring[idx].amp;
    if(a<=0) continue;
    j+="["+String(ring[idx].t)+","+String(a,3)+"],";
  }
  if(j.endsWith(",")) j.remove(j.length()-1);
  j+="]";
  server.send(200,"application/json",j);
}

void web_begin()
{
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.print(F("Web server: http://")); Serial.println(WiFi.localIP());
}

/* ---------- Setup & loop ------------------ */
void setup()
{
  Serial.begin(115200); delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print(F("WiFi…"));
  while (WiFi.status() != WL_CONNECTED) { Serial.print('.'); delay(400); }
  Serial.println("connected.");

  i2s_install();
  #if LOG_TO_SD
    sd_begin();
  #endif
  web_begin();
}

/* RMS helper */
float computeRMS(int32_t* buf, size_t count)
{
  uint64_t sumSq=0;
  for(size_t i=0;i<count;++i){
    int32_t s=buf[i]>>8;
    sumSq+=(int64_t)s*(int64_t)s;
  }
  return sqrt((double)sumSq/count)/8388608.0;
}

void loop()
{
  static int32_t* i2sBuf = (int32_t*)malloc(I2S_BYTES_PER_BLOCK);
  size_t bytesRead=0;
  i2s_read(I2S_PORT,(void*)i2sBuf,I2S_BYTES_PER_BLOCK,&bytesRead,portMAX_DELAY);
  size_t samples=bytesRead/sizeof(int32_t);
  if(!samples) return;

  float rms=computeRMS(i2sBuf,samples);
  uint32_t now=millis();

  ring[ringHead]={now,rms};
  ringHead=(ringHead+1)%GRAPH_SIZE;

  #if LOG_TO_SD
    File f=SD.open(CSV_FILE,FILE_APPEND);
    if(f){ f.printf("%lu,%.6f\n",now,rms); f.close(); }
  #endif

  server.handleClient();
}
