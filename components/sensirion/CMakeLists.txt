idf_component_register(
  SRCS
    "scd41.cpp" "sgp41.cpp"
    "sensirion_i2c_hal.cpp"
    "embedded_i2c_scd4x/scd4x_i2c.c" "embedded_i2c_scd4x/sensirion_common.c" "embedded_i2c_scd4x/sensirion_i2c.c"
    "embedded_i2c_sgp41/sgp41_i2c.c"
    "gas_index_algorithm/sensirion_gas_index_algorithm/sensirion_gas_index_algorithm.c"
  INCLUDE_DIRS "." "embedded_i2c_scd4x" "embedded_i2c_sgp41" "gas_index_algorithm/sensirion_gas_index_algorithm"
  PRIV_REQUIRES util esp_driver_i2c
)
