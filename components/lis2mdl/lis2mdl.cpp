#include "lis2mdl.h"
#include <cstring>
#include <freertos/FreeRTOS.h>

/// Platform dependent functions for ST drivers
/// A return value of `0` means no error was encountered.

int32_t Lis2mdl::platform_write(void* handle, uint8_t reg, uint8_t const* bufp,
                                uint16_t len) {
  u8 buf[len + 1];
  buf[0] = reg;
  memcpy(&buf[1], bufp, len);
  auto res = ESP_ERROR_CHECK_WITHOUT_ABORT(
      i2c_master_transmit(static_cast<i2c_master_dev_handle_t>(handle), buf, 2,
                          Lis2mdl::I2C_TIMEOUT_MS));
  return res == ESP_OK ? 0 : 1;
}

int32_t Lis2mdl::platform_read(void* handle, uint8_t reg, uint8_t* bufp,
                               uint16_t len) {
  auto res = ESP_ERROR_CHECK_WITHOUT_ABORT(
      i2c_master_transmit_receive(static_cast<i2c_master_dev_handle_t>(handle),
                                  &reg, 1, bufp, len, Lis2mdl::I2C_TIMEOUT_MS));
  return res == ESP_OK ? 0 : 1;
}

void Lis2mdl::platform_delay(uint32_t millisec) {
  vTaskDelay(pdMS_TO_TICKS(millisec));
}

Lis2mdl::Lis2mdl(i2c_master_bus_handle_t i2c_handle, u16 address) {
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

  platform_delay(10);

  lis2mdl_reset_set(&m_device, PROPERTY_ENABLE);
  platform_delay(10);

  lis2mdl_block_data_update_set(&m_device, PROPERTY_ENABLE);
  lis2mdl_set_rst_mode_set(&m_device, LIS2MDL_SENS_OFF_CANC_EVERY_ODR);
  lis2mdl_operating_mode_set(&m_device, LIS2MDL_CONTINUOUS_MODE);
  lis2mdl_offset_temp_comp_set(&m_device, PROPERTY_ENABLE);
  set_data_rate(DataRate::Rate100Hz);
}

void Lis2mdl::set_data_rate(DataRate rate) {
  lis2mdl_data_rate_set(&m_device, static_cast<lis2mdl_odr_t>(rate));
}

std::optional<Lis2mdl::Data> Lis2mdl::read_sensor() {
  u8 mag_ready;
  lis2mdl_mag_data_ready_get(&m_device, &mag_ready);
  if (!mag_ready)
    return std::nullopt;

  i16 raw[3];
  lis2mdl_magnetic_raw_get(&m_device, raw);
  return Data{
      .x = lis2mdl_from_lsb_to_mgauss(raw[0]) / 1000.f,
      .y = lis2mdl_from_lsb_to_mgauss(raw[1]) / 1000.f,
      .z = lis2mdl_from_lsb_to_mgauss(raw[2]) / 1000.f,
  };
}

void Lis2mdl::power_down() {
  lis2mdl_operating_mode_set(&m_device, LIS2MDL_POWER_DOWN);
}
