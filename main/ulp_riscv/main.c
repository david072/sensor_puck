#include "sensirion_i2c_hal.h"
#include <scd4x_i2c.h>
#include <stdbool.h>
#include <stdint.h>
#include <ulp_riscv_lock_ulp_core.h>
#include <ulp_riscv_utils.h>

#define SCD_SAMPLES 3

volatile ulp_riscv_lock_t lock;

volatile uint32_t last_co2_measurement = 0;
volatile uint32_t last_temperature_measurement = 0;
volatile uint32_t wake_threshold_ppm = 0;

typedef struct {
  uint16_t co2;
  int32_t temp;
} Data;

Data scd41_measure_single_shot(void) {
  scd4x_measure_single_shot();

  bool data_rdy = false;
  scd4x_get_data_ready_flag(&data_rdy);
  while (!data_rdy) {
    // wait 5s if there is no data available yet
    ulp_riscv_delay_cycles(ULP_RISCV_CYCLES_PER_US * 5000 * 1000);
    scd4x_get_data_ready_flag(&data_rdy);
  }

  uint16_t co2 = 0;
  int32_t temp = 0, rh = 0;
  scd4x_read_measurement(&co2, &temp, &rh);

  return (Data){
      .co2 = co2,
      .temp = temp,
  };
}

int main(void) {
  // make sure the variable doesn't get optimized out or something
  wake_threshold_ppm;

  Data data_sum = {.co2 = 0, .temp = 0};
  for (int i = 0; i < SCD_SAMPLES; ++i) {
    ulp_riscv_lock_acquire(&lock);

    sensirion_i2c_hal_init();
    // clean up potential SCD41 states
    scd4x_wake_up();
    scd4x_stop_periodic_measurement();
    scd4x_reinit();

    Data result = scd41_measure_single_shot();
    data_sum.co2 += result.co2;
    data_sum.temp += result.temp;

    last_co2_measurement = data_sum.co2 / (i + 1);
    last_temperature_measurement = data_sum.temp / (i + 1);

    if (last_co2_measurement >= wake_threshold_ppm) {
      ulp_riscv_wakeup_main_processor();
    }

    scd4x_power_down();
    sensirion_i2c_hal_free();
    ulp_riscv_lock_release(&lock);

    // wait 10ms to give the main CPU time to take the lock if woken up by the
    // user
    ulp_riscv_delay_cycles(ULP_RISCV_CYCLES_PER_US * 10000);
  }

  // we will be awoken again by the timer, as configured by the main CPU
  return 0;
}
