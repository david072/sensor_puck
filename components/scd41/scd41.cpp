#include "scd41.h"
#include <freertos/FreeRTOS.h>

Scd41::Scd41(i2c_master_bus_handle_t i2c_handle, u16 address) {
  i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 100000,
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &dev, &m_device));
}

void Scd41::start_low_power_periodic_measurement() {
  write_read(START_PERIODIC_MEASUREMENT_ADDR);
}

std::optional<u16> Scd41::read_co2() {
  u16 ready_status;
  write_read(GET_READY_STATUS_ADDR, NULL, 0, &ready_status, 1);
  // From the datasheet section 3.9.2:
  // If the least significant 11 bits of word[0] are:
  //  - 0 -> data not ready
  if (ready_status << 5 == 0) {
    return std::nullopt;
  }
  // - else -> data ready

  u16 co2_reading;
  write_read(READ_MEASUREMENT_ADDR, NULL, 0, &co2_reading, 1);
  return co2_reading;
}

void Scd41::stop_periodic_measurement() {
  write_read(STOP_PERIODIC_MEASUREMENT_ADDR);
}

void Scd41::write_read(u16 address, u16* data, size_t data_len, u16* response,
                       size_t response_len) {
  // command, then each word in data as MSB, LSB, CRC
  u8 write_buf[2 + data_len * 3];
  write_buf[0] = address >> 8;
  write_buf[1] = address & 0xFF;
  if (data) {
    size_t i = 2;
    for (size_t j = 0; j < data_len; ++j) {
      write_buf[i] = data[j] >> 8;
      write_buf[i + 1] = data[j] & 0xFF;
      write_buf[i + 2] = sensirion_common_generate_crc(&write_buf[i], 2);
      i += 3;
    }
  }

  // NOTE: It seems like the sensor doesn't properly acknowledge the address for
  //  like the first few writes. After that, everything _seems_ to be working
  //  properly though...
  if (!response) {
    i2c_master_transmit(m_device, write_buf, sizeof(write_buf), I2C_TIMEOUT_MS);
  } else {
    // for each word in response: MSB, LSB, CRC
    u8 read_buf[response_len * 3];
    i2c_master_transmit(m_device, write_buf, sizeof(write_buf), I2C_TIMEOUT_MS);
    i2c_master_receive(m_device, read_buf, sizeof(read_buf), I2C_TIMEOUT_MS);
    size_t i = 0;
    for (size_t j = 0; j < response_len; ++j) {
      response[j] = (read_buf[i] << 8) | read_buf[i + 1];
      i += 3;
    }
  }
}

u8 Scd41::sensirion_common_generate_crc(u8* data, u16 len) {
  u8 crc = CRC8_INIT;
  /* calculates 8-Bit checksum with given polynomial */
  for (u16 current_byte = 0; current_byte < len; ++current_byte) {
    crc ^= (data[current_byte]);
    for (u8 crc_bit = 8; crc_bit > 0; --crc_bit) {
      if (crc & 0x80)
        crc = (crc << 1) ^ CRC8_POLYNOMIAL;
      else
        crc = (crc << 1);
    }
  }
  return crc;
}
