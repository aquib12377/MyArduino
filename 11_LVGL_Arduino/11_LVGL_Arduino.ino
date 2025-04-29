#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include "pin_config.h"
#include "lv_conf.h"
#include <D:/Aquib/MyArduino/libraries/lvgl/src/demos/lv_demos.h>
#include "HWCDC.h"
#include "image.h"

HWCDC USBSerial;

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);

Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST /* RST */,
                                      0 /* rotation */, true /* IPS */, LCD_WIDTH, LCD_HEIGHT, 0, 20, 0, 0);

static lv_obj_t *pin_label;           // shows the digits already typed
static char      pin_buf[5] = "";     // holds the 4‑digit PIN (+ NUL)
static uint8_t   pin_index  = 0;      // number of digits typed 0‑4
std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
  std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

void Arduino_IIC_Touch_Interrupt(void);

std::unique_ptr<Arduino_IIC> CST816T(new Arduino_CST816x(IIC_Bus, CST816T_DEVICE_ADDRESS,
                                                         TP_RST, TP_INT, Arduino_IIC_Touch_Interrupt));

void Arduino_IIC_Touch_Interrupt(void) {
  CST816T->IIC_Interrupt_Flag = true;
}

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

uint32_t screenWidth;
uint32_t screenHeight;

static lv_disp_draw_buf_t draw_buf;
// static lv_color_t buf[screenWidth * screenHeight / 10];


#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void example_increase_lvgl_tick(void *arg) {
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static uint8_t count = 0;
void example_increase_reboot(void *arg) {
  count++;
  if (count == 30) {
    esp_restart();
  }
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  int32_t touchX = CST816T->IIC_Read_Device_Value(CST816T->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
  int32_t touchY = CST816T->IIC_Read_Device_Value(CST816T->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);

  if (CST816T->IIC_Interrupt_Flag == true) {
    CST816T->IIC_Interrupt_Flag = false;
    data->state = LV_INDEV_STATE_PR;

    /* Set the coordinates with some debounce */
    if (touchX >= 0 && touchY >= 0) {
      data->point.x = touchX;
      data->point.y = touchY;

      USBSerial.printf("Data x: %d, Data y: %d\n", touchX, touchY);
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}


void setup() {
  USBSerial.begin(115200); /* prepare for possible serial debug */

  while (CST816T->begin() == false) {
    USBSerial.println("CST816T initialization fail");
    delay(2000);
  }
  USBSerial.println("CST816T initialization successfully");

  CST816T->IIC_Write_Device_State(CST816T->Arduino_IIC_Touch::Device::TOUCH_DEVICE_INTERRUPT_MODE,
                                  CST816T->Arduino_IIC_Touch::Device_Mode::TOUCH_DEVICE_INTERRUPT_PERIODIC);

  gfx->begin();
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  lv_init();

  lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);

  lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  USBSerial.println(LVGL_Arduino);
  USBSerial.println("I am LVGL_Arduino");



#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * screenHeight / 4);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Hello Ardino and LVGL!");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };

  const esp_timer_create_args_t reboot_timer_args = {
    .callback = &example_increase_reboot,
    .name = "reboot"
  };

  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);
  create_pin_ui();
  USBSerial.println("Setup done – keypad ready");  // lv_demo_benchmark();
  lv_example_style_1();
  // lv_demo_music();
  // lv_demo_stress();

  // lv_obj_t *img_obj = lv_img_create(lv_scr_act());
  // lv_img_set_src(img_obj, &img_test3);  // Set the image source to img_test3
  // lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 0);
  // USBSerial.println("Setup done");
}
/*— button‑event handler —*/
static void pin_btn_event_cb(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  const char *txt = lv_label_get_text(lv_obj_get_child(btn, 0));   // button caption

  if (pin_index < 4) {               // still room for another digit
    pin_buf[pin_index++] = txt[0];   // copy digit into the buffer
    pin_buf[pin_index]   = '\0';     // keep it NUL‑terminated
    lv_label_set_text_fmt(pin_label, "PIN: %s", pin_buf);
  }

  if (pin_index == 4) {
    /*  >>> 4‑digit PIN complete –‑ do whatever you need here <<<  */
    USBSerial.printf("PIN entered: %s\n", pin_buf);
    /* simple feedback: flash the text for half a second */
    lv_label_set_text(pin_label, "PIN OK!");
    lv_timer_t *t = lv_timer_create([](lv_timer_t *){
                                      pin_index = 0;
                                      pin_buf[0] = '\0';
                                      lv_label_set_text(pin_label, "PIN: ");
                                    },
                                    500, nullptr);
    lv_timer_set_repeat_count(t, 1);  // one‑shot timer
  }
}

/*— build the whole screen —*/
static void create_pin_ui(void)
{
  /* container = white card, centred */
  lv_obj_t *card = lv_obj_create(lv_scr_act());
  lv_obj_set_size(card, 210, 180);
  lv_obj_center(card);
  lv_obj_set_style_pad_all(card, 8, 0);
  /* title / feedback label */
  pin_label = lv_label_create(card);
  lv_label_set_text(pin_label, "PIN: ");
  lv_obj_align(pin_label, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_text_font(pin_label, &lv_font_montserrat_14, 0);

  /* 4 buttons laid out in a 2 × 2 grid */
  static const char *numbers[4] = { "1", "2", "3", "4" };

  for (uint8_t i = 0; i < 4; i++) {
    lv_obj_t *btn = lv_btn_create(card);
    lv_obj_set_size(btn, 70, 50);
    lv_obj_align(btn,
                 LV_ALIGN_CENTER,
                 (i % 2) ? 50 : -50,          // X offsets: –50, 50
                 (i / 2) ? 50 : -10);         // Y offsets: –10, 50
    lv_obj_add_event_cb(btn, pin_btn_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, numbers[i]);
    lv_obj_center(lbl);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  }
}
void loop() {
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
