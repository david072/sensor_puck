#include "sgp41.h"
#include "sensirion_i2c_hal.h"
#include <embedded_i2c_sgp41/sgp41_i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

void Sgp41::GasIndexAlgorithm::initialize(float sampling_interval_s) {
  GasIndexAlgorithm_init_with_sampling_interval(
      &m_voc_params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC, sampling_interval_s);
  GasIndexAlgorithm_init_with_sampling_interval(
      &m_nox_params, GasIndexAlgorithm_ALGORITHM_TYPE_NOX, sampling_interval_s);
}

Sgp41::Data Sgp41::GasIndexAlgorithm::process(u16 sraw_voc, u16 sraw_nox) {
  i32 voc_index, nox_index;
  GasIndexAlgorithm_process(&m_voc_params, sraw_voc, &voc_index);
  GasIndexAlgorithm_process(&m_nox_params, sraw_nox, &nox_index);
  return Data{
      .voc_index = static_cast<u16>(voc_index),
      .nox_index = static_cast<u16>(nox_index),
  };
}

void Sgp41::GasIndexAlgorithm::reset() {
  GasIndexAlgorithm_reset(&m_voc_params);
  GasIndexAlgorithm_reset(&m_nox_params);
}

Sgp41::Sgp41(i2c_master_bus_handle_t i2c_handle, u16 address) {
  sensirion_i2c_hal_init(i2c_handle, address);

  u16 serial_number[3];
  if (auto error = sgp41_get_serial_number(serial_number); error != 0) {
    ESP_LOGE("SGP41", "Error getting serial number: %d", error);
  }
  ESP_LOGI("SGP41", "Serial number: 0x%04x%04x%04x", serial_number[0],
           serial_number[1], serial_number[2]);
}

void Sgp41::perform_conditioning(float temperature, float humidity) {
  auto compensation_t =
      static_cast<u16>(lround((temperature + 45) * 65535 / 175));
  auto compensation_rh = static_cast<u16>(lround(humidity * 65535 / 100));

  u16 sraw_voc;
  for (size_t i = 0; i < 10; ++i) {
    sgp41_execute_conditioning(compensation_rh, compensation_t, &sraw_voc);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

std::optional<Sgp41::Data> Sgp41::read(float temperature, float humidity,
                                       GasIndexAlgorithm& gia) {
  u16 sraw_voc, sraw_nox;
  if (sgp41_measure_raw_signals(compensation_rh(humidity),
                                compensation_t(temperature), &sraw_voc,
                                &sraw_nox) != 0) {
    return std::nullopt;
  }

  return gia.process(sraw_voc, sraw_nox);
}

void Sgp41::turn_heater_off() { sgp41_turn_heater_off(); }

u16 Sgp41::compensation_t(float temperature) {
  return static_cast<u16>(lround((temperature + 45) * 65535 / 175));
}

u16 Sgp41::compensation_rh(float humidity) {
  return static_cast<u16>(lround(humidity * 65535 / 100));
}
