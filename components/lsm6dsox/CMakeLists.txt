idf_component_register(
  SRCS "lsm6dsox.cpp"
  INCLUDE_DIRS "."
  PRIV_REQUIRES esp_driver_i2c util
)
