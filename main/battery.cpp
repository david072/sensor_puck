#include "battery.h"

Battery::Battery(gpio_num_t read_pin) {
  adc_unit_t adc_unit;
  ESP_ERROR_CHECK(
      adc_oneshot_io_to_channel(read_pin, &adc_unit, &m_adc_channel));

  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = adc_unit,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &m_adc_handle));

  adc_oneshot_chan_cfg_t config = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(m_adc_handle, m_adc_channel, &config));

  adc_cali_curve_fitting_config_t cali_config = {
      .unit_id = adc_unit,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(
      adc_cali_create_scheme_curve_fitting(&cali_config, &m_cali_handle));
}

u32 Battery::read_voltage() const {
  int battery_voltage = 0;
  for (int i = 0; i < READ_SAMPLES; ++i) {
    int v;
    ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(
        m_adc_handle, m_cali_handle, m_adc_channel, &v));
    battery_voltage += v;
  }

  return battery_voltage / READ_SAMPLES;
}
