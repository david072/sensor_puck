execute_process(
  COMMAND "${CMAKE_CURRENT_LIST_DIR}/generate_fonts.sh"
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE font_gen_result
)
# turn multiline output into CMake list
string(REPLACE "\n" ";" fonts_generated ${font_gen_result})
list(APPEND sources ${fonts_generated})

idf_component_register(
  SRCS ${sources}
  INCLUDE_DIRS "."
  PRIV_REQUIRES lvgl
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
