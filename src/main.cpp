#include "data.h"
#include "pages.h"
#include "ui.h"
#include <Adafruit_BME680.h>
#include <Adafruit_LSM6DSOX.h>
#include <Arduino.h>

#define USE_ARDUINO_GFX_LIBRARY
#include "lv_xiao_round_screen.h"

#include <I2C_BM8563.h>
#include <lvgl.h>

/// MEZ time zone
constexpr timezone TIMEZONE = {
    .tz_minuteswest = 1 * 60,
    .tz_dsttime = DST_WET,
};

constexpr lv_color_t BACKGROUND_COLOR = make_color(0x1a, 0x1a, 0x1a);

constexpr ulong BATTERY_READ_INTERVAL = 10000;
constexpr int32_t BATTERY_SAMPLES = 20;

constexpr ulong BME_READ_INTERVAL = 10000;

std::vector<ui::Page*> g_pages = {};
lv_obj_t* g_ui_container;

I2C_BM8563 g_rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);
Adafruit_BME680 g_bme688;
Adafruit_LSM6DSOX g_lsm6;

ulong g_last_battery_read = 0;
ulong g_last_bme_read = 0;
ulong g_last_lsm_read = 0;
ulong g_last_ui_update = 0;

void disable_scroll_on_fullscreen_enter(lv_obj_t* obj) {
  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        lv_obj_set_scroll_dir(
            static_cast<lv_obj_t*>(lv_event_get_user_data(event)), LV_DIR_NONE);
      },
      ui::Ui::the().enter_fullscreen_event(), obj);
}

void enable_scroll_on_fullscreen_exit(lv_obj_t* obj, lv_dir_t dir) {
  struct Args {
    lv_obj_t* target;
    lv_dir_t dir;
  };

  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        auto* args = static_cast<Args*>(lv_event_get_user_data(event));
        lv_obj_set_scroll_dir(args->target, args->dir);
      },
      ui::Ui::the().exit_fullscreen_event(),
      new Args{.target = obj, .dir = dir});
}

lv_obj_t* snapping_flex_container(lv_obj_t* parent = lv_scr_act()) {
  auto* cont = lv_obj_create(parent);
  lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_bg_opa(cont, 0, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_margin_all(cont, 0, 0);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLL_ONE);
  lv_obj_set_size(cont, lv_obj_get_width(lv_scr_act()),
                  lv_obj_get_height(lv_scr_act()));
  return cont;
}

template <typename... Page>
void add_grouped_pages() {
  auto* container = snapping_flex_container(g_ui_container);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
  lv_obj_set_scroll_dir(container, LV_DIR_HOR);
  lv_obj_set_scroll_snap_x(container, LV_SCROLL_SNAP_START);
  lv_obj_add_flag(container, LV_OBJ_FLAG_EVENT_BUBBLE);

  disable_scroll_on_fullscreen_enter(container);
  enable_scroll_on_fullscreen_exit(container, LV_DIR_HOR);

  ([&] { g_pages.push_back(new Page(container)); }(), ...);
}

I2C_BM8563_TimeTypeDef get_rtc_time() {
  I2C_BM8563_TimeTypeDef time;
  g_rtc.getTime(&time);
  return time;
}

I2C_BM8563_DateTypeDef get_rtc_date() {
  I2C_BM8563_DateTypeDef date;
  g_rtc.getDate(&date);
  return date;
}

void update_system_time_from_rtc() {
  auto rtc_time = get_rtc_time();
  auto rtc_date = get_rtc_date();

  tm rt = {
      .tm_sec = rtc_time.seconds,
      .tm_min = rtc_time.minutes,
      .tm_hour = rtc_time.hours,
      .tm_mday = rtc_date.date,
      .tm_mon = rtc_date.month - 1,
      .tm_year = rtc_date.year,
  };

  timeval time = {
      .tv_sec = mktime(&rt),
      .tv_usec = 0,
  };
  settimeofday(&time, &TIMEZONE);
}

void setup() {
  Serial.begin(115200);
  gpio_reset_pin(GPIO_NUM_8);
  gpio_reset_pin(GPIO_NUM_7);
  Wire.setPins(GPIO_NUM_8, GPIO_NUM_7);
  Wire.begin();
  esp_log_level_set("*", ESP_LOG_DEBUG);

  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();

  g_rtc.begin();
  update_system_time_from_rtc();

  g_bme688.begin(0x76);
  g_lsm6.begin_I2C();

  lv_obj_set_style_bg_color(lv_scr_act(), BACKGROUND_COLOR, 0);
  lv_obj_set_style_text_color(lv_scr_act(), lv_color_white(), 0);
  lv_obj_set_style_border_width(lv_scr_act(), 0, 0);
  lv_obj_set_scroll_dir(lv_scr_act(), LV_DIR_NONE);
  lv_obj_set_style_pad_all(lv_scr_act(), 0, 0);

  g_ui_container = snapping_flex_container();
  lv_obj_set_flex_flow(g_ui_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(g_ui_container, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(g_ui_container, LV_SCROLL_SNAP_START);

  disable_scroll_on_fullscreen_enter(g_ui_container);
  enable_scroll_on_fullscreen_exit(g_ui_container, LV_DIR_VER);

  add_grouped_pages<ui::ClockPage, ui::TimerPage>();
  add_grouped_pages<ui::AirQualityPage>();
  add_grouped_pages<ui::CompassPage>();
}

void loop() {
  if (millis() - g_last_battery_read >= BATTERY_READ_INTERVAL) {
    g_last_battery_read = millis();

    int32_t milli_volts = 0;
    for (int32_t i = 0; i < BATTERY_SAMPLES; ++i) {
      milli_volts += analogReadMilliVolts(GPIO_NUM_9);
    }

    milli_volts /= BATTERY_SAMPLES;
    Data::the()->update_battery_percentage(milli_volts);
  }

  // if (millis() - g_last_bme_read >= BME_READ_INTERVAL) {
  //   g_last_bme_read = millis();

  //   auto temp = g_bme688.readTemperature();
  //   auto humidity = g_bme688.readHumidity();

  //   printf("temp: %f °C, hum: %f\n", temp, humidity);
  // }

  if (millis() - g_last_lsm_read >= 50) {
    g_last_lsm_read = millis();

    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    g_lsm6.getEvent(&accel, &gyro, &temp);

    Data::the()->update_gyroscope(
        Vector3(gyro.gyro.pitch, gyro.gyro.heading, gyro.gyro.roll));
  }

  lv_tick_inc(millis() - g_last_ui_update);
  g_last_ui_update = millis();
  lv_timer_handler();
}
