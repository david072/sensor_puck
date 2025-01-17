#include "display_driver.h"
#include <data.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_lcd_gc9a01.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <stdio.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

constexpr size_t CHSC6X_READ_POINT_LEN = 5;
constexpr long TOUCH_RELEASE_DEBOUNCE_MS = 50;
constexpr u32 CHSC6X_I2C_SPEED = 100000;
constexpr u32 CHSC6X_I2C_TIMEOUT_MS = 50;

i2c_master_dev_handle_t g_chsc6x_handle;

bool lcd_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                             esp_lcd_panel_io_event_data_t* edata,
                             void* user_ctx) {
  lv_display_t* disp = static_cast<lv_display_t*>(user_ctx);
  lv_display_flush_ready(disp);
  return false;
}

// https://github.com/espressif/esp-idf/blob/f420609c332fbd2d2f7f188c6579d046c9560e42/examples/peripherals/lcd/spi_lcd_touch/main/spi_lcd_touch_example_main.c#L114
void lvgl_flush_cb(lv_display_t* disp, lv_area_t const* area, uint8_t* px_map) {
  auto panel_handle =
      static_cast<esp_lcd_panel_handle_t>(lv_display_get_user_data(disp));
  int x1 = area->x1, x2 = area->x2, y1 = area->y1, y2 = area->y2;
  // because SPI LCD is big-endian, we need to swap the RGB bytes order
  lv_draw_sw_rgb565_swap(px_map, (x2 + 1 - x1) * (y2 + 1 - y1));
  esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, px_map);
}

void lvgl_tick_cb(void* arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); }

void lvgl_port_task(void* arg) {
  constexpr u32 MAX_DELAY_MS = 1000 / CONFIG_FREERTOS_HZ;

  ESP_LOGI("Display", "Starting LVGL task");
  while (true) {
    u32 time_until_next;
    {
      auto lvgl_guard = Data::the()->lock_lvgl();
      time_until_next = lv_timer_handler();
      // make sure to not trigger the watchdog waiting for next execution
      time_until_next = MIN(time_until_next, MAX_DELAY_MS);

      if (lv_display_get_inactive_time(NULL) >
          DEEP_SLEEP_DISPLAY_INACTIVITY_MS) {
        esp_event_post(DATA_EVENT_BASE, Data::Event::Inactivity, NULL, 0, 10);
        lv_display_trigger_activity(NULL);
      }
    }
    usleep(1000 * time_until_next);
  }
}

void init_display() {
  gpio_config_t bk_gpio_config = {
      .pin_bit_mask = 1ULL << DP_BL,
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
  set_display_backlight(true);

  ESP_LOGI("Display", "Initialize SPI bus");
  spi_bus_config_t buscfg = {
      .mosi_io_num = DP_MOSI,
      .miso_io_num = DP_MISO,
      .sclk_io_num = DP_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = DP_H_RES * 80 * sizeof(uint16_t),
  };
  ESP_ERROR_CHECK(spi_bus_initialize(DP_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI("Display", "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = DP_CS,
      .dc_gpio_num = DP_DC,
      .spi_mode = 0,
      .pclk_hz = DP_PIXEL_CLOCK,
      .trans_queue_depth = 10,
      .lcd_cmd_bits = DP_CMD_BIT_WIDTH,
      .lcd_param_bits = DP_PARAM_BIT_WIDTH,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
      static_cast<esp_lcd_spi_bus_handle_t>(DP_HOST), &io_config, &io_handle));

  ESP_LOGI("Display", "Install GC9A01 panel driver");
  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = -1,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  set_display_backlight(true);

  ESP_LOGI("Display", "Initialize LVGL library");
  lv_init();

  lv_display_t* display = lv_display_create(DP_H_RES, DP_V_RES);

  size_t draw_buffer_sz = DP_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);

  void* buf1 = malloc(draw_buffer_sz);
  void* buf2 = malloc(draw_buffer_sz);

  lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_user_data(display, panel_handle);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, lvgl_flush_cb);

  ESP_LOGI("Display", "Install LVGL tick timer");
  esp_timer_create_args_t const lvgl_tick_timer_args = {
      .callback = &lvgl_tick_cb,
      .name = "lvgl_tick",
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

  ESP_LOGI("Display",
           "Register panel event callback for flush ready notification");
  esp_lcd_panel_io_callbacks_t const cbs = {
      .on_color_trans_done = lcd_on_color_trans_done,
  };
  ESP_ERROR_CHECK(
      esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));

  ESP_LOGI("Display", "Create LVGL task");
  xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL,
              LVGL_TASK_PRIORITY, NULL);

  ESP_LOGI("Display", "Display setup done!");
}

void clear_display() {
  static constexpr size_t BUF_LINES = 10;

  auto panel = static_cast<esp_lcd_panel_handle_t>(
      lv_display_get_user_data(lv_display_get_default()));

  lv_color16_t* zeroes = static_cast<lv_color16_t*>(
      calloc(DP_H_RES * BUF_LINES, sizeof(lv_color16_t)));

  for (int y = 0; y <= DP_V_RES; y += BUF_LINES - 1) {
    esp_lcd_panel_draw_bitmap(panel, 0, y, DP_H_RES + 1, y + BUF_LINES - 1,
                              zeroes);
  }
}

void set_display_backlight(bool enable) {
  gpio_set_level(DP_BL, enable ? 1 : 0);
}

bool chsc6x_is_pressed() {
  if (gpio_get_level(DP_TOUCH_INT) != 0) {
    usleep(1000);
    if (gpio_get_level(DP_TOUCH_INT) != 0)
      return false;
  }
  return true;
}

void chsc6x_get_xy(u8* x, u8* y) {
  auto i2c_guard = Data::the()->lock_i2c();

  u8 temp[CHSC6X_READ_POINT_LEN] = {0};
  u8 tx = 0;

  auto res =
      i2c_master_transmit_receive(g_chsc6x_handle, &tx, 1, temp,
                                  CHSC6X_READ_POINT_LEN, CHSC6X_I2C_TIMEOUT_MS);
  if (res != ESP_OK) {
    ESP_LOGI("CHSC6X", "Couldn't receive from I2C");
    *x = 0;
    *y = 0;
    return;
  }

  if (temp[0] == 0x01) {
    *x = temp[2];
    *y = temp[4];
  }
}

void chsc6x_read(lv_indev_t* indev, lv_indev_data_t* data) {
  static u32 last_pressed = 0;
  static u8 last_touch_x = 0;
  static u8 last_touch_y = 0;

  if (!chsc6x_is_pressed()) {
    if (millis() - last_pressed > TOUCH_RELEASE_DEBOUNCE_MS) {
      data->state = LV_INDEV_STATE_RELEASED;
    }
    return;
  }

  u8 touch_x, touch_y;
  chsc6x_get_xy(&touch_x, &touch_y);

  data->state = LV_INDEV_STATE_PRESSED;
  data->point.x = last_touch_x;
  data->point.y = last_touch_y;

  if (touch_x < 0 || touch_x > DP_H_RES || touch_y < 0 || touch_y > DP_H_RES) {
    ESP_LOGD("Display", "Filtered out-of-bounds position (%d, %d)", touch_x,
             touch_y);
    return;
  }

  auto dist_center_sq = sq(touch_x - DP_H_RES / 2) + sq(touch_y - DP_V_RES / 2);
  if (dist_center_sq > sq(DP_H_RES / 2)) {
    ESP_LOGD("Display", "Filtered out-of-display-circle position (%d, %d)",
             touch_x, touch_y);
    return;
  }

  data->state = LV_INDEV_STATE_PRESSED;
  ESP_LOGD("Display", "Touch %d, %d", touch_x, touch_y);
  last_pressed = millis();
  data->point.x = last_touch_x = touch_x;
  data->point.y = last_touch_y = touch_y;
}

void init_display_touch(i2c_master_bus_handle_t i2c_handle) {
  i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = I2C_CHSC6X_ADDR,
      .scl_speed_hz = CHSC6X_I2C_SPEED,
  };
  ESP_ERROR_CHECK(
      i2c_master_bus_add_device(i2c_handle, &dev, &g_chsc6x_handle));

  gpio_set_direction(DP_TOUCH_INT, GPIO_MODE_INPUT);
  gpio_set_pull_mode(DP_TOUCH_INT, GPIO_PULLUP_ONLY);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, chsc6x_read);
}
