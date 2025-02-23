idf_component_register(
  SRCS "lis2mdl.cpp" "lis2mdl_pid/lis2mdl_reg.c"
  INCLUDE_DIRS "."
  PRIV_REQUIRES esp_driver_i2c util
)
