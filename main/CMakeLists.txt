idf_component_register(
  SRCS
    "sensor_puck.cpp" "display_driver.cpp" "battery.cpp"
    "data.cpp" "preferences.cpp" "ble_peripheral_manager.cpp" "wifi_manager.cpp"
    "ui/ui.cpp" "ui/pages.cpp" "ui/timer_page.cpp"
  INCLUDE_DIRS "."
)
