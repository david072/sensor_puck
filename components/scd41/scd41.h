#pragma once

#include <driver/i2c_master.h>
#include <optional>
#include <types.h>

class Scd41 {
public:
  static constexpr u16 DEFAULT_ADDRESS = 0x62;
  static constexpr u32 I2C_TIMEOUT_MS = 50;

  Scd41(i2c_master_bus_handle_t i2c_handle, u16 address = DEFAULT_ADDRESS);

  void start_low_power_periodic_measurement();
  std::optional<u16> read_co2();
  /// Stops the period measurement (both normal and low power).
  void stop_periodic_measurement();
};
