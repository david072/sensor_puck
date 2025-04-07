#pragma once

#include <driver/i2c_master.h>
#include <optional>
#include <sensirion_gas_index_algorithm.h>
#include <types.h>

class Sgp41 {
public:
  static constexpr u16 DEFAULT_ADDRESS = 0x59;
  static constexpr u32 I2C_TIMEOUT_MS = 50;

  struct Data {
    u16 voc_index = 0;
    u16 nox_index = 0;
  };

  class GasIndexAlgorithm {
  public:
    void initialize(float sampling_interval_s);

    Data process(u16 sraw_voc, u16 sraw_nox);

    void reset();

  private:
    GasIndexAlgorithmParams m_voc_params;
    GasIndexAlgorithmParams m_nox_params;
  };

  Sgp41(i2c_master_bus_handle_t i2c_handle, u16 address = DEFAULT_ADDRESS);

  /// Performs sensor conditioning. This will execute the
  /// sgp41_execute_conditioning command every second for 10s.
  /// (see datasheet 3.1 or Sensirion examples for more details)
  ///
  /// NOTE: This takes 10 seconds!
  void perform_conditioning(float temperature, float humidity);

  std::optional<Data> read(float temperature, float humidity,
                           GasIndexAlgorithm& gia);

  void turn_heater_off();

private:
  u16 compensation_t(float temperature);
  u16 compensation_rh(float humidity);
};
