#pragma once

#include <driver/i2c_master.h>
#include <optional>
#include <types.h>

class Scd41 {
public:
  static constexpr u16 DEFAULT_ADDRESS = 0x62;
  static constexpr u32 I2C_TIMEOUT_MS = 50;

  Scd41(i2c_master_bus_handle_t i2c_handle, u16 address = DEFAULT_ADDRESS);

  void start_low_power_periodic_measurement();
  std::optional<u16> read_co2();
  /// Stops the period measurement (both normal and low power).
  ///
  /// NOTE: The sensor will only respond to other commands *500ms* after this
  ///  command has been issued (see datasheet section 3.6.3)!
  void stop_periodic_measurement();

private:
  // SCD4x datasheet:
  // https://sensirion.com/media/documents/48C4B7FB/66E05452/CD_DS_SCD4x_Datasheet_D1.pdf

  static constexpr u8 CRC8_POLYNOMIAL = 0x31;
  static constexpr u8 CRC8_INIT = 0xFF;

  static constexpr u16 START_LOW_POWER_PERIODIC_MEASUREMENT_ADDR = 0x21AC;
  static constexpr u16 START_PERIODIC_MEASUREMENT_ADDR = 0x21B1;
  static constexpr u16 READ_MEASUREMENT_ADDR = 0xEC05;
  static constexpr u16 STOP_PERIODIC_MEASUREMENT_ADDR = 0x3F86;
  static constexpr u16 GET_READY_STATUS_ADDR = 0xE4B8;

  void write_read(u16 address, u16* data = nullptr, size_t data_len = 0,
                  u16* response = nullptr, size_t response_len = 0);

  /// See datasheet section 3.12
  u8 sensirion_common_generate_crc(u8* data, u16 len);

  i2c_master_dev_handle_t m_device;
};
