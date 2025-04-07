#pragma once

#include <driver/i2c_master.h>
#include <optional>
#include <types.h>

class Scd41 {
public:
  static constexpr u16 DEFAULT_ADDRESS = 0x62;
  static constexpr u32 I2C_TIMEOUT_MS = 50;

  struct Data {
    /// CO2 concentration in ppm
    u16 co2;
    /// Temperature in °C
    float temperature;
    /// Relative humidity in %
    float humidity;
  };

  Scd41(i2c_master_bus_handle_t i2c_handle);

  void start_periodic_measurement();
  void start_low_power_periodic_measurement();
  std::optional<Data> read();
  /// Stops the period measurement (both normal and low power).
  void stop_periodic_measurement();

  void power_down();
};
