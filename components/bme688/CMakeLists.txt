idf_component_register(
  SRCS "bme688.cpp" "bme68x/bme68x.c"
  INCLUDE_DIRS "."
  PRIV_REQUIRES esp_driver_i2c util
)
