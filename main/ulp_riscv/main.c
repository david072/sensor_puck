#include "sensirion_i2c_hal.h"
#include <scd4x_i2c.h>
#include <stdbool.h>
#include <stdint.h>
#include <ulp_riscv_lock_ulp_core.h>
#include <ulp_riscv_utils.h>

volatile ulp_riscv_lock_t lock;

volatile uint32_t last_measurement = 0;
volatile uint32_t wake_threshold_ppm = 0;

uint16_t scd41_measure_single_shot() {
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
  return co2;
}

int main(void) {
  // make sure the variable doesn't get optimized out or something
  wake_threshold_ppm;
  ulp_riscv_lock_acquire(&lock);

  sensirion_i2c_hal_init();
  // clean up potential SCD41 states
  scd4x_wake_up();
  scd4x_stop_periodic_measurement();
  scd4x_reinit();

  last_measurement = scd41_measure_single_shot();
  if (last_measurement >= wake_threshold_ppm) {
    ulp_riscv_wakeup_main_processor();
  }

  scd4x_power_down();
  sensirion_i2c_hal_free();
  ulp_riscv_lock_release(&lock);
  // we will be awoken again by the timer, as configured by the main CPU
  return 0;
}
