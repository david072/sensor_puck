#include "scd41.h"
#include "scd4x_i2c.h"
#include "sensirion_i2c_hal.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

Scd41::Scd41(i2c_master_bus_handle_t i2c_handle, u16 address) {
  sensirion_i2c_hal_init(i2c_handle, address);

  // TODO: Currently, we don't do this, since we just leave the sensor running
  // while in deep sleep. This is because this way, when we wake up, we
  // immediately have sensor readings available (as opposed to having to wait
  // the 30s it takes to get a low power measurement).

  // clean up potential SCD41 states
  // scd4x_wake_up();
  // scd4x_stop_periodic_measurement();
  // scd4x_reinit();
}

void Scd41::start_low_power_periodic_measurement() {
  scd4x_start_low_power_periodic_measurement();
}

std::optional<u16> Scd41::read_co2() {
  bool data_ready;
  if (auto res = scd4x_get_data_ready_flag(&data_ready); res != 0) {
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

  return co2;
}

void Scd41::stop_periodic_measurement() { scd4x_stop_periodic_measurement(); }
