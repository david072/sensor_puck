#pragma once

#include <driver/i2c_master.h>
#include <string>
#include <types.h>

namespace nfc {

enum class UriPrefix : u8 {
  HttpsWww = 0x01,
  Https = 0x04,
};
enum class Tnf : u8 { NfcForumWellKnown = 0x01 };

struct NdefRecord {
  u8* data;
  size_t length;
};

NdefRecord build_ndef_uri_record(UriPrefix prefix, std::string const& uri);
void free_ndef_record(NdefRecord record);

} // namespace nfc

namespace st25dv {

static constexpr u8 DEVICE_SELECT_CODE = 0b1010;
static constexpr u8 I2C_E0 = 0b1;

/// Returns the I2C address of the device for the given E2 and E1 values.
///
/// See https://www.st.com/resource/en/datasheet/st25dv04kc.pdf, section 6.3
static constexpr u16 device_address(bool e2, bool e1) {
  return (DEVICE_SELECT_CODE << 3) | e2 << 2 | e1 << 1 | I2C_E0;
}

} // namespace st25dv

class St25dv16kc {
public:
  static constexpr u32 I2C_TIMEOUT_MS = 50;

  St25dv16kc(i2c_master_bus_handle_t i2c_handle);

  void write_ndef_record(nfc::NdefRecord record);

private:
  static constexpr u16 USER_MEMORY_ADDRESS = st25dv::device_address(0, 1);
  static constexpr u16 SYSTEM_MEMORY_ADDRESS = st25dv::device_address(1, 1);
  static constexpr u16 RF_ON_ADDRESS = st25dv::device_address(0, 0);
  static constexpr u16 RF_OFF_ADDRESS = st25dv::device_address(1, 0);

  static constexpr size_t MAX_WRITE_LENGTH = 255;

  static constexpr u16 REG_SECURITY_PASSWORD_START = 0x0900;
  static constexpr u16 REG_SECURITY_SESSION_STATUS = 0x2004;

  static constexpr u8 CC_FILE[] = {
      0xE2,       // magic number
      0b01000000, // version 1.0, RW access
      0x00,       // always 0x00
      0b00000000, // additional info
      0x00,       // RFU
      0x00,       // RFU
      0x00,       // memory size
      0xFF,       // memory size
  };

  void write(i2c_master_dev_handle_t device, u16 address, u8 const* data,
             u16 length);
  void read(i2c_master_dev_handle_t device, u16 address, u8* data, u16 length);

  i2c_master_dev_handle_t m_user_device;
  i2c_master_dev_handle_t m_system_device;
};
