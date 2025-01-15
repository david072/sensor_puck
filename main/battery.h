#pragma once

#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <types.h>

class Battery {
public:
  Battery(gpio_num_t read_pin);

  u32 read_voltage() const;

private:
  static constexpr int READ_SAMPLES = 20;

  adc_oneshot_unit_handle_t m_adc_handle;
  adc_cali_handle_t m_cali_handle;
  adc_channel_t m_adc_channel;
};
