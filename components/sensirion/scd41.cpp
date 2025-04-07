#include "scd41.h"
#include "scd4x_i2c.h"
#include "sensirion_i2c_hal.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

Scd41::Scd41(i2c_master_bus_handle_t i2c_handle) {
  sensirion_i2c_hal_init(i2c_handle, DEFAULT_ADDRESS);

  // clean up potential SCD41 states
  scd4x_init(DEFAULT_ADDRESS);
  scd4x_wake_up();
  scd4x_stop_periodic_measurement();
  scd4x_reinit();
}

void Scd41::start_periodic_measurement() { scd4x_start_periodic_measurement(); }

void Scd41::start_low_power_periodic_measurement() {
  scd4x_start_low_power_periodic_measurement();
}

std::optional<Scd41::Data> Scd41::read() {
  bool data_ready;
  if (auto res = scd4x_get_data_ready_status(&data_ready); res != 0) {
    ESP_LOGE("SCD41", "Getting data ready flag failed: %d", res);
    return std::nullopt;
  }
  if (!data_ready) {
    ESP_LOGW("SCD41", "No data available!");
    return std::nullopt;
  }

  u16 co2;
  i32 temp, hum;
  if (auto res = scd4x_read_measurement(&co2, &temp, &hum); res != 0) {
    ESP_LOGE("SCD41", "Getting data ready flag failed: %d", res);
    return std::nullopt;
  }

  return Data{
      .co2 = co2,
      .temperature = static_cast<float>(temp) / 1000.f,
      .humidity = static_cast<float>(hum) / 1000.f,
  };
}

void Scd41::stop_periodic_measurement() { scd4x_stop_periodic_measurement(); }

void Scd41::power_down() {
  scd4x_stop_periodic_measurement();
  scd4x_power_down();
}
