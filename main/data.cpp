#include "data.h"
#include <cmath>
#include <esp_log.h>
#include <preferences.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ble_peripheral_manager.h>
#include <wifi_manager.h>

void UserTimer::recover(int original_duration, int remaining_duration) {
  if (remaining_duration == 0) {
    if (m_timer) {
      xTimerDelete(m_timer, portMAX_DELAY);
      m_timer = NULL;
    }

    esp_event_post(DATA_EVENT_BASE, Data::Event::UserTimerExpired, NULL, 0,
                   portMAX_DELAY);
    return;
  }

  start(original_duration);
  xTimerChangePeriod(m_timer, pdMS_TO_TICKS(remaining_duration), portMAX_DELAY);
}

void UserTimer::start(int duration) {
  if (!m_timer) {
    m_timer =
        xTimerCreate("User Timer", 1, false, NULL, [](TimerHandle_t timer) {
          esp_event_post(DATA_EVENT_BASE, Data::Event::UserTimerExpired, NULL,
                         0, 10);

          // make sure to not block timer callback on Data mutex acquisition
          xTaskCreate(
              [](void*) {
                Data::the()->user_timer().reset();
                vTaskDelete(NULL);
              },
              "tmr cleanup", 2048, NULL, 10, NULL);
        });
  }

  xTimerStop(m_timer, portMAX_DELAY);
  xTimerChangePeriod(m_timer, pdMS_TO_TICKS(duration), portMAX_DELAY);
  esp_event_post(DATA_EVENT_BASE, Data::Event::UserTimerStarted, &duration,
                 sizeof(duration), portMAX_DELAY);

  m_original_duration = duration;
}

void UserTimer::resume() const {
  if (!m_timer)
    return;
  if (is_running())
    return;
  xTimerStart(m_timer, portMAX_DELAY);
}

void UserTimer::stop() const {
  if (!m_timer)
    return;
  auto remaining_time = remaining_duration_ms();
  xTimerChangePeriod(m_timer, pdMS_TO_TICKS(remaining_time), portMAX_DELAY);
  xTimerStop(m_timer, portMAX_DELAY);
}

void UserTimer::reset() {
  if (!m_timer)
    return;
  xTimerDelete(m_timer, portMAX_DELAY);
  m_timer = NULL;
}

int UserTimer::remaining_duration_ms() const {
  if (!m_timer)
    return 0;

  auto remaining_ticks = xTimerGetExpiryTime(m_timer) - xTaskGetTickCount();
  return pdTICKS_TO_MS(remaining_ticks);
}

bool UserTimer::is_running() const {
  if (!m_timer)
    return false;
  return xTimerIsTimerActive(m_timer);
}

void initialize_nvs_flash() {
  ESP_LOGI("Data", "Initializing NVS Flash");
  auto ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW("Data", "NVS initialization failed, erasing and retrying...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_LOGI("Data", "NVS successfully initialized");
}

ESP_EVENT_DEFINE_BASE(DATA_EVENT_BASE);

template <typename T>
T low_pass_filter(T noisy_signal, T out, float gain = 0.1) {
  return out + (noisy_signal - out) * gain;
}

Mutex<Data>::Guard Data::the() {
  static Mutex<Data> data = Data();
  return data.lock();
}

void Data::initialize() {
  m_muted =
      Preferences::instance().get_bool(PREFERENCES_IS_MUTED).value_or(false);

  xTaskCreate(
      [](void*) {
        {
          auto last_history_entry = Data::the()->last_history_entry();
          if (last_history_entry) {
            auto elapsed = time(NULL) - last_history_entry->timestamp;
            auto time_until_next_history_entry =
                TIME_BETWEEN_HISTORY_ENTRIES_S -
                (elapsed % TIME_BETWEEN_HISTORY_ENTRIES_S);
            vTaskDelay(pdMS_TO_TICKS(time_until_next_history_entry * 1000));
          } else {
            vTaskDelay(pdMS_TO_TICKS(TIME_BETWEEN_HISTORY_ENTRIES_S * 1000));
          }
        }

        while (true) {
          {
            Data::the()->update_history();
          }
          vTaskDelay(pdMS_TO_TICKS(TIME_BETWEEN_HISTORY_ENTRIES_S * 1000));
        }

        vTaskDelete(NULL);
      },
      "HIST UPDATE", 1024 * 3, NULL, ENV_TASK_PRIORITY, NULL);
}

void Data::enable_bluetooth() {
  BlePeripheralManager::the().start(BLUETOOTH_ADVERSISEMENT_DURATION_MS);
}
void Data::disable_bluetooth() { BlePeripheralManager::the().stop(); }
bool Data::bluetooth_enabled() { return BlePeripheralManager::the().started(); }

void Data::enable_wifi() { WifiManager::the().enable_wifi(); }
void Data::disable_wifi() { WifiManager::the().disable_wifi(); }
bool Data::wifi_enabled() { return WifiManager::the().wifi_enabled(); }

void post_environment_data_updated_event() {
  esp_event_post(DATA_EVENT_BASE, Data::Event::EnvironmentDataUpdated, NULL, 0,
                 10);
}

void Data::update_battery_voltage(uint32_t voltage) {
  m_battery_percentage =
      round((voltage - MIN_BATTERY_VOLTAGE) /
            (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100.f);
  m_battery_percentage = MIN(m_battery_percentage, (uint8_t)100);
  esp_event_post(DATA_EVENT_BASE, Event::BatteryChargeUpdated, NULL, 0, 10);
}

void Data::update_inertial_measurements(Vector3 accel, Vector3 gyro,
                                        Vector3 mag) {
  m_gyroscope = low_pass_filter(gyro, m_gyroscope, 0.5);
  m_acceleration = low_pass_filter(accel, m_acceleration, 0.8);
  m_magnetic = low_pass_filter(mag + HARD_IRON_OFFSET, m_magnetic, 0.25);

  m_compass_heading =
      atan2f(m_magnetic.x, m_magnetic.z) * RAD_TO_DEG + COMPASS_HEADING_OFFSET;
  // transform range from [-180° ; 180°] to [0° ; 360°]
  if (m_compass_heading < 0)
    m_compass_heading += 360.f;

  m_compass_heading = 360.f - m_compass_heading;

  if (set_down_gesture_detected()) {
    esp_event_post(DATA_EVENT_BASE, Event::SetDownGesture, NULL, 0, 10);
  }
}

void Data::set_muted(bool muted) {
  m_muted = muted;
  Preferences::instance().set_bool(PREFERENCES_IS_MUTED, muted);
  esp_event_post(DATA_EVENT_BASE, Event::StatusUpdated, NULL, 0, 10);
}

bool Data::set_down_gesture_detected() {
  if (m_acceleration.y >= SDG_COOLDOWN_ACCELERATION_THRESHOLD &&
      sdg_cooldown_exceeded()) {
    m_sdg_cooldown = millis();
  }

  if (-m_acceleration.y >= SDG_DOWNWARDS_ACCELERATION_THRESHOLD &&
      sdg_cooldown_exceeded()) {
    if (millis() - m_set_down_gesture_start >
        SDG_DOWNWARDS_ACCELERATION_MAX_DURATION_MS) {
      m_set_down_gesture_start = millis();
    }
  }

  auto elapsed = millis() - m_set_down_gesture_start;

  if (elapsed <= SDG_DOWNWARDS_ACCELERATION_MIN_DURATION_MS &&
      abs(m_acceleration.y) <= SDG_END_ACCELERATION) {
    m_set_down_gesture_start = 0;
    elapsed = millis() - m_set_down_gesture_start;
  }

  if (elapsed <= SDG_DOWNWARDS_ACCELERATION_MAX_DURATION_MS) {
    if (abs(m_acceleration.y) <= SDG_END_ACCELERATION) {
      ESP_LOGI("Data", "Detected SDG after %ld ms", elapsed);
      m_set_down_gesture_start = 0;
      return true;
    }
  }

  return false;
}

void Data::update_temperature(float temp) {
  m_temperature = temp + TEMPERATURE_OFFSET;
  post_environment_data_updated_event();
}

void Data::update_humidity(float hum) {
  m_humidity = hum + HUMIDITY_OFFSET;
  post_environment_data_updated_event();
}

void Data::update_co2_ppm(u16 co2_ppm) {
  m_co2_ppm = co2_ppm;
  post_environment_data_updated_event();
}

void Data::update_history() {
  auto save_history_entry = [this](FILE* file = NULL) {
    if (file == NULL) {
      file = fopen(HISTORY_FILE_PATH, "ab");
    }

    RawHistoryEntry e = {
        .timestamp = time(NULL),
        .co2_ppm = m_co2_ppm,
        .temp = static_cast<i16>(round(m_temperature * 100.f)),
        .hum = static_cast<i16>(round(m_humidity * 100.f)),
    };
    fwrite(&e, sizeof(RawHistoryEntry), 1, file);
    fclose(file);
  };

  auto* file = fopen(HISTORY_FILE_PATH, "rb");

  if (file == NULL) {
    save_history_entry();
    return;
  }

  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  ESP_LOGI("Data", "History file size: %ld", size);
  RawHistoryEntry last_entry = {.timestamp = 0};
  if (size > 0) {
    fseek(file, size - sizeof(RawHistoryEntry), SEEK_SET);
    fread(&last_entry, sizeof(RawHistoryEntry), 1, file);
  }

  if (time(NULL) - last_entry.timestamp >= TIME_BETWEEN_HISTORY_ENTRIES_S) {
    if (size / sizeof(RawHistoryEntry) >= MAX_HISTORY_ENTRIES) {
      // remove first element from file
      RawHistoryEntry buf[MAX_HISTORY_ENTRIES - 1];
      fseek(file, sizeof(RawHistoryEntry), SEEK_SET);
      auto read_entries = fread(buf, sizeof(RawHistoryEntry),
                                sizeof(buf) / sizeof(RawHistoryEntry), file);

      fclose(file);
      file = fopen(HISTORY_FILE_PATH, "wb");
      if (file == NULL) {
        perror("fopen");
        return;
      }
      fwrite(buf, sizeof(RawHistoryEntry), read_entries, file);
      save_history_entry(file);
    } else {
      fclose(file);
      save_history_entry();
    }
  } else {
    fclose(file);
  }
}

std::optional<Data::RawHistoryEntry> Data::last_history_entry() const {
  auto* file = fopen(HISTORY_FILE_PATH, "rb");
  if (file == NULL)
    return std::nullopt;

  fseek(file, -sizeof(RawHistoryEntry), SEEK_END);
  RawHistoryEntry e;
  fread(&e, sizeof(RawHistoryEntry), 1, file);
  fclose(file);
  return e;
}

std::vector<Data::HistoryEntry> Data::history() const {
  auto* file = fopen(HISTORY_FILE_PATH, "rb");
  if (!file)
    return {};

  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  fseek(file, 0, SEEK_SET);

  auto entry_count = size / sizeof(RawHistoryEntry);
  std::vector<HistoryEntry> result;
  result.resize(entry_count);

  RawHistoryEntry raw_entries[entry_count];
  fread(raw_entries, sizeof(RawHistoryEntry), entry_count, file);
  fclose(file);

  for (size_t i = 0; i < entry_count; ++i) {
    result[i] = {
        .timestamp = raw_entries[i].timestamp,
        .co2_ppm = raw_entries[i].co2_ppm,
        .temp = static_cast<float>(raw_entries[i].temp) / 100.f,
        .hum = static_cast<float>(raw_entries[i].hum) / 100.f,
    };
  }

  return result;
}

tm Data::get_time() {
  auto now = time(NULL);
  tm time;
  localtime_r(&now, &time);
  return time;
}

tm Data::get_utc_time() {
  auto now = time(NULL);
  return *gmtime(&now);
}

void Data::set_time(tm time) {
  ESP_LOGI("Data", "Setting system time to %02d:%02d:%02d, %02d.%02d.%04d",
           time.tm_hour, time.tm_min, time.tm_sec, time.tm_mday, time.tm_mon,
           time.tm_year);
  set_time(mktime(&time));
}

void Data::set_time(time_t unix_timestamp) {
  ESP_LOGI("Data", "Setting system time to Unix timestamp %lld",
           unix_timestamp);
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  timeval tv = {
      .tv_sec = unix_timestamp,
      .tv_usec = 0,
  };
  settimeofday(&tv, NULL);
  esp_event_post(DATA_EVENT_BASE, Event::TimeChanged, NULL, 0, 0);
}

bool Data::is_upside_down() const {
  return abs(m_acceleration.y - GRAVITATIONAL_ACCELERATION.y) < 2;
}
