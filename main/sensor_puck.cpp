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
#include <esp_vfs.h>
#include <esp_vfs_fat.h>
#include <lis2mdl.h>
#include <lsm6dsox.h>
#include <lvgl.h>
#include <mbedtls/base64.h>
#include <scd41.h>
#include <st25dv.h>
#include <stdio.h>
#include <ui/pages.h>
#include <ui/ui.h>
#include <vector>

// some include in this file fucks the compiler so hard omg
#include <ble_peripheral_manager.h>

#define SCD_LOW_POWER false

constexpr u64 WAKEUP_PERIOD_MS = 60 * 1000;

constexpr u32 ENV_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 ENV_READ_INTERVAL_MS = 5 * 1000;

constexpr u32 LSM_TASK_STACK_SIZE = 5 * 1024;
constexpr u32 LSM_READ_INTERVAL_MS = 50;

constexpr u32 BEEP_DURATION_MS = 100;
constexpr u32 BEEP_COUNT = 2;
constexpr i64 MEDIOCRE_CO2_PPM_LEVEL_BEEP_INTERVAL_MS = 5 * 60 * 1000; // 5min
constexpr i64 BAD_CO2_PPM_LEVEL_BEEP_INTERVAL_MS = 1 * 60 * 1000;      // 1min

constexpr u32 NFC_DATA_UPDATE_INTERVAL_MS = 10 * 1000;
constexpr char const* NFC_URL_ADDRESS = "sensor-puck.web.app?d=";

i2c_master_bus_handle_t g_i2c_handle;
i2c_master_bus_handle_t g_lcd_i2c_handle;

/// Handle to the wear leveling library.
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

St25dv16kc* g_nfc;
Bm8563* g_rtc;
Scd41* g_scd;

/// If the user timer duration is <= this value, we don't go to sleep and
/// instead wait for the timer to expire.
constexpr u32 MIN_TIMER_DURATION_FOR_DEEP_SLEEP_MS = 5000;

struct DeepSleepTimer {
  long sleep_start = 0;
  int original_timer_duration = 0;
  int remaining_timer_duration = 0;
};

RTC_DATA_ATTR DeepSleepTimer deep_sleep_timer = DeepSleepTimer{};

struct EnvironmentData {
  bool has_values = false;
  u16 co2;
  float temperature;
  float humidity;
};

RTC_DATA_ATTR EnvironmentData rtc_env_data = {};
RTC_DATA_ATTR bool rtc_check_env_only = false;

template <typename T>
void num_to_bytes(u8* buf, size_t& idx, T num, i64 width) {
  for (i64 i = width - 8; i >= 0; i -= 8) {
    buf[idx++] = (num >> i) & 0xFF;
  }
}

void update_nfc_data() {
  auto d = Data::the();

  auto timestamp = static_cast<i64>(time(NULL));
  auto co2 = d->co2_ppm();
  auto temp = static_cast<i16>(round(d->temperature() * 100.f));
  auto hum = static_cast<i16>(round(d->humidity() * 100.f));

  auto history = d->history();

  // type + current value + history
  auto property_length = 1 + 2 + history.size() * 2;

  // timestamp + history length + history time offset (if possible) + 3
  // properties (co2, temperature, humidity)
  u8 nfc_buf[8 + 1 + (history.empty() ? 0 : 1) + property_length * 3];
  size_t i = 0;
  // timestamp
  num_to_bytes(nfc_buf, i, timestamp, 64);

  // history length and history time offset
  nfc_buf[i++] = static_cast<u8>(history.size());
  if (!history.empty()) {
    auto history_offset_min = std::min(
        (timestamp - history[history.size() - 1].timestamp) / 60, (i64)0xFF);
    nfc_buf[i++] = static_cast<u8>(history_offset_min);
  }

  // CO2 property
  nfc_buf[i++] = 0x00;
  num_to_bytes(nfc_buf, i, co2, 16);
  for (auto const& e : history) {
    num_to_bytes(nfc_buf, i, e.co2_ppm, 16);
  }

  // temperature property
  nfc_buf[i++] = 0x01;
  num_to_bytes(nfc_buf, i, temp, 16);
  for (auto const& e : history) {
    num_to_bytes(nfc_buf, i, static_cast<i16>(round(e.temp * 100.f)), 16);
  }

  // humidity property
  nfc_buf[i++] = 0x02;
  num_to_bytes(nfc_buf, i, hum, 16);
  for (auto const& e : history) {
    num_to_bytes(nfc_buf, i, static_cast<i16>(round(e.hum * 100.f)), 16);
  }

  u8 b64_buf[512];
  size_t b64_buf_len;
  mbedtls_base64_encode(b64_buf, sizeof(b64_buf), &b64_buf_len, nfc_buf,
                        sizeof(nfc_buf));

  // replace characters that are not URL safe
  for (size_t i = 0; i < b64_buf_len; ++i) {
    if (b64_buf[i] == '+') {
      b64_buf[i] = '-';
    } else if (b64_buf[i] == '/') {
      b64_buf[i] = '_';
    }
  }

  std::string uri = NFC_URL_ADDRESS;
  uri.append((char*)b64_buf, b64_buf_len);

  auto record = nfc::build_ndef_uri_record(nfc::UriPrefix::Https, uri);
  ESP_LOGI("NFC", "Writing record of length %d", record.length);
  g_nfc->write_ndef_record(record);
  nfc::free_ndef_record(record);
}

void update_nfc_data_if_necessary() {
  static i64 last_update = 0;
  if (esp_timer_get_time() - last_update < NFC_DATA_UPDATE_INTERVAL_MS * 1000)
    return;
  last_update = esp_timer_get_time();
  update_nfc_data();
}

bool read_environment_sensors() {
  auto data = Data::the();
  auto scd_data = g_scd->read();

  if (scd_data) {
    data->update_temperature(scd_data->temperature);
    data->update_humidity(scd_data->humidity);
    data->update_co2_ppm(scd_data->co2);

    rtc_env_data.has_values = true;
    rtc_env_data.co2 = scd_data->co2;
    rtc_env_data.temperature = scd_data->temperature;
    rtc_env_data.humidity = scd_data->humidity;
    return true;
  } else {
    ESP_LOGW("SCD41", "Failed reading sensor!");
    return false;
  }
}

void environment_read_task(void* arg) {
  DeepSleepPreparation deep_sleep;

  Battery battery(BATTERY_READ_PIN);

  ESP_LOGI("ENV", "Sensors initialized");

  while (true) {
    {
      read_environment_sensors();
      Data::the()->update_battery_voltage(battery.read_voltage());
    }

    update_nfc_data_if_necessary();

    if (deep_sleep.wait_for_event(pdMS_TO_TICKS(ENV_READ_INTERVAL_MS))) {
      ESP_LOGI("ENV", "Powering down...");
      g_scd->power_down();
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
            Vector3(-accel_gyro->acc_y, accel_gyro->acc_z, -accel_gyro->acc_x),
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

void buzzer_beep(u32 beep_duration_ms, u32 pause_duration_ms, size_t count) {
  { Data::the()->disable_sdg_detection(); }
  for (size_t i = 0; i < count; ++i) {
    ESP_ERROR_CHECK(ledc_timer_rst(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER));
    ESP_ERROR_CHECK(ledc_timer_resume(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER));
    vTaskDelay(pdMS_TO_TICKS(beep_duration_ms));
    ESP_ERROR_CHECK(ledc_timer_pause(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER));
    vTaskDelay(pdMS_TO_TICKS(pause_duration_ms));
  }
  { Data::the()->enable_sdg_detection(); }
}

void buzzer_task(void* arg) {
  initialize_buzzer_ledc();

  constexpr u32 ENVIRONMENT_DATA_UPDATED_BIT = 0x01;
  constexpr u32 USER_TIMER_EXPIRED_BIT = 0x02;

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::EnvironmentDataUpdated,
      [](void* task, char const*, auto, void*) {
        xTaskNotify(static_cast<TaskHandle_t>(task),
                    ENVIRONMENT_DATA_UPDATED_BIT, eSetBits);
      },
      xTaskGetCurrentTaskHandle());

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::UserTimerExpired,
      [](void* task, char const*, auto, void*) {
        xTaskNotify(static_cast<TaskHandle_t>(task), USER_TIMER_EXPIRED_BIT,
                    eSetBits);
      },
      xTaskGetCurrentTaskHandle());

  i64 last_co2_beep = 0;
  while (true) {
    u32 notified_value;
    xTaskNotifyWait(false, ULONG_MAX, &notified_value, portMAX_DELAY);

    if (Data::the()->muted() || Data::the()->is_upside_down()) {
      last_co2_beep = esp_timer_get_time();
      continue;
    }

    if ((notified_value & ENVIRONMENT_DATA_UPDATED_BIT) != 0) {
      auto co2 = Data::the()->co2_ppm();

      if (co2 < MEDIOCRE_CO2_PPM_LEVEL)
        goto env_data_updated_end;

      if (co2 >= MEDIOCRE_CO2_PPM_LEVEL && co2 < BAD_CO2_PPM_LEVEL) {
        if (last_co2_beep > 0 &&
            esp_timer_get_time() - last_co2_beep <
                MEDIOCRE_CO2_PPM_LEVEL_BEEP_INTERVAL_MS * 1000) {
          goto env_data_updated_end;
        }
      }

      if (co2 >= BAD_CO2_PPM_LEVEL) {
        if (last_co2_beep > 0 &&
            esp_timer_get_time() - last_co2_beep <
                BAD_CO2_PPM_LEVEL_BEEP_INTERVAL_MS * 1000) {
          goto env_data_updated_end;
        }
      }

      buzzer_beep(BEEP_DURATION_MS, BEEP_DURATION_MS, BEEP_COUNT);

      last_co2_beep = esp_timer_get_time();
    env_data_updated_end:
    }

    if ((notified_value & USER_TIMER_EXPIRED_BIT) != 0) {
      buzzer_beep(ui::TimerPage::BLINK_TIMER_PERIOD_MS,
                  ui::TimerPage::BLINK_TIMER_PERIOD_MS,
                  ui::TimerPage::BLINK_TIMER_REPEAT_COUNT);
    }
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
void enter_deep_sleep(bool sparse = false) {
  // TODO: Do proper cleanup? We do a lot of heap allocations that are
  // never freed, but then RAM is reset on wakeup, so does it
  // matter?

  if (!sparse) {
    esp_event_post(DATA_EVENT_BASE, Data::Event::PrepareDeepSleep, NULL, 0,
                   pdMS_TO_TICKS(100));

    // wait for deep sleep preparation to finish
    auto task_count = uxSemaphoreGetCount(s_prepare_deep_sleep_counter);
    ESP_LOGI("Sleep", "Waiting for %d tasks to prepare for deep sleep...",
             task_count);
    xEventGroupWaitBits(s_deep_sleep_ready_event_group, (1 << task_count) - 1,
                        true, true, pdMS_TO_TICKS(1000));
  }

  esp_sleep_enable_timer_wakeup(WAKEUP_PERIOD_MS * 1000);
  rtc_check_env_only = true;

  {
    auto d = Data::the();

    // save timer state
    if (d->user_timer().remaining_duration_ms() > 0) {
      deep_sleep_timer.sleep_start = time(NULL) * 1000;
      deep_sleep_timer.original_timer_duration =
          d->user_timer().original_duration_ms();
      deep_sleep_timer.remaining_timer_duration =
          d->user_timer().remaining_duration_ms();

      if (d->user_timer().remaining_duration_ms() <= WAKEUP_PERIOD_MS) {
        rtc_check_env_only = false;
        esp_sleep_enable_timer_wakeup(
            static_cast<uint64_t>(d->user_timer().remaining_duration_ms()) *
            1000);
      }
    } else {
      deep_sleep_timer = DeepSleepTimer{};
    }

    if (!sparse) {
      set_display_backlight(false);
      clear_display();
      display_enter_sleep_mode();
    }
  }

  ESP_ERROR_CHECK(rtc_gpio_pullup_en(DP_TOUCH_INT));
  ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(DP_TOUCH_INT, 0));
  esp_deep_sleep_start();
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

void pull_rtc_environment_data() {
  if (!rtc_env_data.has_values)
    return;
  auto d = Data::the();
  d->update_co2_ppm(rtc_env_data.co2);
  d->update_temperature(rtc_env_data.temperature);
  d->update_humidity(rtc_env_data.humidity);
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

  initialize_nvs_flash();

  ESP_LOGI("Setup", "Mounting FAT filesystem");
  esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = true,
      .max_files = 4,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
      .use_one_fat = false,
  };
  ESP_ERROR_CHECK(esp_vfs_fat_spiflash_mount_rw_wl(
      BASE_PATH, "storage", &mount_config, &s_wl_handle));
  ESP_LOGI("Setup", "Filesystem mounted!");

  Data::the()->initialize();

  g_rtc = new Bm8563(g_lcd_i2c_handle);
  update_system_time_from_rtc();

  g_nfc = new St25dv16kc(g_i2c_handle);

  if (rtc_check_env_only &&
      esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    ESP_LOGI("Setup", "Checking env while staying silent...");

    g_scd = new Scd41(g_i2c_handle);
#if SCD_LOW_POWER
    scd.start_low_power_periodic_measurement();
#else
    g_scd->start_periodic_measurement();
#endif

    recover_from_sleep();
    while (!read_environment_sensors())
      vTaskDelay(pdMS_TO_TICKS(5500));

    update_nfc_data();

    g_scd->power_down();
    enter_deep_sleep(true);
    return;
  }

  xTaskCreate(buzzer_task, "Buzzer", 2048, NULL, MISC_TASK_PRIORITY, NULL);

  ESP_LOGI("Setup", "Initialize display");
  init_display();

  pull_rtc_environment_data();
  recover_from_sleep();

  init_display_touch(g_lcd_i2c_handle);

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
            "Update RTC", 4 * 2048, NULL, 1, NULL);
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

  g_scd = new Scd41(g_i2c_handle);
#if SCD_LOW_POWER
  scd.start_low_power_periodic_measurement();
#else
  g_scd->start_periodic_measurement();
#endif

  ESP_LOGI("Setup", "Starting sensor tasks");
  xTaskCreate(environment_read_task, "ENV SENS", ENV_TASK_STACK_SIZE, NULL,
              ENV_TASK_PRIORITY, NULL);
  xTaskCreate(lsm_read_task, "LSM6DSOX", LSM_TASK_STACK_SIZE, NULL,
              LSM_TASK_PRIORITY, NULL);

  ESP_LOGI("Setup", "Wakeup cause: %d", esp_sleep_get_wakeup_cause());

  ESP_LOGI("Setup", "Setup finished successfully!");
}
