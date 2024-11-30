#include "bme688.h"
#include <cstdlib>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <unistd.h>

constexpr char const* bme688_error_to_string(i8 status) {
#define CASE(v)                                                                \
  case v:                                                                      \
    return #v;

  switch (status) {
    CASE(BME68X_E_NULL_PTR)
    CASE(BME68X_E_COM_FAIL)
    CASE(BME68X_E_DEV_NOT_FOUND)
    CASE(BME68X_E_INVALID_LENGTH)
    CASE(BME68X_E_SELF_TEST)

    CASE(BME68X_W_DEFINE_OP_MODE)
    CASE(BME68X_W_NO_NEW_DATA)
    CASE(BME68X_W_DEFINE_SHD_HEATR_DUR)
  default:
    return "unknown error";
  }

#undef CASE
}

#define BME_ERROR_CHECK(x)                                                     \
  do {                                                                         \
    auto status = x;                                                           \
    if (status == BME68X_OK)                                                   \
      break;                                                                   \
    ESP_LOGE(__FILE__, "BME688 failed at %d with: %s", __LINE__,               \
             bme688_error_to_string(status));                                  \
    abort();                                                                   \
  } while (false);

i8 i2c_read(u8 reg, u8* reg_data, u32 length, void* intf_ptr) {
  auto dev = static_cast<i2c_master_dev_handle_t>(intf_ptr);
  auto result = i2c_master_transmit_receive(dev, &reg, 1, reg_data, length,
                                            Bme688::I2C_TIMEOUT_MS);
  if (result != ESP_OK) {
    ESP_LOGD("BME688", "i2c_read receive failure: %s", esp_err_to_name(result));
    return BME68X_E_COM_FAIL;
  }
  return BME68X_OK;
}

i8 i2c_write(u8 reg, u8 const* reg_data, u32 length, void* intf_ptr) {
  auto dev = static_cast<i2c_master_dev_handle_t>(intf_ptr);

  // copy the data to insert the register at the beginning
  uint8_t buf[64] = {0};
  uint16_t buf_len = 0;
  buf[buf_len++] = reg;
  for (uint8_t i = 0; i < length; i++) {
    buf[buf_len++] = reg_data[i];
  }

  auto result = i2c_master_transmit(dev, buf, buf_len, Bme688::I2C_TIMEOUT_MS);
  if (result != ESP_OK) {
    ESP_LOGD("BME688", "i2c_write failed: %s", esp_err_to_name(result));
    return BME68X_E_COM_FAIL;
  }
  return BME68X_OK;
}

Bme688::Bme688(i2c_master_bus_handle_t i2c_handle, u16 address) {
  i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &dev, &m_device));

  m_sensor.chip_id = address;
  m_sensor.intf = BME68X_I2C_INTF;
  m_sensor.intf_ptr = m_device;
  m_sensor.read = &i2c_read;
  m_sensor.write = &i2c_write;
  m_sensor.amb_temp = 25; // used for defining the heater temperature
  m_sensor.delay_us = [](u32 us, void* intf_ptr) { usleep(us); };

  BME_ERROR_CHECK(bme68x_init(&m_sensor));
  BME_ERROR_CHECK(bme68x_set_op_mode(BME68X_FORCED_MODE, &m_sensor));

  m_heater_conf.enable = BME68X_DISABLE;
  BME_ERROR_CHECK(
      bme68x_set_heatr_conf(BME68X_FORCED_MODE, &m_heater_conf, &m_sensor));
}

std::optional<Bme688::Data> Bme688::read_sensor() {
  BME_ERROR_CHECK(bme68x_set_op_mode(BME68X_FORCED_MODE, &m_sensor));
  auto measurement_duration =
      bme68x_get_meas_dur(BME68X_FORCED_MODE, &m_conf, &m_sensor) +
      m_heater_conf.heatr_dur * 1000;
  if (measurement_duration == 0)
    return std::nullopt;

  usleep(measurement_duration);

  bme68x_data data;
  u8 n_fields;
  BME_ERROR_CHECK(
      bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &m_sensor));

  return Data{
      .temperature = data.temperature,
      .humidity = data.humidity,
      .pressure = data.pressure / 100.f,
  };
}

void Bme688::set_temperature_oversampling(Oversampling sampling) {
  m_conf.os_temp = static_cast<u8>(sampling);
  set_conf();
}

void Bme688::set_humidity_oversampling(Oversampling sampling) {
  m_conf.os_hum = static_cast<u8>(sampling);
  set_conf();
}

void Bme688::set_pressure_oversampling(Oversampling sampling) {
  m_conf.os_pres = static_cast<u8>(sampling);
  set_conf();
}

void Bme688::set_iir_filter_size(FilterSize size) {
  m_conf.filter = static_cast<u8>(size);
  set_conf();
}

Bme688::Oversampling Bme688::get_temperature_oversampling() {
  return static_cast<Oversampling>(get_conf().os_temp);
}

Bme688::Oversampling Bme688::get_humidity_oversampling() {
  return static_cast<Oversampling>(get_conf().os_hum);
}

Bme688::Oversampling Bme688::get_pressure_oversampling() {
  return static_cast<Oversampling>(get_conf().os_pres);
}

void Bme688::set_conf() {
  BME_ERROR_CHECK(bme68x_set_conf(&m_conf, &m_sensor));
}

bme68x_conf Bme688::get_conf() {
  bme68x_conf conf;
  BME_ERROR_CHECK(bme68x_get_conf(&conf, &m_sensor));
  return conf;
}
