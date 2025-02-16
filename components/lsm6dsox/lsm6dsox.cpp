#include "lsm6dsox.h"
#include <util.h>

Lsm6dsox::Lsm6dsox(i2c_master_bus_handle_t i2c_handle, u16 address) {
  i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &dev, &m_device));

  reset();

  set_accelerometer_data_rate(DataRate::Rate104Hz);
  set_accelerometer_range(AccelerometerRange::Range4g);

  set_gyroscope_data_rate(DataRate::Rate104Hz);
  set_gyroscope_range(GyroscopeRange::Range2000dps);
}

void Lsm6dsox::set_accelerometer_data_rate(DataRate rate) const {
  auto reg = read(REG_CTRL1_XL) & 0b00001111;
  write(REG_CTRL1_XL, reg | (static_cast<u8>(rate) << 4));
}

void Lsm6dsox::set_accelerometer_range(AccelerometerRange range) {
  m_acc_range = range;
  auto reg = read(REG_CTRL1_XL) & 0b11110011;
  write(REG_CTRL1_XL, reg | (static_cast<u8>(range) << 2));
}

void Lsm6dsox::set_gyroscope_data_rate(DataRate rate) const {
  auto reg = read(REG_CTRL2_G) & 0b00001111;
  write(REG_CTRL2_G, reg | (static_cast<u8>(rate) << 4));
}

void Lsm6dsox::set_gyroscope_range(GyroscopeRange range) {
  m_gyro_range = range;
  auto reg = read(REG_CTRL2_G) & 0b11110001;
  write(REG_CTRL2_G, reg | (static_cast<u8>(range) << 2));
}

Lsm6dsox::Data Lsm6dsox::read_sensor() const {
  u8 buf[12];
  read(REG_OUTX_L_G, buf, 12);
  i16 raw_gyro_x = (buf[1] << 8) | buf[0];
  i16 raw_gyro_y = (buf[3] << 8) | buf[2];
  i16 raw_gyro_z = (buf[5] << 8) | buf[4];

  i16 raw_acc_x = (buf[7] << 8) | buf[6];
  i16 raw_acc_y = (buf[9] << 8) | buf[8];
  i16 raw_acc_z = (buf[11] << 8) | buf[10];

  // https://github.com/adafruit/Adafruit_LSM6DS/blob/5aeda9947a93ed76b8db8bb4ed0c410de9c36c54/Adafruit_LSM6DS.cpp#L476
  // no idea how they got these values...
  float gyro_scale = 1;
  switch (m_gyro_range) {
    // case ISM330DHCX_GYRO_RANGE_4000_DPS:
    //   gyro_scale = 140.0;
    //   break;
  case GyroscopeRange::Range2000dps:
    gyro_scale = 70.0;
    break;
  case GyroscopeRange::Range1000dps:
    gyro_scale = 35.0;
    break;
  case GyroscopeRange::Range500dps:
    gyro_scale = 17.50;
    break;
  case GyroscopeRange::Range250dps:
    gyro_scale = 8.75;
    break;
    // case GyroscopeRange::Range125dps:
    //   gyro_scale = 4.375;
    //   break;
  }

  float pitch = raw_gyro_x * gyro_scale * DEG_TO_RAD / 1000.0;
  float yaw = raw_gyro_z * gyro_scale * DEG_TO_RAD / 1000.0;
  float roll = raw_gyro_y * gyro_scale * DEG_TO_RAD / 1000.0;

  // https://github.com/adafruit/Adafruit_LSM6DS/blob/5aeda9947a93ed76b8db8bb4ed0c410de9c36c54/Adafruit_LSM6DS.cpp#L502
  // no idea how they got these values...
  float accel_scale = 1;
  switch (m_acc_range) {
  case AccelerometerRange::Range16g:
    accel_scale = 0.488;
    break;
  case AccelerometerRange::Range8g:
    accel_scale = 0.244;
    break;
  case AccelerometerRange::Range4g:
    accel_scale = 0.122;
    break;
  case AccelerometerRange::Range2g:
    accel_scale = 0.061;
    break;
  }

  float acc_x = raw_acc_x * accel_scale * GRAVITY_STANDARD / 1000.0;
  float acc_y = raw_acc_y * accel_scale * GRAVITY_STANDARD / 1000.0;
  float acc_z = raw_acc_z * accel_scale * GRAVITY_STANDARD / 1000.0;

  return Data{
      .acc_x = acc_x,
      .acc_y = acc_y,
      .acc_z = acc_z,
      .pitch = pitch,
      .yaw = yaw,
      .roll = roll,
  };
}

void Lsm6dsox::reset() const { write(REG_CTRL3_C, 0b10000101); }

void Lsm6dsox::write(u8 reg, u8 data) const {
  u8 buf[2];
  buf[0] = reg;
  buf[1] = data;
  ESP_ERROR_CHECK(i2c_master_transmit(m_device, buf, 2, I2C_TIMEOUT_MS));
}

u8 Lsm6dsox::read(u8 reg) const {
  u8 buf;
  ESP_ERROR_CHECK(
      i2c_master_transmit_receive(m_device, &reg, 1, &buf, 1, I2C_TIMEOUT_MS));
  return buf;
}

void Lsm6dsox::read(u8 reg, u8* data, size_t length) const {
  ESP_ERROR_CHECK(i2c_master_transmit_receive(m_device, &reg, 1, data, length,
                                              I2C_TIMEOUT_MS));
}
