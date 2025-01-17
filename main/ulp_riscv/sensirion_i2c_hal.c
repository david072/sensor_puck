/*
 * Copyright (c) 2018, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "sensirion_i2c_hal.h"
#include "sensirion_common.h"
#include "sensirion_config.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * INSTRUCTIONS
 * ============
 *
 * Implement all functions where they are marked as IMPLEMENT.
 * Follow the function specification in the comments.
 */

/**
 * Select the current i2c bus by index.
 * All following i2c operations will be directed at that bus.
 *
 * THE IMPLEMENTATION IS OPTIONAL ON SINGLE-BUS SETUPS (all sensors on the same
 * bus)
 *
 * @param bus_idx   Bus index to select
 * @returns         0 on success, an error code otherwise
 */
int16_t sensirion_i2c_hal_select_bus(uint8_t bus_idx) { return 0; }

/**
 * Initialize all hard- and software components that are needed for the I2C
 * communication.
 */
void sensirion_i2c_hal_init(void) {
  ulp_riscv_gpio_init(SCL);
  ulp_riscv_gpio_init(SDA);
  ulp_riscv_gpio_output_enable(SCL);
  ulp_riscv_gpio_output_enable(SDA);
}

/**
 * Release all resources initialized by sensirion_i2c_hal_init().
 */
void sensirion_i2c_hal_free(void) {
  ulp_riscv_gpio_output_disable(SCL);
  ulp_riscv_gpio_output_disable(SDA);
  ulp_riscv_gpio_deinit(SCL);
  ulp_riscv_gpio_deinit(SDA);
}

void set_scl(uint8_t b) { ulp_riscv_gpio_output_level(SCL, b); }
void set_sda(uint8_t b) { ulp_riscv_gpio_output_level(SDA, b); }

void i2c_delay(void) { sensirion_i2c_hal_sleep_usec(10); }

void i2c_start_condition(void) {
  set_scl(1);
  set_sda(0);
  i2c_delay();
  set_scl(0);
  i2c_delay();
}

void i2c_transfer_bit(uint8_t bit) {
  set_sda(bit);
  i2c_delay();
  set_scl(1);
  i2c_delay();
  set_scl(0);
  i2c_delay();
}

void i2c_transfer_byte(uint8_t byte) {
  for (uint8_t i = 0; i < 8; ++i) {
    // transfer individual bits, beginning with the MSB
    i2c_transfer_bit((byte >> (8 - i - 1)) & 1);
  }

  // accept slave acknowledge
  ulp_riscv_gpio_output_disable(SDA);
  i2c_delay();
  set_scl(1);
  i2c_delay();
  set_scl(0);
  i2c_delay();
  ulp_riscv_gpio_output_enable(SDA);
}

uint8_t receive_bit(void) {
  set_scl(1);
  i2c_delay();
  uint8_t result = ulp_riscv_gpio_get_level(SDA);
  set_scl(0);
  i2c_delay();
  return result;
}

uint8_t i2c_receive_byte(bool acknowledge) {
  ulp_riscv_gpio_output_disable(SDA);
  ulp_riscv_gpio_input_enable(SDA);

  uint8_t result = receive_bit();
  for (uint8_t i = 0; i < 7; ++i) {
    result <<= 1;
    result |= receive_bit();
  }

  ulp_riscv_gpio_input_disable(SDA);
  ulp_riscv_gpio_output_enable(SDA);

  i2c_transfer_bit(!acknowledge);

  return result;
}

void i2c_stop_condition(void) {
  set_sda(0);
  i2c_delay();
  set_scl(1);
  i2c_delay();
  set_sda(1);
}

/**
 * Execute one read transaction on the I2C bus, reading a given number of bytes.
 * If the device does not acknowledge the read command, an error shall be
 * returned.
 *
 * @param address 7-bit I2C address to read from
 * @param data    pointer to the buffer where the data is to be stored
 * @param count   number of bytes to read from I2C and store in the buffer
 * @returns 0 on success, error code otherwise
 */
int8_t sensirion_i2c_hal_read(uint8_t address, uint8_t* data, uint16_t count) {
  i2c_start_condition();
  // transfer address, setting the lowest bit to indicate a "read" transfer
  i2c_transfer_byte((address << 1) | 0b1);
  for (uint16_t i = 0; i < count; ++i) {
    data[i] = i2c_receive_byte(i != count - 1);
  }
  i2c_stop_condition();
  return 0;
}

/**
 * Execute one write transaction on the I2C bus, sending a given number of
 * bytes. The bytes in the supplied buffer must be sent to the given address. If
 * the slave device does not acknowledge any of the bytes, an error shall be
 * returned.
 *
 * @param address 7-bit I2C address to write to
 * @param data    pointer to the buffer containing the data to write
 * @param count   number of bytes to read from the buffer and send over I2C
 * @returns 0 on success, error code otherwise
 */
int8_t sensirion_i2c_hal_write(uint8_t address, uint8_t const* data,
                               uint16_t count) {
  i2c_start_condition();
  // transfer address, unsetting the lowest bit to indicate a "write" transfer
  i2c_transfer_byte(address << 1);
  for (uint16_t i = 0; i < count; ++i) {
    i2c_transfer_byte(data[i]);
  }
  i2c_stop_condition();
  return 0;
}

/**
 * Sleep for a given number of microseconds. The function should delay the
 * execution for at least the given time, but may also sleep longer.
 *
 * Despite the unit, a <10 millisecond precision is sufficient.
 *
 * @param useconds the sleep time in microseconds
 */
void sensirion_i2c_hal_sleep_usec(uint32_t useconds) {
  ulp_riscv_delay_cycles(ULP_RISCV_CYCLES_PER_US * useconds);
}
