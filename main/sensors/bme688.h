#pragma once

#include "bme688/bme68x.h"
#include <driver/i2c_master.h>
#include <driver/i2s_common.h>
#include <driver/i2s_types.h>
#include <optional>
#include <types.h>

class Bme688 {
public:
  static constexpr u32 I2C_TIMEOUT_MS = 50;
  static constexpr u16 BME_DEFAULT_ADDRESS = 0b1110110;
  static constexpr u16 BME_ALTERNATIVE_ADDRESS = 0b1110111;

  enum class Oversampling : u8 {
    Os1x = BME68X_OS_1X,
    Os2x = BME68X_OS_2X,
    Os4x = BME68X_OS_4X,
    Os8x = BME68X_OS_8X,
    Os16x = BME68X_OS_16X,
  };

  enum class FilterSize : u8 {
    Size1 = BME68X_FILTER_SIZE_1,
    Size3 = BME68X_FILTER_SIZE_3,
    Size7 = BME68X_FILTER_SIZE_7,
    Size15 = BME68X_FILTER_SIZE_15,
    Size31 = BME68X_FILTER_SIZE_31,
    Size63 = BME68X_FILTER_SIZE_63,
    Size127 = BME68X_FILTER_SIZE_127,
  };

  struct Data {
    /// Temperature in °C
    float temperature;
    /// Relative humidity in %
    float humidity;
    /// Pressure in hPa
    float pressure;
  };

  Bme688(i2c_master_bus_handle_t i2c_handle, u16 address = BME_DEFAULT_ADDRESS);

  std::optional<Data> read_sensor();

  void set_temperature_oversampling(Oversampling sampling);
  void set_humidity_oversampling(Oversampling sampling);
  void set_pressure_oversampling(Oversampling sampling);
  void set_iir_filter_size(FilterSize size);

  Oversampling get_temperature_oversampling();
  Oversampling get_humidity_oversampling();
  Oversampling get_pressure_oversampling();

private:
  void set_conf();
  bme68x_conf get_conf();

  i2c_master_dev_handle_t m_device{};
  bme68x_dev m_sensor{};
  bme68x_conf m_conf{};
  bme68x_heatr_conf m_heater_conf{};
};
