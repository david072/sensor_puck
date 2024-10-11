#include "ui.h"
#include <Arduino.h>
#include <memory>

#define USE_ARDUINO_GFX_LIBRARY
#include "lv_xiao_round_screen.h"

#include <I2C_BM8563.h>
#include <lvgl.h>

constexpr lv_color_t BACKGROUND_COLOR = make_color(0x1a, 0x1a, 0x1a);

std::vector<std::unique_ptr<ui::Page>> g_pages = {};
lv_obj_t* g_ui_container;

I2C_BM8563 g_rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);

unsigned long g_last_ui_update = 0;

lv_obj_t* snapping_flex_container(lv_obj_t* parent = lv_scr_act()) {
  auto* cont = lv_obj_create(parent);
  lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_bg_opa(cont, 0, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_margin_all(cont, 0, 0);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLL_ONE);
  lv_obj_set_size(cont, lv_obj_get_width(lv_scr_act()),
                  lv_obj_get_height(lv_scr_act()));
  return cont;
}

template <typename... Page>
void add_grouped_pages() {
  auto* container = snapping_flex_container(g_ui_container);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
  lv_obj_set_scroll_dir(container, LV_DIR_HOR);
  lv_obj_set_scroll_snap_x(container, LV_SCROLL_SNAP_START);

  ([&] { g_pages.push_back(std::make_unique<ui::Page>(Page(container))); }(),
   ...);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  esp_log_level_set("*", ESP_LOG_DEBUG);

  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();

  lv_obj_set_style_bg_color(lv_scr_act(), BACKGROUND_COLOR, 0);
  lv_obj_set_style_text_color(lv_scr_act(), lv_color_white(), 0);
  lv_obj_set_style_border_width(lv_scr_act(), 0, 0);
  lv_obj_set_scroll_dir(lv_scr_act(), LV_DIR_NONE);
  lv_obj_set_style_pad_all(lv_scr_act(), 0, 0);

  g_ui_container = snapping_flex_container();
  lv_obj_set_flex_flow(g_ui_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(g_ui_container, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(g_ui_container, LV_SCROLL_SNAP_START);

  add_grouped_pages<ui::ClockPage, ui::TimerPage>();
  add_grouped_pages<ui::AirQualityPage>();
  add_grouped_pages<ui::CompassPage>();
}

void loop() {
  lv_tick_inc(millis() - g_last_ui_update);
  g_last_ui_update = millis();
  lv_timer_handler();
}
