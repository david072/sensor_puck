#pragma once

#include "lsm6dsox_pid/lsm6dsox_reg.h"
#include <driver/i2c_master.h>
#include <types.h>

class Lsm6dsox {
public:
  static constexpr u32 I2C_TIMEOUT_MS = 50;
  static constexpr u16 DEFAULT_ADDRESS = 0x6A;
  static constexpr u16 ALTERNATIVE_ADDRESS = 0x6B;

  enum class DataRate : u8 {
    Off = LSM6DSOX_XL_ODR_OFF,
    /// Low-power mody only
    Rate1_6Hz = LSM6DSOX_XL_ODR_1Hz6,
    /// Low-power mode
    Rate12_5Hz = LSM6DSOX_XL_ODR_12Hz5,
    /// Low-power mode
    Rate26Hz = LSM6DSOX_XL_ODR_26Hz,
    /// Low-power mode
    Rate52Hz = LSM6DSOX_XL_ODR_52Hz,
    /// Normal mode
    Rate104Hz = LSM6DSOX_XL_ODR_104Hz,
    /// Normal mode
    Rate208Hz = LSM6DSOX_XL_ODR_208Hz,
    /// High-performance mode
    Rate417Hz = LSM6DSOX_XL_ODR_417Hz,
    /// High-performance mode
    Rate833Hz = LSM6DSOX_XL_ODR_833Hz,
    /// High-performance mode
    Rate1667Hz = LSM6DSOX_XL_ODR_1667Hz,
    /// High-performance mode
    Rate3333kHz = LSM6DSOX_XL_ODR_3333Hz,
    /// High-performance mode
    Rate6667kHz = LSM6DSOX_XL_ODR_6667Hz,
  };

  /// Range in `g`
  enum class AccelerometerRange : u8 {
    Range2g = LSM6DSOX_2g,
    Range16g = LSM6DSOX_16g,
    Range4g = LSM6DSOX_4g,
    Range8g = LSM6DSOX_8g,
  };

  /// Range in `°/s`
  enum class GyroscopeRange : u8 {
    Range250dps = LSM6DSOX_250dps,
    Range500dps = LSM6DSOX_500dps,
    Range1000dps = LSM6DSOX_1000dps,
    Range2000dps = LSM6DSOX_2000dps,
  };

  /// Accelerometer readings in m/s^2
  ///
  /// Gyroscope readings in °/s
  struct Data {
    float acc_x;
    float acc_y;
    float acc_z;
    float pitch;
    float yaw;
    float roll;
  };

  Lsm6dsox(i2c_master_bus_handle_t i2c_handle, u16 address = DEFAULT_ADDRESS);

  void set_accelerometer_data_rate(DataRate rate) const;
  void set_accelerometer_range(AccelerometerRange range);

  void set_gyroscope_data_rate(DataRate rate) const;
  void set_gyroscope_range(GyroscopeRange range);

  std::optional<Data> read_sensor() const;

  void reset() const;

private:
  /// Earth's gravity in m/s^2 (i.e. 1g)
  static constexpr float GRAVITY_STANDARD = 9.80665f;

  stmdev_ctx_t m_device;
};
