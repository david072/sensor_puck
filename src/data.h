#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory>
#include <optional>

class Data;

template <typename T>
class Mutex {
public:
  class Guard {
  public:
    Guard(T* data, SemaphoreHandle_t mutex)
        : m_data(data),
          m_mutex(mutex) {}
    ~Guard() { xSemaphoreGive(m_mutex); }

    T* operator*() { return m_data; }
    T* operator->() { return m_data; }

  private:
    T* m_data;
    SemaphoreHandle_t const m_mutex;
  };

  Mutex()
      : m_data(nullptr),
        m_mutex(xSemaphoreCreateMutex()) {}

  Mutex(T data)
      : m_data(std::make_unique<T>(std::move(data))),
        m_mutex(xSemaphoreCreateMutex()) {}

  Guard lock(uint32_t ticks_to_wait = portMAX_DELAY) {
    xSemaphoreTake(m_mutex, ticks_to_wait);
    return Guard(m_data.get(), m_mutex);
  }

private:
  std::unique_ptr<T> m_data;
  SemaphoreHandle_t const m_mutex;
};

using Lock = Mutex<std::nullopt_t>;

class Data {
public:
  /// https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#measure-battery-voltage-pins
  /// [mV]
  static constexpr uint32_t MAX_BATTERY_VOLTAGE = 2150;
  /// [mV]
  static constexpr uint32_t MIN_BATTERY_VOLTAGE = 1800;

  static Mutex<Data>::Guard the();

  void update_battery_percentage(uint32_t voltage);

  uint8_t battery_percentage() const { return m_battery_percentage; }

private:
  uint8_t m_battery_percentage;
};
