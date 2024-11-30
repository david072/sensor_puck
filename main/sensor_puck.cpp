#include "display_driver.h"
#include <bm8563.h>
#include <data.h>
#include <driver/adc.h>
#include <driver/i2c_master.h>
#include <driver/rtc_io.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <lsm6dsox.h>
#include <lvgl.h>
#include <sensors/bme688.h>
#include <ui/pages.h>
#include <ui/ui.h>
#include <vector>

constexpr lv_color_t BACKGROUND_COLOR = make_color(0x1a, 0x1a, 0x1a);

constexpr u32 BATTERY_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 BATTERY_READ_INTERVAL = 30 * 1000;
constexpr u32 BATTERY_SAMPLES = 20;

constexpr u32 BME_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 BME_READ_INTERVAL_MS = 30 * 1000;

constexpr u32 LSM_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 LSM_READ_INTERVAL_MS = 50;

constexpr u32 DISPLAY_INACTIVITY_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 DISPLAY_INACTIVITY_TASK_INTERVAL_MS = 10 * 1000;

i2c_master_bus_handle_t g_i2c_handle;

Bm8563* g_rtc;

std::vector<ui::Page*> g_pages = {};
lv_obj_t* g_ui_container;

struct DeepSleepTimer {
  long sleep_start = 0;
  int original_timer_duration = 0;
  int remaining_timer_duration = 0;
};

RTC_DATA_ATTR DeepSleepTimer deep_sleep_timer = DeepSleepTimer{};

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

void bme_read_task(void* arg) {
  auto init_bme = []() {
    auto i2c_guard = Data::the()->lock_i2c();
    Bme688 bme(g_i2c_handle);
    bme.set_temperature_oversampling(Bme688::Oversampling::Os8x);
    bme.set_humidity_oversampling(Bme688::Oversampling::Os2x);
    bme.set_pressure_oversampling(Bme688::Oversampling::Os4x);
    bme.set_iir_filter_size(Bme688::FilterSize::Size3);
    return bme;
  };

  auto bme = init_bme();

  while (true) {
    {
      auto data = Data::the();
      auto i2c_guard = data->lock_i2c();
      auto values = bme.read_sensor();
      if (values.has_value()) {
        data->update_environment_measurements(
            values->temperature, values->humidity, values->pressure);
      } else {
        ESP_LOGE("BME688", "Failed reading sensor!");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(BME_READ_INTERVAL_MS));
  }
}

void lsm_read_task(void* arg) {
  auto init_lsm = []() {
    auto i2c_guard = Data::the()->lock_i2c();
    Lsm6dsox lsm(g_i2c_handle);
    return lsm;
  };

  auto lsm = init_lsm();

  while (true) {
    {
      auto data = Data::the();
      auto i2c_guard = data->lock_i2c();
      auto values = lsm.read_sensor();

      data->update_inertial_measurements(
          Vector3(values.acc_x, -values.acc_z, values.acc_y),
          Vector3(values.pitch, values.yaw, values.roll));
    }

    vTaskDelay(pdMS_TO_TICKS(LSM_READ_INTERVAL_MS));
  }
}

void battery_read_task(void* arg) {
  adc_unit_t adc_unit;
  adc_channel_t adc_channel;
  ESP_ERROR_CHECK(
      adc_oneshot_io_to_channel(BATTERY_READ_PIN, &adc_unit, &adc_channel));

  adc_oneshot_unit_handle_t adc_handle;
  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = adc_unit,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

  adc_oneshot_chan_cfg_t config = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &config));

  adc_cali_handle_t cali_handle;
  adc_cali_curve_fitting_config_t cali_config = {
      .unit_id = adc_unit,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(
      adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle));

  while (true) {
    int battery_voltage = 0;
    for (int i = 0; i < BATTERY_SAMPLES; ++i) {
      int v;
      ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(adc_handle, cali_handle,
                                                        adc_channel, &v));
      battery_voltage += v;
    }

    Data::the()->update_battery_percentage(battery_voltage / BATTERY_SAMPLES);

    vTaskDelay(pdMS_TO_TICKS(BATTERY_READ_INTERVAL));
  }
}

void update_system_time_from_rtc() {
  auto data = Data::the();
  auto guard = data->lock_i2c();
  auto rtc = g_rtc->read_date_time();
  tm rt = {
      .tm_sec = rtc.second,
      .tm_min = rtc.minute,
      .tm_hour = rtc.hour,
      .tm_mday = rtc.day,
      .tm_mon = rtc.month,
      // tm_year should be relative to 1900
      .tm_year = rtc.year - 1900,
  };

  ESP_LOGI("BM8563", "Updating system time from RTC");
  data->set_time(rt);
}

void set_rtc_time(struct tm tm) {
  auto guard = Data::the()->lock_i2c();
  ESP_LOGI("BM8563", "Setting RTC time to %02d:%02d:%02d, %02d.%02d.%04d",
           tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_mday, tm.tm_mon, tm.tm_year);
  Bm8563::DateTime dt = {
      // tm_year is relative to 1900
      .year = tm.tm_year + 1900,
      .month = tm.tm_mon,
      .day = tm.tm_mday,
      .hour = static_cast<int8_t>(tm.tm_hour),
      .minute = static_cast<int8_t>(tm.tm_min),
      .second = static_cast<int8_t>(tm.tm_sec),
  };
  g_rtc->set_date_time(dt);
}

void display_inactivity_task(void* arg) {
  while (true) {
    {
      auto d = Data::the();
      auto guard = d->lock_lvgl();
      if (lv_display_get_inactive_time(NULL) >
          DEEP_SLEEP_DISPLAY_INACTIVITY_MS) {
        if (ui::Ui::the().in_fullscreen()) {
          lv_display_trigger_activity(NULL);
        } else {
          // enter  deep-sleep
          // TODO: Do proper cleanup? We do a lot of heap allocations that are
          // never
          //  freed, but then RAM is reset on wakeup, so does it matter?
          lv_obj_clean(lv_scr_act());
          lv_refr_now(NULL);

          if (d->remaining_timer_duration_ms() > 0) {
            deep_sleep_timer.sleep_start = time(NULL) * 1000;
            deep_sleep_timer.original_timer_duration =
                d->original_timer_duration_ms();
            deep_sleep_timer.remaining_timer_duration =
                d->remaining_timer_duration_ms();
            esp_sleep_enable_timer_wakeup(
                static_cast<uint64_t>(d->remaining_timer_duration_ms()) * 1000);
          } else {
            deep_sleep_timer = DeepSleepTimer{};
          }

          set_backlight(false);
          rtc_gpio_pullup_en(DP_TOUCH_INT);
          esp_sleep_enable_ext0_wakeup(DP_TOUCH_INT, 0);
          esp_deep_sleep_start();
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_INACTIVITY_TASK_INTERVAL_MS));
  }
}

void recover_from_sleep() {
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

extern "C" void app_main() {
  esp_event_loop_create_default();

  // FIXME: The time between touching the display and the screen showing an
  // image is too long. Why?

  ESP_LOGI("Setup", "Initialize I2C master bus");
  gpio_reset_pin(B_SDA);
  gpio_reset_pin(B_SCL);
  i2c_master_bus_config_t i2c_config = {
      .i2c_port = 0,
      .sda_io_num = B_SDA,
      .scl_io_num = B_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags =
          {
              .enable_internal_pullup = true,
          },
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &g_i2c_handle));

  g_rtc = new Bm8563(g_i2c_handle);
  update_system_time_from_rtc();

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::TimeChanged,
      [](void*, esp_event_base_t, int32_t, void*) {
        // don't block in event handler
        xTaskCreate(
            [](void*) {
              tm time = Data::the()->get_time();
              set_rtc_time(time);
              vTaskDelete(NULL);
            },
            "TCE to RTC", 4 * 2048, NULL, 1, NULL);
      },
      NULL);

  ESP_LOGI("Setup", "Initialize display");
  init_display();
  init_display_touch(g_i2c_handle);

  {
    auto lvgl_guard = Data::the()->lock_lvgl();
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
  }

  ESP_LOGI("Setup", "Starting sensor tasks");
  xTaskCreate(bme_read_task, "BME688", BME_TASK_STACK_SIZE, NULL,
              BME_TASK_PRIORITY, NULL);
  xTaskCreate(lsm_read_task, "LSM6DSOX", LSM_TASK_STACK_SIZE, NULL,
              LSM_TASK_PRIORITY, NULL);
  xTaskCreate(battery_read_task, "Battery", BATTERY_TASK_STACK_SIZE, NULL,
              BATTERY_TASK_PRIORITY, NULL);

  xTaskCreate(display_inactivity_task, "DP inact",
              DISPLAY_INACTIVITY_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY,
              NULL);

  recover_from_sleep();
  ESP_LOGI("Setup", "Setup finished successfully!");
}
