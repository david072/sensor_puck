idf_component_register(
  SRCS "bm8563.cpp"
  INCLUDE_DIRS "."
  PRIV_REQUIRES util esp_driver_i2c
)
