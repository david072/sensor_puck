#include "data.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>

ESP_EVENT_DEFINE_BASE(DATA_EVENT_BASE);

template <typename T>
T low_pass_filter(T noisy_signal, T out, float gain = 0.1) {
  return out + (noisy_signal - out) * gain;
}

Mutex<Data>::Guard Data::the() {
  static Mutex<Data> data = Data();
  return data.lock();
}

void Data::start_timer(int duration) {
  if (!m_current_timer) {
    m_current_timer =
        xTimerCreate("User Timer", 1, false, NULL, [](TimerHandle_t timer) {
          xTimerDelete(timer, 10);
          esp_event_post(DATA_EVENT_BASE, Event::UserTimerExpired, NULL, 0, 10);

          // make sure to not block timer callback on Data mutex acquisition
          xTaskCreate(
              [](void*) {
                Data::the()->m_current_timer = NULL;
                vTaskDelete(NULL);
              },
              "tmr cleanup", 2048, NULL, 10, NULL);
        });
  }

  xTimerStop(m_current_timer, portMAX_DELAY);
  xTimerChangePeriod(m_current_timer, pdMS_TO_TICKS(duration), portMAX_DELAY);
  esp_event_post(DATA_EVENT_BASE, Event::UserTimerStarted, &duration,
                 sizeof(duration), portMAX_DELAY);
}

void Data::stop_timer() const {
  if (!m_current_timer)
    return;
  auto remaining_time = remaining_timer_duration_ms();
  xTimerChangePeriod(m_current_timer, pdMS_TO_TICKS(remaining_time),
                     portMAX_DELAY);
  xTimerStop(m_current_timer, portMAX_DELAY);
}

void Data::resume_timer() const {
  if (!m_current_timer)
    return;
  if (is_timer_running())
    return;
  xTimerStart(m_current_timer, portMAX_DELAY);
}

void Data::delete_timer() {
  if (!m_current_timer)
    return;
  xTimerDelete(m_current_timer, portMAX_DELAY);
  m_current_timer = NULL;
}

int Data::remaining_timer_duration_ms() const {
  if (!m_current_timer)
    return 0;

  auto remaining_ticks =
      xTimerGetExpiryTime(m_current_timer) - xTaskGetTickCount();
  return pdTICKS_TO_MS(remaining_ticks);
}

bool Data::is_timer_running() const {
  if (!m_current_timer)
    return false;
  return xTimerIsTimerActive(m_current_timer);
}

void Data::update_battery_percentage(uint32_t voltage) {
  // TODO: properly calculate percentage here
  m_battery_percentage =
      (voltage - 1800) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100;
}

void Data::update_gyroscope(Vector3 v) {
  m_gyroscope = low_pass_filter(v * RAD_TO_DEG, m_gyroscope, 0.5);
}

void Data::update_environment_measurements(float temp, float humidity,
                                           float pressure) {
  m_temperature = temp + TEMPERATURE_OFFSET;
  m_humidity = humidity + HUMIDITY_OFFSET;
  m_pressure = pressure;
}

tm Data::get_time() const {
  auto now = time(NULL);
  tm time;
  localtime_r(&now, &time);
  return time;
}

void Data::set_time(tm time) const {
  ESP_LOGI("Data", "Setting system time to %02d:%02d:%02d, %02d.%02d.%04d",
           time.tm_hour, time.tm_min, time.tm_sec, time.tm_mday, time.tm_mon,
           time.tm_year);

  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  timeval tv = {
      .tv_sec = mktime(&time),
      .tv_usec = 0,
  };
  settimeofday(&tv, NULL);
  esp_event_post(DATA_EVENT_BASE, Event::TimeChanged, NULL, 0, 0);
}
