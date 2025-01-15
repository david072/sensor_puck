idf_component_register(
  SRCS
    "scd41.cpp" "sensirion_i2c_hal.cpp"
    "embedded_i2c_scd4x/scd4x_i2c.c" "embedded_i2c_scd4x/sensirion_common.c" "embedded_i2c_scd4x/sensirion_i2c.c"
  INCLUDE_DIRS "." "embedded_i2c_scd4x"
  PRIV_REQUIRES util esp_driver_i2c
)
