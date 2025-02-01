#include "battery.h"
#include "display_driver.h"
#include <bm8563.h>
#include <bme688.h>
#include <data.h>
#include <driver/adc.h>
#include <driver/i2c_master.h>
#include <driver/rtc_io.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <lsm6dsox.h>
#include <lvgl.h>
#include <scd41.h>
#include <ui/pages.h>
#include <ui/ui.h>
#include <ulp_riscv.h>
#include <ulp_riscv_app.h>
#include <ulp_riscv_lock.h>
#include <vector>

// some include in this file fucks the compiler so hard omg
#include <ble_peripheral_manager.h>

#define SCD_LOW_POWER false

extern const u8 ulp_riscv_bin_start[] asm("_binary_ulp_riscv_app_bin_start");
extern const u8 ulp_riscv_bin_end[] asm("_binary_ulp_riscv_app_bin_end");

constexpr u32 ULP_RISCV_WAKEUP_PERIOD_US = 60 * 1000 * 1000;

constexpr u32 ENV_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 ENV_READ_INTERVAL_MS = 10 * 1000;

constexpr u32 LSM_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 LSM_READ_INTERVAL_MS = 50;

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
RTC_DATA_ATTR bool did_initialize_ulp_riscv = false;

void environment_read_task(void* arg) {
  struct Sensors {
    Bme688 bme;
    Scd41 scd;
    Battery battery;
  };

  auto init = []() {
    Bme688 bme(g_i2c_handle);
    bme.set_temperature_oversampling(Bme688::Oversampling::Os8x);
    bme.set_humidity_oversampling(Bme688::Oversampling::Os2x);
    bme.set_pressure_oversampling(Bme688::Oversampling::Os4x);
    bme.set_iir_filter_size(Bme688::FilterSize::Size3);

    Scd41 scd(g_i2c_handle);
#if SCD_LOW_POWER
    scd.start_low_power_periodic_measurement();
#else
    scd.start_periodic_measurement();
#endif

    Battery bat(BATTERY_READ_PIN);

    ESP_LOGI("ENV", "Sensors initialized");
    return Sensors{
        .bme = bme,
        .scd = scd,
        .battery = bat,
    };
  };

  auto sensors = init();

  while (true) {
    {
      auto data = Data::the();
      auto bme_data = sensors.bme.read_sensor();
      auto scd_data = sensors.scd.read();

      if (bme_data) {
        // data->update_temperature(bme_data->temperature);
        data->update_humidity(bme_data->humidity);
        data->update_pressure(bme_data->pressure);
      } else {
        ESP_LOGW("BME688", "Failed reading sensor!");
      }

      if (scd_data) {
        data->update_temperature(scd_data->temperature);
        data->update_co2_ppm(scd_data->co2);

        ulp_last_co2_measurement = scd_data->co2;
        ulp_last_temperature_measurement =
            static_cast<u32>(scd_data->temperature * 1000.f);
      } else {
        ESP_LOGW("SCD41", "Failed reading sensor!");
      }

      data->update_battery_voltage(sensors.battery.read_voltage());
    }

    vTaskDelay(pdMS_TO_TICKS(ENV_READ_INTERVAL_MS));
  }
}

void lsm_read_task(void* arg) {
  auto init_lsm = []() {
    Lsm6dsox lsm(g_i2c_handle);
    return lsm;
  };

  auto lsm = init_lsm();

  while (true) {
    {
      auto data = Data::the();
      auto values = lsm.read_sensor();

      data->update_inertial_measurements(
          Vector3(values.acc_x, -values.acc_z, values.acc_y),
          Vector3(values.pitch, values.yaw, values.roll));
    }

    vTaskDelay(pdMS_TO_TICKS(LSM_READ_INTERVAL_MS));
  }
}

void update_system_time_from_rtc() {
  auto data = Data::the();
  auto rtc = g_rtc->read_date_time();
  tm rt = {
      .tm_sec = rtc.second,
      .tm_min = rtc.minute,
      .tm_hour = rtc.hour,
      .tm_mday = rtc.day,
      // tm_mon is in the range 0-11, whereas the RTC clock's month is in the
      // range 1-12
      .tm_mon = rtc.month - 1,
      // tm_year should be relative to 1900
      .tm_year = rtc.year - 1900,
  };

  ESP_LOGI("BM8563", "Updating system time from RTC");
  data->set_time(rt);
}

void set_rtc_time(struct tm utc_tm) {
  ESP_LOGI("BM8563", "Setting RTC time to %02d:%02d:%02d, %02d.%02d.%04d",
           utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec, utc_tm.tm_mday,
           utc_tm.tm_mon, utc_tm.tm_year);
  Bm8563::DateTime dt = {
      // tm_year is relative to 1900
      .year = utc_tm.tm_year + 1900,
      .month = utc_tm.tm_mon + 1,
      .weekday = utc_tm.tm_wday,
      .day = utc_tm.tm_mday,
      .hour = static_cast<int8_t>(utc_tm.tm_hour),
      .minute = static_cast<int8_t>(utc_tm.tm_min),
      .second = static_cast<int8_t>(utc_tm.tm_sec),
  };
  g_rtc->set_date_time(dt);
}

bool can_sleep() {
  return !ui::Ui::the().in_fullscreen() && !Data::bluetooth_enabled();
}

/// Saves states where necessary, sets up wakeup sources and enters deep sleep.
/// This function does not return!
void enter_deep_sleep() {
  // TODO: Do proper cleanup? We do a lot of heap allocations that are
  // never freed, but then RAM is reset on wakeup, so does it
  // matter?

  {
    auto d = Data::the();

    // save timer state
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

    auto lvgl_guard = d->lock_lvgl();
    set_display_backlight(false);
    clear_display();
    display_enter_sleep_mode();
  }

  ulp_wake_threshold_ppm = BAD_CO2_PPM_LEVEL;
  ulp_timer_resume();
  ESP_ERROR_CHECK(ulp_riscv_run());

  ESP_ERROR_CHECK(rtc_gpio_pullup_en(DP_TOUCH_INT));
  ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(DP_TOUCH_INT, 0));
  ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
  esp_deep_sleep_start();
}

void load_measurements_from_ulp() {
  if (!did_initialize_ulp_riscv)
    return;

  auto ulp_last_co2 = static_cast<u16>(ulp_last_co2_measurement);
  auto ulp_last_temp =
      static_cast<float>(static_cast<i16>(ulp_last_temperature_measurement)) /
      1000.f;
  Data::the()->update_co2_ppm(ulp_last_co2);
  Data::the()->update_temperature(ulp_last_temp);
}

void recover_from_sleep() {
  switch (esp_sleep_get_wakeup_cause()) {
  case ESP_SLEEP_WAKEUP_ULP: {
    // Scroll to the page showing CO2 PPM. This is very ugly; the 8 is a magic
    // number for the padding between the pages in the scroll view (idk if its
    // correct though).
    lv_obj_scroll_to_y(g_ui_container, lv_obj_get_height(lv_scr_act()) + 8,
                       LV_ANIM_ON);
    auto* scroll_container = lv_obj_get_child(g_ui_container, 1);
    lv_obj_scroll_to_x(scroll_container, 0, LV_ANIM_ON);
    break;
  }
  case ESP_SLEEP_WAKEUP_TIMER: {
    ESP_LOGI("Setup", "Woken up by timer");
    Data::the()->recover_timer(deep_sleep_timer.original_timer_duration, 0);
    break;
  }
  default:
    if (deep_sleep_timer.sleep_start == 0)
      break;

    auto now = time(NULL) * 1000;
    auto remaining_duration = deep_sleep_timer.remaining_timer_duration -
                              (now - deep_sleep_timer.sleep_start);
    Data::the()->recover_timer(deep_sleep_timer.original_timer_duration,
                               remaining_duration);
    break;
  }

  deep_sleep_timer = DeepSleepTimer{};
}

void initialize_ulp_riscv() {
  if (did_initialize_ulp_riscv)
    return;

  ESP_ERROR_CHECK(ulp_riscv_load_binary(
      ulp_riscv_bin_start, ulp_riscv_bin_end - ulp_riscv_bin_start));
  ESP_ERROR_CHECK(ulp_set_wakeup_period(0, ULP_RISCV_WAKEUP_PERIOD_US));
  did_initialize_ulp_riscv = true;
}

/// Taken from
/// https://github.com/espressif/esp-idf/blob/67c1de1eebe095d554d281952fde63c16ee2dca0/components/ulp/ulp_riscv/ulp_core/ulp_riscv_lock.c#L9
///
/// Adapted to reset the task watchdog to avoid it triggering while waiting for
/// the lock.
void ulp_riscv_lock_acquire_wdt_aware(ulp_riscv_lock_t* lock) {
  lock->critical_section_flag_ulp = true;
  lock->turn = ULP_RISCV_LOCK_TURN_MAIN_CPU;

  auto start = esp_timer_get_time();
  while (lock->critical_section_flag_main_cpu &&
         (lock->turn == ULP_RISCV_LOCK_TURN_MAIN_CPU)) {
    esp_task_wdt_reset();
  }

  ESP_LOGI("ULP", "Acquiring ULP RISC-V lock took %lldms",
           (esp_timer_get_time() - start) / 1000);
}

extern "C" void app_main() {
  esp_event_loop_create_default();

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

  load_measurements_from_ulp();

  ESP_LOGI("Setup", "Initialize display");
  init_display();

  recover_from_sleep();

  if (did_initialize_ulp_riscv) {
    // Ensure only the main CPU or the ULP COCPU are accessing the I2C bus and
    // wait if the ULP is currently using it. Also, since we're running now, we
    // can just stop the ULP. Since we have the lock, we can
    // safely tell it to halt without interfering with the running program.
    ESP_LOGI("Setup", "Waiting for ULP COCPU to release lock...");
    ulp_riscv_lock_acquire_wdt_aware((ulp_riscv_lock_t*)&ulp_lock);
    ulp_riscv_timer_stop();
    ulp_riscv_halt();
    ulp_riscv_lock_release((ulp_riscv_lock_t*)&ulp_lock);

    ESP_LOGI("Setup", "Stopped ULP COCPU");
  }

  init_display_touch(g_i2c_handle);

  g_rtc = new Bm8563(g_i2c_handle);
  update_system_time_from_rtc();

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::TimeChanged,
      [](void*, esp_event_base_t, int32_t, void*) {
        // don't block in event handler
        xTaskCreate(
            [](void*) {
              // NOTE: Make sure to store UTC time, as the time is expected to
              // be in UTC when read back in
              tm utc_time = Data::get_utc_time();
              set_rtc_time(utc_time);
              vTaskDelete(NULL);
            },
            "TCE to RTC", 4 * 2048, NULL, 1, NULL);
      },
      NULL);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::Inactivity,
      [](void*, esp_event_base_t, i32, void*) {
        // Don't block event handler. Probably doesn't actually matter here, but
        // better safe than sorry.
        xTaskCreate(
            [](void*) {
              if (!can_sleep())
                return;
              enter_deep_sleep();
              vTaskDelete(NULL);
            },
            "BEGIN DEEP SLP", 5 * 1024, NULL, BEGIN_DEEP_SLEEP_PRIORITY, NULL);
      },
      NULL);

  ESP_LOGI("Setup", "Starting sensor tasks");
  xTaskCreate(environment_read_task, "ENV SENS", ENV_TASK_STACK_SIZE, NULL,
              ENV_TASK_PRIORITY, NULL);
  xTaskCreate(lsm_read_task, "LSM6DSOX", LSM_TASK_STACK_SIZE, NULL,
              LSM_TASK_PRIORITY, NULL);

  initialize_ulp_riscv();

  ESP_LOGI("Setup", "Setup finished successfully!");
}
