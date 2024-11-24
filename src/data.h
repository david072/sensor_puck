#pragma once

#include <cstdint>

#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
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

struct Vector3 {
  Vector3(float x, float y, float z)
      : x(x),
        y(y),
        z(z) {}

  Vector3(Vector3 const& other)
      : Vector3(other.x, other.y, other.z) {}

  Vector3(Vector3& other)
      : Vector3(other.x, other.y, other.z) {}

  Vector3(Vector3&& other)
      : Vector3(other.x, other.y, other.z) {}

  Vector3(float v)
      : Vector3(v, v, v) {}

  Vector3()
      : Vector3(0) {}

  Vector3 operator=(Vector3 const& other) {
    this->x = other.x;
    this->y = other.y;
    this->z = other.z;
    return *this;
  }

  Vector3 operator+(Vector3 const& other) {
    return Vector3(x + other.x, y + other.y, z + other.z);
  }
  Vector3 operator+=(Vector3 const& other) { return *this + other; }

  Vector3 operator-(Vector3 const& other) {
    return Vector3(x - other.x, y - other.y, z - other.z);
  }
  Vector3 operator-=(Vector3 const& other) { return *this - other; }

  Vector3 operator*(float s) { return Vector3(x * s, y * s, z * s); }
  Vector3 operator*=(float s) { return *this * s; }

  Vector3 operator/(float s) { return Vector3(x / s, y / s, z / s); }
  Vector3 operator/=(float s) { return *this / s; }

  float x;
  float y;
  float z;
};

ESP_EVENT_DECLARE_BASE(DATA_EVENT_BASE);

class Data {
public:
  enum Event : int32_t {
    TimeChanged,
    UserTimerStarted,
    UserTimerExpired,
  };

  /// https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#measure-battery-voltage-pins
  /// [mV]
  static constexpr uint32_t MAX_BATTERY_VOLTAGE = 2150;
  /// [mV]
  static constexpr uint32_t MIN_BATTERY_VOLTAGE = 1800;

  /// Temperature correction offset in °C
  static constexpr float TEMPERATURE_OFFSET = -11;
  /// Humidity correction offset in %
  static constexpr float HUMIDITY_OFFSET = 12;

  static Mutex<Data>::Guard the();

  void update_battery_percentage(uint32_t voltage);
  void update_gyroscope(Vector3 v);
  void update_environment_measurements(float temp, float humidity,
                                       float pressure);

  tm get_time() const;
  void set_time(tm time) const;

  Lock::Guard lock_i2c() { return m_i2c_lock.lock(); }

  void start_timer(int duration);
  void stop_timer() const;
  void resume_timer() const;
  void delete_timer();
  int remaining_timer_duration_ms() const;
  bool is_timer_running() const;
  bool timer_exists() const { return m_current_timer != NULL; }

  uint8_t battery_percentage() const { return m_battery_percentage; }

  /// °/s
  Vector3 const& gyroscope() const { return m_gyroscope; }

  float temperature() const { return m_temperature; }

private:
  Lock m_i2c_lock;

  TimerHandle_t m_current_timer{};

  uint8_t m_battery_percentage;

  /// °/s
  Vector3 m_gyroscope;

  /// °C
  float m_temperature;
  /// %
  float m_humidity;
  /// hPa
  float m_pressure;
};
