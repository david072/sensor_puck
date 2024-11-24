#pragma once

#include <cstdint>

#include <Arduino.h>
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
  constexpr Vector3(float x, float y, float z)
      : x(x),
        y(y),
        z(z) {}

  constexpr Vector3(Vector3 const& other)
      : Vector3(other.x, other.y, other.z) {}

  constexpr Vector3(Vector3& other)
      : Vector3(other.x, other.y, other.z) {}

  constexpr Vector3(Vector3&& other)
      : Vector3(other.x, other.y, other.z) {}

  constexpr Vector3(float v)
      : Vector3(v, v, v) {}

  constexpr Vector3()
      : Vector3(0) {}

  constexpr Vector3 operator=(Vector3 const& other) {
    this->x = other.x;
    this->y = other.y;
    this->z = other.z;
    return *this;
  }

  constexpr Vector3 operator+(Vector3 const& other) {
    return Vector3(x + other.x, y + other.y, z + other.z);
  }
  constexpr Vector3 operator+=(Vector3 const& other) { return *this + other; }

  constexpr Vector3 operator-(Vector3 const& other) {
    return Vector3(x - other.x, y - other.y, z - other.z);
  }
  constexpr Vector3 operator-=(Vector3 const& other) { return *this - other; }

  constexpr Vector3 operator*(float s) { return Vector3(x * s, y * s, z * s); }
  constexpr Vector3 operator*=(float s) { return *this * s; }

  constexpr Vector3 operator/(float s) { return Vector3(x / s, y / s, z / s); }
  constexpr Vector3 operator/=(float s) { return *this / s; }

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
    SetDownGesture,
  };

  /// https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#measure-battery-voltage-pins
  /// [mV]
  static constexpr float MAX_BATTERY_VOLTAGE = 2150.f;
  /// [mV]
  static constexpr float MIN_BATTERY_VOLTAGE = 1800.f;

  /// Temperature correction offset in °C
  static constexpr float TEMPERATURE_OFFSET = -11;
  /// Humidity correction offset in %
  static constexpr float HUMIDITY_OFFSET = 12;

  /// Gravitational acceleration to correct the accelerometer's values
  static constexpr Vector3 GRAVITATIONAL_ACCELERATION = Vector3(0, 9.71, 0);

  /// SDG (set-down-gesture) values

  /// Downwards acceleration to exceed for a SDG to be detected
  static constexpr float SDG_DOWNWARDS_ACCELERATION_THRESHOLD = 2.5;
  /// Value the absolute vertical acceleration has to be below to finish a SDG
  static constexpr float SDG_END_ACCELERATION = 0.5;
  /// Maximum time for vertical acceleration to return back to
  /// SDG_END_ACCELERATION
  static constexpr ulong SDG_DOWNWARDS_ACCELERATION_MAX_DURATION_MS = 300;
  /// Minimum vertical acceleration for the cooldown to activate. This is to
  /// prevent triggering a SDG while slowing the device down after lifting it
  /// up.
  static constexpr float SDG_COOLDOWN_ACCELERATION_THRESHOLD = 0.5;
  /// Minimum time to wait after SDG_COOLDOWN_ACCELERATION_THRESHOLD has been
  /// exceeded before allowing SDGs to be detected again
  static constexpr ulong SDG_COOLDOWN_AFTER_UPWARDS_ACCELERATION_MS = 200;

  static Mutex<Data>::Guard the();

  void update_battery_percentage(uint32_t voltage);
  void update_inertial_measurements(Vector3 accel, Vector3 gyro);
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

  Vector3 const& gyroscope() const { return m_gyroscope; }
  Vector3 const& acceleration() const { return m_acceleration; }

  float temperature() const { return m_temperature; }

private:
  bool set_down_gesture_detected();
  bool sdg_cooldown_exceeded() {
    return millis() - m_sdg_cooldown >=
           SDG_COOLDOWN_AFTER_UPWARDS_ACCELERATION_MS;
  }

  Lock m_i2c_lock;

  TimerHandle_t m_current_timer{};

  uint8_t m_battery_percentage;

  /// °/s
  Vector3 m_gyroscope;
  /// m/s^2
  /// positive x-axis: from the LSM away from the ESP
  /// positive y-axis: upwards, towards the display
  /// positive z-axis: parallelt to the USB-port, pointing towards it
  Vector3 m_acceleration;
  float m_sdg_cooldown = 0;
  ulong m_set_down_gesture_start = 0;

  /// °C
  float m_temperature;
  /// %
  float m_humidity;
  /// hPa
  float m_pressure;
};
