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

class Data {
public:
  /// https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#measure-battery-voltage-pins
  /// [mV]
  static constexpr uint32_t MAX_BATTERY_VOLTAGE = 2150;
  /// [mV]
  static constexpr uint32_t MIN_BATTERY_VOLTAGE = 1800;

  static Mutex<Data>::Guard the();

  void update_battery_percentage(uint32_t voltage);
  void update_gyroscope(Vector3 v);

  uint8_t battery_percentage() const { return m_battery_percentage; }

  /// °/s
  Vector3 const& gyroscope() const { return m_gyroscope; }

private:
  uint8_t m_battery_percentage;

  /// °/s
  Vector3 m_gyroscope;
};
