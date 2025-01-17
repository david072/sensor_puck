cmake_minimum_required(VERSION 3.20)

project(ulp_riscv_app)
add_executable(
  ulp_riscv_app
  "main.c" "sensirion_i2c_hal.c"
  "../../components/scd41/embedded_i2c_scd4x/scd4x_i2c.c"
  "../../components/scd41/embedded_i2c_scd4x/sensirion_common.c"
  "../../components/scd41/embedded_i2c_scd4x/sensirion_i2c.c"
)

include(IDFULPProject)
ulp_apply_default_options(ulp_riscv_app)
ulp_apply_default_sources(ulp_riscv_app)
ulp_add_build_binary_targets(ulp_riscv_app)

target_include_directories(
  ulp_riscv_app PRIVATE
  "."
  "../../components/scd41/embedded_i2c_scd4x"
)
