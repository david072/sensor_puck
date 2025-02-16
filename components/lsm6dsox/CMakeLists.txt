idf_component_register(
  SRCS
    "lsm6dsox.cpp"
    "lsm6dsox_pid/lsm6dsox_reg.c"
  INCLUDE_DIRS "."
  PRIV_REQUIRES esp_driver_i2c util
)
