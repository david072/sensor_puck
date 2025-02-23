#include "lsm6dsox.h"
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <util.h>

/// Platform dependent functions for ST drivers
/// A return value of `0` means no error was encountered.

i32 platform_write(void* handle, u8 reg, u8 const* bufp, uint16_t len) {
  u8 buf[len + 1];
  buf[0] = reg;
  memcpy(&buf[1], bufp, len);
  auto res = ESP_ERROR_CHECK_WITHOUT_ABORT(
      i2c_master_transmit(static_cast<i2c_master_dev_handle_t>(handle), buf, 2,
                          Lsm6dsox::I2C_TIMEOUT_MS));
  return res == ESP_OK ? 0 : 1;
}

i32 platform_read(void* handle, u8 reg, u8* bufp, u16 len) {
  auto res = ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_transmit_receive(
      static_cast<i2c_master_dev_handle_t>(handle), &reg, 1, bufp, len,
      Lsm6dsox::I2C_TIMEOUT_MS));
  return res == ESP_OK ? 0 : 1;
}

void platform_delay(uint32_t millisec) { vTaskDelay(pdMS_TO_TICKS(millisec)); }

Lsm6dsox::Lsm6dsox(i2c_master_bus_handle_t i2c_handle, u16 address) {
  i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
  };
  i2c_master_dev_handle_t dev_handle;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &dev, &dev_handle));

  m_device.write_reg = platform_write;
  m_device.read_reg = platform_read;
  m_device.mdelay = platform_delay;
  m_device.handle = dev_handle;

  // wait sensor boot time
  platform_delay(10);

  set_accelerometer_data_rate(DataRate::Rate104Hz);
  set_accelerometer_range(AccelerometerRange::Range4g);

  set_gyroscope_data_rate(DataRate::Rate104Hz);
  set_gyroscope_range(GyroscopeRange::Range2000dps);
}

void Lsm6dsox::set_accelerometer_data_rate(DataRate rate) const {
  lsm6dsox_xl_data_rate_set(&m_device, static_cast<lsm6dsox_odr_xl_t>(rate));
}

void Lsm6dsox::set_accelerometer_range(AccelerometerRange range) {
  lsm6dsox_xl_full_scale_set(&m_device, static_cast<lsm6dsox_fs_xl_t>(range));
}

void Lsm6dsox::set_gyroscope_data_rate(DataRate rate) const {
  lsm6dsox_gy_data_rate_set(&m_device, static_cast<lsm6dsox_odr_g_t>(rate));
}

void Lsm6dsox::set_gyroscope_range(GyroscopeRange range) {
  lsm6dsox_gy_full_scale_set(&m_device, static_cast<lsm6dsox_fs_g_t>(range));
}

std::optional<Lsm6dsox::Data> Lsm6dsox::read_sensor() const {
  lsm6dsox_all_sources_t all_sources;
  lsm6dsox_all_sources_get(&m_device, &all_sources);

  u8 xl_ready;
  lsm6dsox_xl_flag_data_ready_get(&m_device, &xl_ready);
  u8 gy_ready;
  lsm6dsox_gy_flag_data_ready_get(&m_device, &gy_ready);
  if (!xl_ready || !gy_ready) {
    ESP_LOGW("LSM", "Accelerometer or gyroscope not ready");
    return std::nullopt;
  }

  Data result;

  // accelerometer readout
  {
    lsm6dsox_fs_xl_t xl_range;
    lsm6dsox_xl_full_scale_get(&m_device, &xl_range);

    i16 raw_accel[3] = {0};
    lsm6dsox_acceleration_raw_get(&m_device, raw_accel);

    float (*xl_conv)(i16);
    switch (static_cast<AccelerometerRange>(xl_range)) {
    case AccelerometerRange::Range16g:
      xl_conv = lsm6dsox_from_fs16_to_mg;
      break;
    case AccelerometerRange::Range8g:
      xl_conv = lsm6dsox_from_fs8_to_mg;
      break;
    case AccelerometerRange::Range4g:
      xl_conv = lsm6dsox_from_fs4_to_mg;
      break;
    case AccelerometerRange::Range2g:
      xl_conv = lsm6dsox_from_fs2_to_mg;
      break;
    default:
      xl_conv = lsm6dsox_from_fs2_to_mg;
      break;
    }

    result.acc_x = xl_conv(raw_accel[0]) / 1000.f * GRAVITY_STANDARD;
    result.acc_y = xl_conv(raw_accel[1]) / 1000.f * GRAVITY_STANDARD;
    result.acc_z = xl_conv(raw_accel[2]) / 1000.f * GRAVITY_STANDARD;
  }

  // gyroscope readout
  {
    lsm6dsox_fs_g_t gy_range;
    lsm6dsox_gy_full_scale_get(&m_device, &gy_range);

    i16 raw_rot[3] = {0};
    lsm6dsox_angular_rate_raw_get(&m_device, raw_rot);

    float (*gy_conv)(i16);
    switch (static_cast<GyroscopeRange>(gy_range)) {
    case GyroscopeRange::Range2000dps:
      gy_conv = lsm6dsox_from_fs2000_to_mdps;
      break;
    case GyroscopeRange::Range1000dps:
      gy_conv = lsm6dsox_from_fs1000_to_mdps;
      break;
    case GyroscopeRange::Range500dps:
      gy_conv = lsm6dsox_from_fs500_to_mdps;
      break;
    case GyroscopeRange::Range250dps:
      gy_conv = lsm6dsox_from_fs250_to_mdps;
      break;
    default:
      gy_conv = lsm6dsox_from_fs250_to_mdps;
      break;
    }

    result.pitch = gy_conv(raw_rot[0]) / 1000.f;
    result.roll = gy_conv(raw_rot[1]) / 1000.f;
    result.yaw = gy_conv(raw_rot[2]) / 1000.f;
  }

  return result;
}

void Lsm6dsox::reset() const {
  lsm6dsox_reset_set(&m_device, PROPERTY_ENABLE);
  lsm6dsox_i3c_disable_set(&m_device, LSM6DSOX_I3C_DISABLE);
  lsm6dsox_i2c_interface_set(&m_device, LSM6DSOX_I2C_ENABLE);
}
