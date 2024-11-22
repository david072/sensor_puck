#include "data.h"
#include <Arduino.h>
#include <cstdio>

template <typename T>
T low_pass_filter(T noisy_signal, T out, float gain = 0.1) {
  return out + (noisy_signal - out) * gain;
}

Mutex<Data>::Guard Data::the() {
  static Mutex<Data> data = Data();
  return data.lock();
}

void Data::update_battery_percentage(uint32_t voltage) {
  // TODO: properly calculate percentage here
  m_battery_percentage =
      (voltage - 1800) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100;
}

void Data::update_gyroscope(Vector3 v) {
  m_gyroscope = low_pass_filter(v * RAD_TO_DEG, m_gyroscope, 0.5);
}
