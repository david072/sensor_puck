#include "data.h"
#include <cstdio>

Mutex<Data>::Guard Data::the() {
  static Mutex<Data> data = Data();
  return data.lock();
}

void Data::update_battery_percentage(uint32_t voltage) {
  // TODO: properly calculate percentage here
  m_battery_percentage =
      (voltage - 1800) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100;
}
