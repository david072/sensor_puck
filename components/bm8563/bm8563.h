#pragma once

#include <driver/i2c_master.h>
#include <types.h>

class Bm8563 {
public:
  static constexpr u16 DEFAULT_ADDRESS = 0b1010001;
  static constexpr u32 I2C_TIMEOUT_MS = 50;

  struct DateTime {
    /// Range: 1900-2099
    int year;
    /// Range: 1-12
    int month;
    /// Range: 0-6
    int weekday;
    /// Range: 1-31
    int day;
    /// Range: 0-23
    int hour;
    /// Range: 0-59
    int minute;
    /// Range: 0-59
    int second;
  };

  Bm8563(i2c_master_bus_handle_t i2c_handle, u16 address = DEFAULT_ADDRESS);

  DateTime read_date_time() const;
  void set_date_time(DateTime dt) const;

private:
  static constexpr u8 SECONDS_REGISTER = 0x02;
  static constexpr u8 CENTURY_MASK = 0b10000000;
  static constexpr u8 MONTH_MASK = 0b00011111;
  static constexpr u8 WEEKDAY_MASK = 0b00000111;
  static constexpr u8 DAY_MASK = 0b00111111;
  static constexpr u8 HOUR_MASK = 0b00111111;
  static constexpr u8 MINUTE_MASK = 0b01111111;
  static constexpr u8 SECOND_MASK = 0b01111111;

  void write(u8 reg, u8 const* data, u32 length) const;
  void read(u8 reg, u8* data, u32 length) const;

  i2c_master_dev_handle_t m_device;
};
