//! Display and touch driver for the SeeedStudio XIAO Round Display
//! Adapted from https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_log.h>
#include <lvgl.h>

constexpr lv_coord_t SCREEN_WIDTH = 240;
constexpr lv_coord_t SCREEN_HEIGHT = 240;
constexpr int LVGL_BUF_SIZE = 100; // Number of rows

constexpr int CHSC6X_I2C_ID = 0x2e;
constexpr int CHSC6X_READ_POINT_LEN = 5;
constexpr uint8_t TOUCH_INT = GPIO_NUM_12;

constexpr uint8_t DP_BL = GPIO_NUM_48;
constexpr uint8_t DP_DC = GPIO_NUM_35;
constexpr uint8_t DP_CS = GPIO_NUM_36;
constexpr uint8_t DP_SCK = GPIO_NUM_13;
constexpr uint8_t DP_MOSI = GPIO_NUM_17;
constexpr uint8_t DP_MISO = GPIO_NUM_14;

uint8_t screen_rotation = 0;

#define SPI_FREQ 20000000 // SPI bus frequency: 20Mhz
Arduino_DataBus* bus =
    new Arduino_ESP32SPI(DP_DC, DP_CS, DP_SCK, DP_MOSI, DP_MISO, FSPI);
Arduino_GFX* gfx = new Arduino_GC9A01(bus, -1, screen_rotation, true);

void xiao_disp_flush(lv_display_t* disp, lv_area_t const* area,
                     uint8_t* px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  uint16_t* px_buf = (uint16_t*)px_map;

  gfx->draw16bitRGBBitmap(area->x1, area->y1, px_buf, w, h);
  lv_display_flush_ready(disp);
}

void xiao_disp_init() {
  pinMode(DP_BL, OUTPUT);
  digitalWrite(DP_BL, HIGH);

  gfx->begin(SPI_FREQ);
  gfx->fillScreen(BLACK);
}

void lv_xiao_disp_init() {
  xiao_disp_init();

  static uint8_t draw_buf[SCREEN_WIDTH * LVGL_BUF_SIZE * LV_COLOR_DEPTH / 8];
  lv_display_t* disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, xiao_disp_flush);
  lv_display_set_buffers(disp, (void*)draw_buf, NULL, sizeof(draw_buf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
}

bool chsc6x_is_pressed() {
  return digitalRead(TOUCH_INT) == LOW;

  // Debouncing. Done by the original library, doesn't seem to really change
  // anything
  // if (digitalRead(TOUCH_INT) != LOW) {
  //   delay(1);
  //   if (digitalRead(TOUCH_INT) != LOW)
  //     return false;
  // }
  // return true;
}

void chsc6x_convert_xy(uint8_t* x, uint8_t* y) {
  uint8_t x_tmp = *x, y_tmp = *y, _end = 0;
  for (int i = 1; i <= screen_rotation; i++) {
    x_tmp = *x;
    y_tmp = *y;
    _end = (i % 2) ? SCREEN_WIDTH : SCREEN_HEIGHT;
    *x = y_tmp;
    *y = _end - x_tmp;
  }
}

void chsc6x_get_xy(lv_coord_t* x, lv_coord_t* y) {
  uint8_t temp[CHSC6X_READ_POINT_LEN] = {0};
  uint8_t read_len = Wire.requestFrom(CHSC6X_I2C_ID, CHSC6X_READ_POINT_LEN);
  if (read_len != CHSC6X_READ_POINT_LEN)
    return;

  Wire.readBytes(temp, read_len);
  if (temp[0] == 0x01) {
    chsc6x_convert_xy(&temp[2], &temp[4]);
    *x = temp[2];
    *y = temp[4];
  }
}

void chsc6x_read(lv_indev_t* indev, lv_indev_data_t* data) {
  if (!chsc6x_is_pressed()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  lv_coord_t touch_x, touch_y;
  chsc6x_get_xy(&touch_x, &touch_y);

  if (touch_x > SCREEN_WIDTH || touch_y > SCREEN_WIDTH) {
    ESP_LOGD("Display", "Filtered out-of-bounds position (%d, %d)", touch_x,
             touch_y);
    return;
  }

  auto dist_center_sq =
      sq(touch_x - SCREEN_WIDTH / 2) + sq(touch_y - SCREEN_HEIGHT / 2);
  if (dist_center_sq > sq(SCREEN_WIDTH / 2)) {
    ESP_LOGD("Display", "Filtered out-of-display-circle position (%d, %d)",
             touch_x, touch_y);
    return;
  }

  data->state = LV_INDEV_STATE_PRESSED;
  ESP_LOGD("Display", "Touch %d, %d", touch_x, touch_y);
  data->point.x = touch_x;
  data->point.y = touch_y;
}

void lv_xiao_touch_init() {
  pinMode(TOUCH_INT, INPUT_PULLUP);
  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, chsc6x_read);
}
