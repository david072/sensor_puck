#pragma once

#include <driver/i2c_master.h>
#include <types.h>

class Lsm6dsox {
public:
  static constexpr u16 DEFAULT_ADDRESS = 0x6A;
  static constexpr u16 ALTERNATIVE_ADDRESS = 0x6B;

  static constexpr u32 I2C_TIMEOUT_MS = 50;

  enum class DataRate : u8 {
    Off = 0b0000,
    /// Low-power mody only
    Rate1_6Hz = 0b1011,
    /// Low-power mode
    Rate12_5Hz = 0b0001,
    /// Low-power mode
    Rate26Hz = 0b0010,
    /// Low-power mode
    Rate52Hz = 0b001,
    /// Normal mode
    Rate104Hz = 0b0100,
    /// Normal mode
    Rate208Hz = 0b0101,
    /// High-performance mode
    Rate416Hz = 0b0110,
    /// High-performance mode
    Rate833Hz = 0b0111,
    /// High-performance mode
    Rate1_66kHz = 0b1000,
    /// High-performance mode
    Rate3_33kHz = 0b1001,
    /// High-performance mode
    Rate6_66kHz = 0b1010,
  };

  /// Range in `g`
  enum class AccelerometerRange : u8 {
    Range2g = 0b00,
    Range16g = 0b01,
    Range4g = 0b10,
    Range8g = 0b11,
  };

  /// Range in `°/s`
  enum class GyroscopeRange : u8 {
    Range250dps = 0b00,
    Range500dps = 0b01,
    Range1000dps = 0b10,
    Range2000dps = 0b11,
  };

  /// Accelerometer readings in m/s^2
  ///
  /// Gyroscope readings in rad/s
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

  Data read_sensor() const;

private:
  /// Earth's gravity in m/s^2
  static constexpr float GRAVITY_STANDARD = 9.80665f;

  /// Accelerometer control register 1
  ///
  /// ODR_XL3 | ODR_XL2 | ODR_XL1 | ODR_XL0 | FS1_XL | FS0_XL | LPF2_XL_EN | 0
  static constexpr u8 REG_CTRL1_XL = 0x10;
  /// Gyroscope control register 1
  ///
  /// ODR_G3 | ODR_G2 | ODR_G1 | ODR_G0 | FS1_G | FS0_G | FS_125 | 0
  static constexpr u8 REG_CTRL2_G = 0x11;
  /// Gyroscope x-axis data bit 7:0 register. The other gyroscope values are in
  /// the next 5 registers.
  static constexpr u8 REG_OUTX_L_G = 0x22;

  void write(u8 reg, u8 data) const;
  u8 read(u8 reg) const;
  void read(u8 reg, u8* data, size_t length) const;

  AccelerometerRange m_acc_range;
  GyroscopeRange m_gyro_range;

  i2c_master_dev_handle_t m_device;
};
