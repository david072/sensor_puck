idf_component_register(
  SRCS "st25dv.cpp"
  INCLUDE_DIRS "."
  PRIV_REQUIRES esp_driver_i2c util
)
