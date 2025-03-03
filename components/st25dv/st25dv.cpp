#include "st25dv.h"
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

namespace nfc {

struct NdefHeaderByte {
  /// Message begin flag.
  bool mb : 1 = false;
  /// Message end flag.
  bool me : 1 = false;
  /// Chunk flag.
  bool cf : 1 = false;
  /// Short record flag.
  bool sr : 1 = false;
  /// ID and ID length fields present flag.
  bool il : 1 = false;
  /// Type Name Format
  Tnf tnf : 3;
};

u8 ndef_record_header_byte(NdefHeaderByte h) {
  return h.mb << 7 | h.me << 6 | h.cf << 5 | h.sr << 4 | h.il << 3 |
         static_cast<u8>(h.tnf);
}

// URI record anatomy:
// [
//   0x03 - no idea what this byte means
//   length of the entire record (all bytes after this one)
//   header byte: bitfield consisting of the following fields
//     MB - message begin flag
//     ME - message end flag
//     CF - chunk flag
//     SR - short record flag (if set, only one byte is used for the payload
//          length)
//     IL - ID & ID length fields present flag (if set, the record
//          contains an ID and an ID length byte)
//     TNF (3 bits) - type name format, for a URI record this is 0x01 (NFC
//                    Forum well-known type)
//   type length
//   payload length (4 or 1 byte depending on SR)
//   type - for a URI record, the type is 'U' (0x55)
//   payload
//     prefix byte (e.g. 0x01 for "https://www.")
//     rest of the URI
// ]
NdefRecord build_ndef_uri_record(UriPrefix prefix, std::string const& uri) {
  // uri length + prefix length
  u32 payload_length = uri.length() + 1;
  // 7 extra bytes, i.e. header, type length, payload length (4 bytes),
  // well-known type
  auto record_length = payload_length + 7;
  // 2 extra bytes, i.e. 0x03 and the entire length of the record
  auto buffer_length = record_length + 2;
  u8* record = static_cast<u8*>(malloc(buffer_length));
  record[0] = 0x03; // I don't know what this byte means...
  record[1] = record_length;
  record[2] = ndef_record_header_byte(NdefHeaderByte{
      .mb = true,
      .me = true,
      .sr = false,
      .tnf = Tnf::NfcForumWellKnown,
  });
  record[3] = 0x01; // Type length (in bytes)
  // payload length as u32
  record[4] = (payload_length >> 24) & 0xFF;
  record[5] = (payload_length >> 16) & 0xFF;
  record[6] = (payload_length >> 8) & 0xFF;
  record[7] = (payload_length >> 0) & 0xFF;
  record[8] = 'U'; // Type - the well-known type for a URI record is 'U' (0x55)

  record[9] = static_cast<u8>(prefix);
  memcpy(&record[10], uri.data(), uri.length());

  return NdefRecord{
      .data = record,
      .length = buffer_length,
  };
}

void free_ndef_record(NdefRecord record) { free(record.data); }

} // namespace nfc

void add_device(i2c_master_bus_handle_t i2c_handle, u16 address,
                i2c_master_dev_handle_t* handle) {
  i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &dev, handle));
}

St25dv16kc::St25dv16kc(i2c_master_bus_handle_t i2c_handle) {
  add_device(i2c_handle, USER_MEMORY_ADDRESS, &m_user_device);
  add_device(i2c_handle, SYSTEM_MEMORY_ADDRESS, &m_system_device);

  write(m_user_device, 0x00, CC_FILE, sizeof(CC_FILE));
  vTaskDelay(pdMS_TO_TICKS(50));

  // Execute present password command
  // (https://www.st.com/resource/en/datasheet/st25dv04kc.pdf, section 6.6.1),
  // i.e. write password, validation code 0x9, and the password again.
  // By default, the password is 0.
  u8 password[8 * 2 + 1] = {0};
  password[8] = 0x9;
  write(m_system_device, REG_SECURITY_PASSWORD_START, password, 17);

  // u8 value = 0;
  // read(m_user_device, REG_SECURITY_SESSION_STATUS, &value, 1);
  // ESP_LOGI("ST25DV", "I2C session started: %s", value & 1 ? "yes :O" : "no
  // D:");
}

void St25dv16kc::write_ndef_record(nfc::NdefRecord record) {
  write(m_user_device, sizeof(CC_FILE), record.data, record.length);
  vTaskDelay(pdMS_TO_TICKS(50));
}

void St25dv16kc::write(i2c_master_dev_handle_t device, u16 address,
                       u8 const* data, u16 length) {
  u8 buf[length + 2];
  buf[0] = address >> 8;
  buf[1] = address & 0xFF;
  memcpy(&buf[2], data, length);

  ESP_ERROR_CHECK(
      i2c_master_transmit(device, buf, sizeof(buf), I2C_TIMEOUT_MS));
}

void St25dv16kc::read(i2c_master_dev_handle_t device, u16 address, u8* data,
                      u16 length) {
  u8 buf[2];
  buf[0] = address >> 8;
  buf[1] = address & 0xFF;
  ESP_ERROR_CHECK(i2c_master_transmit_receive(device, buf, sizeof(buf), data,
                                              length, I2C_TIMEOUT_MS));
}
