#include "battery.h"
#include "display_driver.h"
#include <bm8563.h>
#include <data.h>
#include <driver/adc.h>
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <driver/rtc_io.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <lis2mdl.h>
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
constexpr u32 ENV_READ_INTERVAL_MS = 5 * 1000;

constexpr u32 LSM_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 LSM_READ_INTERVAL_MS = 50;

constexpr u32 BEEP_DURATION_MS = 100;
constexpr u32 BEEP_COUNT = 2;
constexpr i64 MEDIOCRE_CO2_PPM_LEVEL_BEEP_INTERVAL_MS = 5 * 60 * 1000; // 5min
constexpr i64 BAD_CO2_PPM_LEVEL_BEEP_INTERVAL_MS = 1 * 60 * 1000;      // 1min

i2c_master_bus_handle_t g_i2c_handle;
i2c_master_bus_handle_t g_lcd_i2c_handle;

Bm8563* g_rtc;

/// If the user timer duration is <= this value, we don't go to sleep and
/// instead wait for the timer to expire.
constexpr u32 MIN_TIMER_DURATION_FOR_DEEP_SLEEP_MS = 5000;

struct DeepSleepTimer {
  long sleep_start = 0;
  int original_timer_duration = 0;
  int remaining_timer_duration = 0;
};

RTC_DATA_ATTR DeepSleepTimer deep_sleep_timer = DeepSleepTimer{};
RTC_DATA_ATTR bool did_initialize_ulp_riscv = false;

void environment_read_task(void* arg) {
  DeepSleepPreparation deep_sleep;

  Scd41 scd(g_i2c_handle);
#if SCD_LOW_POWER
  scd.start_low_power_periodic_measurement();
#else
  scd.start_periodic_measurement();
#endif

  Battery battery(BATTERY_READ_PIN);

  ESP_LOGI("ENV", "Sensors initialized");

  while (true) {
    {
      auto data = Data::the();
      auto scd_data = scd.read();

      if (scd_data) {
        data->update_temperature(scd_data->temperature);
        data->update_humidity(scd_data->humidity);
        data->update_co2_ppm(scd_data->co2);

        ulp_last_co2_measurement = scd_data->co2;
        ulp_last_temperature_measurement = std::bit_cast<u32>(
            static_cast<i32>(scd_data->temperature * 1000.f));
        ulp_last_humidity_measurement =
            std::bit_cast<u32>(static_cast<i32>(scd_data->humidity * 1000.f));
      } else {
        ESP_LOGW("SCD41", "Failed reading sensor!");
      }

      data->update_battery_voltage(battery.read_voltage());
    }

    if (deep_sleep.wait_for_event(pdMS_TO_TICKS(ENV_READ_INTERVAL_MS))) {
      ESP_LOGI("ENV", "Powering down...");
      scd.power_down();
      // bme.power_down();

      deep_sleep.ready();
      break;
    }
  }

  vTaskDelete(NULL);
}

void lsm_read_task(void* arg) {
  DeepSleepPreparation deep_sleep;

  Lsm6dsox lsm(g_i2c_handle);
  Lis2mdl lis(g_i2c_handle);

  while (true) {
    {
      auto data = Data::the();
      auto accel_gyro = lsm.read_sensor();
      auto mag = lis.read_sensor();

      if (accel_gyro && mag) {
        auto mag_vec = Vector3(-mag->x, -mag->z, -mag->y);
        data->update_inertial_measurements(
            Vector3(-accel_gyro->acc_y, -accel_gyro->acc_z, -accel_gyro->acc_x),
            Vector3(accel_gyro->roll, accel_gyro->yaw, accel_gyro->pitch),
            mag_vec);

        // printf("dir: %.2f°\n", atan2f(mag_vec.z, mag_vec.x) * RAD_TO_DEG);
        // printf("(%.6f, %.6f, %.6f)\n", mag_vec.x, mag_vec.y, mag_vec.z);
      } else {
        if (!accel_gyro)
          ESP_LOGW("LSM", "Failed reading sensor!");
        if (!mag)
          ESP_LOGW("LIS", "Failed reading sensor!");
      }
    }

    if (deep_sleep.wait_for_event(pdMS_TO_TICKS(LSM_READ_INTERVAL_MS))) {
      ESP_LOGI("LSM", "Powering down...");
      lsm.set_accelerometer_data_rate(Lsm6dsox::DataRate::Off);
      lsm.set_gyroscope_data_rate(Lsm6dsox::DataRate::Off);
      lis.power_down();

      deep_sleep.ready();
      break;
    }
  }

  vTaskDelete(NULL);
}

void initialize_buzzer_ledc() {
  ledc_timer_config_t ledc_timer = {
      .speed_mode = BUZZER_LEDC_MODE,
      .duty_resolution = BUZZER_LEDC_DUTY_RES,
      .timer_num = BUZZER_LEDC_TIMER,
      .freq_hz = 2700,
      .clk_cfg = BUZZER_LEDC_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {
      .gpio_num = BUZZER_PIN,
      .speed_mode = BUZZER_LEDC_MODE,
      .channel = BUZZER_LEDC_CHANNEL,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = BUZZER_LEDC_TIMER,
      .duty = 0,
      .hpoint = 0,
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
  ESP_ERROR_CHECK(ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 1024));
  ESP_ERROR_CHECK(ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL));
  ESP_ERROR_CHECK(ledc_timer_pause(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER));
}

void buzzer_beep() {
  ESP_ERROR_CHECK(ledc_timer_rst(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER));
  ESP_ERROR_CHECK(ledc_timer_resume(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER));
  vTaskDelay(pdMS_TO_TICKS(BEEP_DURATION_MS));
  ESP_ERROR_CHECK(ledc_timer_pause(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER));
}

void buzzer_task(void* arg) {
  initialize_buzzer_ledc();

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::EnvironmentDataUpdated,
      [](void* task, char const*, auto, void*) {
        xTaskNotify(static_cast<TaskHandle_t>(task), 1, eSetValueWithOverwrite);
      },
      xTaskGetCurrentTaskHandle());

  i64 last_beep = 0;
  while (true) {
    ulTaskNotifyTake(true, portMAX_DELAY);
    auto co2 = Data::the()->co2_ppm();
    if (co2 < MEDIOCRE_CO2_PPM_LEVEL)
      continue;

    if (co2 >= MEDIOCRE_CO2_PPM_LEVEL && co2 < BAD_CO2_PPM_LEVEL) {
      if (last_beep > 0 && esp_timer_get_time() - last_beep <
                               MEDIOCRE_CO2_PPM_LEVEL_BEEP_INTERVAL_MS * 1000) {
        continue;
      }
    }

    if (co2 >= BAD_CO2_PPM_LEVEL) {
      if (last_beep > 0 && esp_timer_get_time() - last_beep <
                               BAD_CO2_PPM_LEVEL_BEEP_INTERVAL_MS * 1000) {
        continue;
      }
    }

    for (size_t i = 0; i < BEEP_COUNT; ++i) {
      buzzer_beep();
      vTaskDelay(pdMS_TO_TICKS(BEEP_DURATION_MS));
    }

    last_beep = esp_timer_get_time();
  }

  vTaskDelete(NULL);
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
  auto d = Data::the();

  if (d->user_timer().is_running() &&
      d->user_timer().remaining_duration_ms() <
          MIN_TIMER_DURATION_FOR_DEEP_SLEEP_MS) {
    return false;
  }

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
    if (d->user_timer().remaining_duration_ms() > 0) {
      deep_sleep_timer.sleep_start = time(NULL) * 1000;
      deep_sleep_timer.original_timer_duration =
          d->user_timer().original_duration_ms();
      deep_sleep_timer.remaining_timer_duration =
          d->user_timer().remaining_duration_ms();
      esp_sleep_enable_timer_wakeup(
          static_cast<uint64_t>(d->user_timer().remaining_duration_ms()) *
          1000);
    } else {
      deep_sleep_timer = DeepSleepTimer{};
    }

    set_display_backlight(false);
    clear_display();
    display_enter_sleep_mode();
  }

  esp_event_post(DATA_EVENT_BASE, Data::Event::PrepareDeepSleep, NULL, 0,
                 pdMS_TO_TICKS(100));

  // wait for deep sleep preparation to finish
  auto task_count = uxSemaphoreGetCount(s_prepare_deep_sleep_counter);
  ESP_LOGI("Sleep", "Waiting for %d tasks to prepare for deep sleep...",
           task_count);
  xEventGroupWaitBits(s_deep_sleep_ready_event_group, (1 << task_count) - 1,
                      true, true, pdMS_TO_TICKS(1000));

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
      static_cast<float>(std::bit_cast<i32>(ulp_last_temperature_measurement)) /
      1000.f;
  auto ulp_last_hum =
      static_cast<float>(std::bit_cast<i32>(ulp_last_humidity_measurement)) /
      1000.f;
  Data::the()->update_co2_ppm(ulp_last_co2);
  Data::the()->update_temperature(ulp_last_temp);
  Data::the()->update_humidity(ulp_last_hum);
}

void recover_from_sleep() {
  switch (esp_sleep_get_wakeup_cause()) {
  case ESP_SLEEP_WAKEUP_TIMER: {
    ESP_LOGI("Setup", "Woken up by timer");
    Data::the()->user_timer().recover(deep_sleep_timer.original_timer_duration,
                                      0);
    break;
  }
  default: {
    if (deep_sleep_timer.sleep_start == 0)
      break;

    auto now = time(NULL) * 1000;
    auto remaining_duration = deep_sleep_timer.remaining_timer_duration -
                              (now - deep_sleep_timer.sleep_start);
    Data::the()->user_timer().recover(deep_sleep_timer.original_timer_duration,
                                      remaining_duration);
    break;
  }
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
  // https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/06-Event-groups#:~:text=The%20number%20of%20bits%20(or%20flags)%20stored%20within%20an%20event%20group%20is%208%20if%20configUSE_16_BIT_TICKS%20is%20set%20to%201%2C%20or%2024%20if%20configUSE_16_BIT_TICKS%20is%20set%20to%200.
  auto ds_counter_max = configUSE_16_BIT_TICKS ? 8 : 24;
  s_prepare_deep_sleep_counter = xSemaphoreCreateCounting(ds_counter_max, 0);
  s_deep_sleep_ready_event_group = xEventGroupCreate();

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

  ESP_LOGI("Setup", "Initialize LCD I2C master bus");
  gpio_reset_pin(B_LCD_SDA);
  gpio_reset_pin(B_LCD_SCL);
  i2c_master_bus_config_t lcd_i2c_config = {
      .i2c_port = 1,
      .sda_io_num = B_LCD_SDA,
      .scl_io_num = B_LCD_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags =
          {
              .enable_internal_pullup = true,
          },
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&lcd_i2c_config, &g_lcd_i2c_handle));

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

  init_display_touch(g_lcd_i2c_handle);

  g_rtc = new Bm8563(g_lcd_i2c_handle);
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
              if (can_sleep()) {
                enter_deep_sleep();
              }
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
  xTaskCreate(buzzer_task, "BUZZER", 2048, NULL, MISC_TASK_PRIORITY, NULL);

  initialize_ulp_riscv();

  ESP_LOGI("Setup", "Setup finished successfully!");
}
