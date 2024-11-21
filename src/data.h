#pragma once

#include <cstdint>

class Data {
public:
  /// https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#measure-battery-voltage-pins
  /// [mV]
  static constexpr uint32_t MAX_BATTERY_VOLTAGE = 2150;
  /// [mV]
  static constexpr uint32_t MIN_BATTERY_VOLTAGE = 1800;

  static Data& the();

  void update_battery_percentage(uint32_t voltage);

  uint8_t battery_percentage() const { return m_battery_percentage; }

private:
  uint8_t m_battery_percentage;
};
