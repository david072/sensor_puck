#pragma once

#include "lis2mdl_pid/lis2mdl_reg.h"
#include <driver/i2c_master.h>
#include <types.h>

class Lis2mdl {
public:
  static constexpr u32 I2C_TIMEOUT_MS = 50;
  static constexpr u16 DEFAULT_ADDRESS = 0x1E;

  enum class DataRate : u8 {
    Rate10Hz = LIS2MDL_ODR_10Hz,
    Rate20Hz = LIS2MDL_ODR_20Hz,
    Rate50Hz = LIS2MDL_ODR_50Hz,
    Rate100Hz = LIS2MDL_ODR_100Hz,
  };

  struct Data {
    /// Magnetic field strength in Gauss.
    float x, y, z;
  };

  static int32_t platform_write(void* handle, uint8_t reg, uint8_t const* bufp,
                                uint16_t len);
  static int32_t platform_read(void* handle, uint8_t reg, uint8_t* bufp,
                               uint16_t len);
  static void platform_delay(uint32_t millisec);

  Lis2mdl(i2c_master_bus_handle_t i2c_handle, u16 address = DEFAULT_ADDRESS);

  void set_data_rate(DataRate rate);

  std::optional<Data> read_sensor();

  void power_down();

private:
  stmdev_ctx_t m_device;
};
