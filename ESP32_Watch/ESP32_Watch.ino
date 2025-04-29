#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include "pin_config.h"
#include "lv_conf.h"
#include <D:/Aquib/MyArduino/libraries/lvgl/src/demos/lv_demos.h>
#include "HWCDC.h"
#include "image.h"
#include <FastLED.h>
#include <WiFi.h>
#include <time.h>
#include <time.h>


// — Free pins —
#define FALL_PIN 3      // fall-detect input
#define SOS_BTN_PIN 2  // hardware SOS long-press
#define BUZZER_PIN 17   // buzzer output
#define DATA_PIN 18
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];      // Define an array of CRGB objects to hold the LED data

// Wi-Fi Socket configuration
const char *WIFI_SSID = "MyProject";
const char *WIFI_PASS = "12345678";
const uint16_t WIFI_PORT = 3333;
WiFiServer wifiServer(WIFI_PORT);
WiFiClient wifiClient;

HWCDC USBSerial;

// LVGL + Display + Touch bus
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, true, LCD_WIDTH, LCD_HEIGHT, 0, 20, 0, 0);
std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

// Touch controller
void Arduino_IIC_Touch_Interrupt(void);
std::unique_ptr<Arduino_IIC> CST816T(new Arduino_CST816x(IIC_Bus, CST816T_DEVICE_ADDRESS,
                                                         TP_RST, TP_INT, Arduino_IIC_Touch_Interrupt));

// LVGL globals
static lv_disp_draw_buf_t draw_buf;
uint32_t screenWidth, screenHeight;
static lv_obj_t *pin_label;
static char pin_buf[5] = "";
static uint8_t pin_index = 0;

// NeoPixel strip & heartbeat state
bool heartbeatOn = true;
uint32_t lastBlinkMs = 0;

// SOS state
bool sosActive = false;
uint32_t sosStartMs = 0;

// LVGL tick period
#define LVGL_TICK_MS 2

// Forward declarations
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void example_increase_lvgl_tick(void *);
void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
static void pin_btn_event_cb(lv_event_t *e);
static void create_pin_ui();
static void create_watchface_ui();
void startSOS();

#if LV_USE_LOG
void my_print(const char *buf) {
  USBSerial.printf(buf);
}
#endif
static lv_obj_t * time_label;

// Timer callback to update the clock every second
static void update_time_cb(lv_timer_t * timer) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[16];
    // Format as HH:MM:SS
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    lv_label_set_text(time_label, buf);
  }
}
void setup() {
  USBSerial.begin(115200);
  // Touch init
  while (!CST816T->begin()) {
    USBSerial.println("CST816T init fail");
    delay(2000);
  }
  CST816T->IIC_Write_Device_State(
    CST816T->Arduino_IIC_Touch::Device::TOUCH_DEVICE_INTERRUPT_MODE,
    CST816T->Arduino_IIC_Touch::Device_Mode::TOUCH_DEVICE_INTERRUPT_PERIODIC);

  // Display init
  gfx->begin();
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  screenWidth = gfx->width();
  screenHeight = gfx->height();
  lv_init();

  // Draw buffers
  lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);
  lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);
#if LV_USE_LOG
  lv_log_register_print_cb(my_print);
#endif
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * screenHeight / 4);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Touch driver
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize the LED strip
  FastLED.setBrightness(50);

  // Buttons & buzzer
  pinMode(FALL_PIN, INPUT_PULLUP);
  pinMode(SOS_BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // LVGL tick timer
  const esp_timer_create_args_t tick_args = { .callback = &example_increase_lvgl_tick, .name = "lvgl_tick" };
  esp_timer_handle_t tick_timer;
  esp_timer_create(&tick_args, &tick_timer);
  esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000);

  // WiFi socket
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  USBSerial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    USBSerial.print('.');
  }
  USBSerial.println();
  USBSerial.print("IP: ");
  USBSerial.println(WiFi.localIP());
  wifiServer.begin();
  USBSerial.printf("Server on %d\n", WIFI_PORT);

  configTime(5 * 3600 + 30 * 60, 0, "pool.ntp.org", "time.nist.gov");

  // wait up to 5 sec for time to be set
  Serial.print("Waiting for NTP time sync");
  time_t now = time(nullptr);
  for (int i = 0; i < 10 && now < 8 * 3600 * 2; ++i) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  // Show clock screen by default
  create_clock_ui();
}
static void create_clock_ui() {
  lv_obj_clean(lv_scr_act());

  // Create a full-screen label
  time_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_26, 0);
  lv_obj_center(time_label);

  // Immediately set it once
  update_time_cb(nullptr);

  // Then schedule regular updates every 1000 ms
  lv_timer_create(update_time_cb, 1000, nullptr);
}
void loop() {
  lv_timer_handler();
  delay(5);

  // Accept client
  if ((!wifiClient || !wifiClient.connected()) && wifiServer.hasClient()) {
    wifiClient = wifiServer.available();
    USBSerial.println("Client connected");
  }

  fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));

  // Fall-detect trigger
  if (digitalRead(FALL_PIN) == LOW && !sosActive) {
    startSOS();
  }
  uint32_t now = millis();

  // Long-press SOS button
  static uint32_t btnDown = 0;
  if (digitalRead(SOS_BTN_PIN) == LOW) {
    if (!btnDown) btnDown = now;
    else if (!sosActive && now - btnDown >= 60000) startSOS();
  } else btnDown = 0;

  // Send SOS SMS after 60s if not cancelled
  if (sosActive && now - sosStartMs >= 60000) {
    int hr = random(60, 100);
    if (wifiClient && wifiClient.connected()) {
      wifiClient.printf("SOS_SMS:HR=%d\n", hr);
    }
    sosActive = false;
  }
}

// — LVGL flush —
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
  lv_disp_flush_ready(disp);
}

// — LVGL tick —
void example_increase_lvgl_tick(void *arg) {
  lv_tick_inc(LVGL_TICK_MS);
}

// — Touch read —
void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  int32_t x = CST816T->IIC_Read_Device_Value(
    CST816T->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
  int32_t y = CST816T->IIC_Read_Device_Value(
    CST816T->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
  if (CST816T->IIC_Interrupt_Flag) {
    CST816T->IIC_Interrupt_Flag = false;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else data->state = LV_INDEV_STATE_REL;
}

// — Create watch face —
static void create_watchface_ui() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t *btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn, 100, 100);
  lv_obj_center(btn);
  lv_obj_add_event_cb(
    btn, [](lv_event_t *e) {
      if (!sosActive) {
        heartbeatOn = !heartbeatOn;
        int hr = random(60, 100);
        if (wifiClient && wifiClient.connected()) wifiClient.printf("HEART:%d\n", hr);
      }
    },
    LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "♥");
  lv_obj_center(lbl);
}

// — Start SOS —
void startSOS() {
  sosActive = true;
  sosStartMs = millis();
  digitalWrite(BUZZER_PIN, HIGH);
  if (wifiClient && wifiClient.connected()) wifiClient.println("SOS_STARTED");
  create_pin_ui();
}

// — PIN UI —
static void pin_btn_event_cb(lv_event_t *e) {
  if (!sosActive) return;
  lv_obj_t *btn = lv_event_get_target(e);
  const char *c = lv_label_get_text(lv_obj_get_child(btn, 0));
  if (pin_index < 4) {
    pin_buf[pin_index++] = c[0];
    pin_buf[pin_index] = '\0';
    lv_label_set_text_fmt(pin_label, "PIN: %s", pin_buf);
  }
  if (pin_index == 4) {
    if (strcmp(pin_buf, "1234") == 0) {
      digitalWrite(BUZZER_PIN, LOW);
      sosActive = false;
      if (wifiClient && wifiClient.connected()) wifiClient.println("SOS_CANCELLED");
      create_watchface_ui();
    } else {
      pin_index = 0;
      pin_buf[0] = '\0';
      lv_label_set_text(pin_label, "PIN: ");
    }
  }
}

static void create_pin_ui() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t *card = lv_obj_create(lv_scr_act());
  lv_obj_set_size(card, 210, 180);
  lv_obj_center(card);
  lv_obj_set_style_pad_all(card, 8, 0);
  pin_label = lv_label_create(card);
  lv_label_set_text(pin_label, "PIN: ");
  lv_obj_align(pin_label, LV_ALIGN_TOP_MID, 0, 0);
  static const char *nums[4] = { "1", "2", "3", "4" };
  for (int i = 0; i < 4; i++) {
    lv_obj_t *b = lv_btn_create(card);
    lv_obj_set_size(b, 70, 50);
    lv_obj_align(b, LV_ALIGN_CENTER, (i % 2) ? 50 : -50, (i / 2) ? 50 : -10);
    lv_obj_add_event_cb(b, pin_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, nums[i]);
    lv_obj_center(l);
  }
}

void Arduino_IIC_Touch_Interrupt() {
  CST816T->IIC_Interrupt_Flag = true;
}
