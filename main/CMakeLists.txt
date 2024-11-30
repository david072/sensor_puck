idf_component_register(
  SRCS
    "sensor_puck.cpp" "display_driver.cpp"
    "data.cpp"
    "ui/ui.cpp" "ui/pages.cpp"
    "sensors/bme688.cpp" "sensors/bme688/bme68x.c"
  INCLUDE_DIRS "."
)
