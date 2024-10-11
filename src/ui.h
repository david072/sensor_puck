#pragma once

#include "data.h"
#include <lvgl.h>

constexpr lv_color_t make_color(uint8_t r, uint8_t g, uint8_t b) {
  return {.blue = b, .green = g, .red = r};
}

constexpr lv_color_t make_color(uint8_t v) { return make_color(v, v, v); }

namespace ui {

class Style {
public:
  static constexpr lv_color_t ACCENT_COLOR = make_color(0x62, 0x85, 0xF6);
  static constexpr lv_color_t CAPTION_COLOR = make_color(0x81);
  static constexpr lv_color_t ERROR_COLOR = make_color(0xEB, 0x2D, 0x2D);

  static Style const& the();

  lv_style_t const* container() const { return &m_container; }
  lv_style_t const* page_container() const { return &m_page_container; }
  lv_style_t const* divider() const { return &m_divider; }

  lv_style_t const* large_text() const { return &m_large_text; }
  lv_style_t const* body_text() const { return &m_body_text; }
  lv_style_t const* caption1() const { return &m_caption1; }

private:
  Style();

  lv_style_t m_container;
  lv_style_t m_page_container;
  lv_style_t m_divider;

  lv_style_t m_large_text;
  lv_style_t m_body_text;
  lv_style_t m_caption1;
};

lv_obj_t* flex_container(lv_obj_t* parent = nullptr);
lv_obj_t* large_text(lv_obj_t* parent = nullptr);
lv_obj_t* body_text(lv_obj_t* parent = nullptr);
lv_obj_t* caption1(lv_obj_t* parent = nullptr);
lv_obj_t* divider(lv_obj_t* parent = nullptr);

class Page {
public:
  explicit Page(lv_obj_t* parent);

  virtual ~Page() {}

  virtual void update(Data const& data) {}

  lv_obj_t* page_container() const { return m_container; }

private:
  lv_obj_t* m_container;
};

class ClockPage : public Page {
public:
  explicit ClockPage(lv_obj_t* parent);

  void update(Data const& data) override;

private:
  lv_obj_t* m_time;
  lv_obj_t* m_battery_text;
};

class AirQualityPage : public Page {
public:
  explicit AirQualityPage(lv_obj_t* parent);

  void update(Data const& data) override;

private:
  lv_obj_t* m_container;
  lv_obj_t* m_co2_ppm;
  lv_obj_t* m_temperature;
  lv_obj_t* m_warning_circle;
};

} // namespace ui
