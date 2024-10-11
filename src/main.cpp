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

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);

  lv_init();
  lv_xiao_disp_init();
  lv_xiao_touch_init();

  lv_obj_set_style_bg_color(lv_scr_act(), BACKGROUND_COLOR, 0);
  lv_obj_set_style_text_color(lv_scr_act(), lv_color_white(), 0);
  lv_obj_set_style_border_width(lv_scr_act(), 0, 0);
  lv_obj_set_scroll_dir(lv_scr_act(), LV_DIR_NONE);
  lv_obj_set_style_pad_all(lv_scr_act(), 0, 0);

  g_ui_container = lv_obj_create(lv_scr_act());
  lv_obj_set_layout(g_ui_container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(g_ui_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(g_ui_container, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_size(g_ui_container, lv_pct(1000),
                  lv_obj_get_height(lv_scr_act()) * 1);
  lv_obj_set_style_bg_opa(g_ui_container, 0, 0);
  lv_obj_set_style_border_width(g_ui_container, 0, 0);
  lv_obj_set_scroll_dir(g_ui_container, LV_DIR_VER);
  lv_obj_set_style_pad_all(g_ui_container, 0, 0);
  lv_obj_set_style_margin_all(g_ui_container, 0, 0);
  lv_obj_set_scroll_snap_y(g_ui_container, LV_SCROLL_SNAP_START);
  lv_obj_add_flag(g_ui_container, LV_OBJ_FLAG_SCROLL_ONE);

  g_pages.push_back(std::make_unique<ui::Page>(ui::ClockPage(g_ui_container)));
  g_pages.push_back(
      std::make_unique<ui::Page>(ui::AirQualityPage(g_ui_container)));
}

void loop() {
  lv_tick_inc(millis() - g_last_ui_update);
  g_last_ui_update = millis();
  lv_timer_handler();
}
