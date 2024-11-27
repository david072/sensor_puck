#include "constants.h"
#include "data.h"
#include "pages.h"
#include "ui.h"
#include <Adafruit_BME680.h>
#include <Adafruit_LSM6DSOX.h>
#include <Arduino.h>
#include <driver/rtc_io.h>

#include "lv_xiao_round_screen.h"

#include <I2C_BM8563.h>
#include <lvgl.h>

constexpr lv_color_t BACKGROUND_COLOR = make_color(0x1a, 0x1a, 0x1a);

constexpr uint32_t DEEP_SLEEP_DISPLAY_INACTIVITY = 60 * 1000;

constexpr ulong BATTERY_READ_INTERVAL = 10000;
constexpr int32_t BATTERY_SAMPLES = 20;

constexpr ulong BME_READ_INTERVAL = 60 * 1000;
constexpr ulong LSM_READ_INTERVAL = 50;

struct DeepSleepTimer {
  long sleep_start = 0;
  int original_timer_duration = 0;
  int remaining_timer_duration = 0;
};

RTC_DATA_ATTR DeepSleepTimer deep_sleep_timer = DeepSleepTimer{};

std::vector<ui::Page*> g_pages = {};
lv_obj_t* g_ui_container;

I2C_BM8563 g_rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);
Adafruit_BME680 g_bme688;
Adafruit_LSM6DSOX g_lsm6;

ulong g_last_battery_read = 0;
ulong g_last_lsm_read = 0;
ulong g_last_ui_update = 0;

void bme_read_task(void*);

void lvgl_tick() {
  lv_tick_inc(millis() - g_last_ui_update);
  g_last_ui_update = millis();
  lv_timer_handler();
}

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

void set_rtc_time(struct tm tm) {
  I2C_BM8563_TimeTypeDef time = {
      .hours = static_cast<int8_t>(tm.tm_hour),
      .minutes = static_cast<int8_t>(tm.tm_min),
      .seconds = static_cast<int8_t>(tm.tm_sec),
  };
  g_rtc.setTime(&time);
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
      .tm_mon = rtc_date.month,
      // tm_year should be relative to 1900
      .tm_year = rtc_date.year - 1900,
  };

  ESP_LOGI("System", "Updating system time from RTC");
  Data::the()->set_time(rt);
}

void recover_from_wakeup() {
  switch (esp_sleep_get_wakeup_cause()) {
  case ESP_SLEEP_WAKEUP_EXT0: {
    ESP_LOGI("Setup", "Woken up by EXT0");
    if (deep_sleep_timer.sleep_start == 0)
      break;

    auto now = time(NULL) * 1000;
    auto remaining_duration = deep_sleep_timer.remaining_timer_duration -
                              (now - deep_sleep_timer.sleep_start);
    Data::the()->recover_timer(deep_sleep_timer.original_timer_duration,
                               remaining_duration);
    break;
  }
  case ESP_SLEEP_WAKEUP_TIMER: {
    ESP_LOGI("Setup", "Woken up by timer");
    Data::the()->recover_timer(deep_sleep_timer.original_timer_duration, 0);
    break;
  }
  default:
    break;
  }

  deep_sleep_timer = DeepSleepTimer{};
}

void setup() {
  Serial.begin(115200);
  gpio_reset_pin(B_SDA);
  gpio_reset_pin(B_SCL);
  Wire.setPins(B_SDA, B_SCL);
  Wire.begin();
  esp_log_level_set("*", ESP_LOG_DEBUG);

  esp_event_loop_create_default();

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::TimeChanged,
      [](void* handler_arg, esp_event_base_t base, int32_t id,
         void* event_data) { set_rtc_time(Data::the()->get_time()); },
      NULL);

  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();

  g_rtc.begin();
  update_system_time_from_rtc();

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
  add_grouped_pages<ui::AirQualityPage, ui::ExtendedEnvironmentInfoPage>();
  // add_grouped_pages<ui::CompassPage>();

  ui::Ui::the().add_overlay(new ui::TimerPage::TimerOverlay());

  recover_from_wakeup();
  lvgl_tick();

  xTaskCreate(bme_read_task, "BME Read", 2048, NULL, 1, NULL);
}

void bme_read_task(void*) {
  g_bme688.begin(I2C_BME688_ADDR);
  g_bme688.setTemperatureOversampling(BME680_OS_8X);
  g_bme688.setHumidityOversampling(BME680_OS_2X);
  g_bme688.setPressureOversampling(BME680_OS_4X);
  g_bme688.setIIRFilterSize(BME680_FILTER_SIZE_3);

  ESP_LOGI("BME", "BME setup done");

  while (true) {
    {
      auto data = Data::the();
      auto i2c_guard = data->lock_i2c();

      if (g_bme688.performReading()) {
        data->update_environment_measurements(
            g_bme688.temperature, g_bme688.humidity, g_bme688.pressure / 100.0);
      } else {
        ESP_LOGE("BME", "Failed reading BME\n");
      }
    }

    vTaskDelay(BME_READ_INTERVAL / portTICK_RATE_MS);
  }
}

void handle_battery_voltage_read() {
  // if we are called the first time, immediately make a measurement
  if (g_last_battery_read > 0 &&
      millis() - g_last_battery_read < BATTERY_READ_INTERVAL)
    return;

  g_last_battery_read = millis();

  int32_t milli_volts = 0;
  for (int32_t i = 0; i < BATTERY_SAMPLES; ++i) {
    milli_volts += analogReadMilliVolts(BATTERY_READ_PIN);
  }

  milli_volts /= BATTERY_SAMPLES;
  Data::the()->update_battery_percentage(milli_volts);
}

void handle_lsm_read() {
  // if we are called the first time, make data available immediately
  if (g_last_lsm_read > 0 && millis() - g_last_lsm_read < LSM_READ_INTERVAL)
    return;

  g_last_lsm_read = millis();

  auto data = Data::the();
  auto i2c_guard = data->lock_i2c();

  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;
  g_lsm6.getEvent(&accel, &gyro, &temp);

  data->update_inertial_measurements(
      Vector3(accel.acceleration.x, -accel.acceleration.z,
              accel.acceleration.y),
      Vector3(gyro.gyro.pitch, gyro.gyro.heading, gyro.gyro.roll));
}

bool is_inactive() {
  if (lv_display_get_inactive_time(NULL) < DEEP_SLEEP_DISPLAY_INACTIVITY)
    return false;

  if (ui::Ui::the().in_fullscreen()) {
    lv_display_trigger_activity(NULL);
    return false;
  }

  return true;
}

// TODO: Do proper cleanup? We do a lot of heap allocations that are never
//  freed, but then RAM is reset on wakeup, so does it matter?
void enter_deep_sleep() {
  auto d = Data::the();
  if (d->remaining_timer_duration_ms() > 0) {
    deep_sleep_timer.sleep_start = time(NULL) * 1000;
    deep_sleep_timer.original_timer_duration = d->original_timer_duration_ms();
    deep_sleep_timer.remaining_timer_duration =
        d->remaining_timer_duration_ms();
    esp_sleep_enable_timer_wakeup(
        static_cast<uint64_t>(d->remaining_timer_duration_ms()) * 1000);
  } else {
    deep_sleep_timer = DeepSleepTimer{};
  }

  lv_xiao_disp_deinit();
  rtc_gpio_pullup_en(DP_TOUCH_INT);
  esp_sleep_enable_ext0_wakeup(DP_TOUCH_INT, LOW);
  esp_deep_sleep_start();
}

void loop() {
  handle_battery_voltage_read();
  handle_lsm_read();

  if (is_inactive())
    enter_deep_sleep();

  lvgl_tick();
}
