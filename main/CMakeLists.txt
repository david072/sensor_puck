idf_component_register(
  SRCS
    "sensor_puck.cpp" "display_driver.cpp" "battery.cpp"
    "data.cpp" "preferences.cpp" "ble_peripheral_manager.cpp" "wifi_manager.cpp"
    "ui/ui.cpp" "ui/pages.cpp"
  INCLUDE_DIRS "."
)

ulp_add_project("ulp_riscv_app" "${CMAKE_SOURCE_DIR}/main/ulp_riscv/")
