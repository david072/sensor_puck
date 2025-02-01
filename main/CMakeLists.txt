idf_component_register(
  SRCS
    "sensor_puck.cpp" "display_driver.cpp" "battery.cpp"
    "data.cpp" "ble_peripheral_manager.cpp" "wifi_manager.cpp"
    "ui/ui.cpp" "ui/pages.cpp"
    "ui/fonts/font_montagu_slab_40.c" "ui/fonts/font_montagu_slab_32_bold.c" "ui/fonts/font_montagu_slab_24.c" "ui/fonts/font_montagu_slab_20.c" "ui/fonts/font_montagu_slab_16.c"
  INCLUDE_DIRS "."
)

ulp_add_project("ulp_riscv_app" "${CMAKE_SOURCE_DIR}/main/ulp_riscv/")
