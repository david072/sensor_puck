#include "data.h"
#include <cstdio>

Data& Data::the() {
  static Data data = Data();
  return data;
}

void Data::update_battery_percentage(uint32_t voltage) {
  // TODO: properly calculate percentage here
  m_battery_percentage =
      (voltage - 1800) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100;
}
