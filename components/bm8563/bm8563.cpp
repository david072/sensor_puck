#include "bm8563.h"
#include <cstring>

u8 bcd_to_dec(u8 bcd) { return (bcd >> 4) * 10 + (bcd & 0x0f); }

u8 dec_to_bcd(u8 dec) { return ((dec / 10) << 4) | (dec % 10); }

Bm8563::Bm8563(i2c_master_bus_handle_t i2c_handle, u16 address) {
  i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &dev, &m_device));

  u8 data[2] = {0};
  write(0x00, data, 2);
}

Bm8563::DateTime Bm8563::read_date_time() const {
  u8 buf[7];
  read(SECONDS_REGISTER, buf, sizeof(buf));

  return DateTime{
      .year = bcd_to_dec(buf[6]) + (buf[5] & CENTURY_MASK ? 1900 : 2000),
      .month = bcd_to_dec(buf[5] & MONTH_MASK),
      .weekday = bcd_to_dec(buf[4] & WEEKDAY_MASK),
      .day = bcd_to_dec(buf[3] & DAY_MASK),
      .hour = bcd_to_dec(buf[2] & HOUR_MASK),
      .minute = bcd_to_dec(buf[1] & MINUTE_MASK),
      .second = bcd_to_dec(buf[0] & SECOND_MASK),
  };
}

void Bm8563::set_date_time(DateTime dt) const {
  u8 values[7];
  read(SECONDS_REGISTER, values, sizeof(values));

  u8 buf[7] = {
      dec_to_bcd((dt.second & SECOND_MASK) | (values[0] & ~SECOND_MASK)),
      dec_to_bcd(dt.minute),
      dec_to_bcd(dt.hour),
      dec_to_bcd(dt.day),
      values[4],
      static_cast<u8>(((dt.year >= 2000 ? 0 : 1) << 7) | dec_to_bcd(dt.month)),
      dec_to_bcd(dt.year % 100),
  };
  write(SECONDS_REGISTER, buf, sizeof(buf));
}

void Bm8563::read(u8 reg, u8* data, u32 length) const {
  ESP_ERROR_CHECK(i2c_master_transmit_receive(m_device, &reg, 1, data, length,
                                              I2C_TIMEOUT_MS));
}

void Bm8563::write(u8 reg, u8 const* data, u32 length) const {
  u8 buf[64];
  buf[0] = reg;
  memcpy(buf + 1, data, length);
  ESP_ERROR_CHECK(
      i2c_master_transmit(m_device, buf, length + 1, I2C_TIMEOUT_MS));
}
