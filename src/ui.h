#pragma once

#include <lvgl.h>

constexpr lv_color_t make_color(uint8_t r, uint8_t g, uint8_t b) {
  return {.blue = b, .green = g, .red = r};
}

constexpr lv_color_t make_color(uint8_t v) { return make_color(v, v, v); }

namespace ui {

lv_event_code_t register_lv_event_id();

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
  lv_style_t const* medium_text() const { return &m_medium_text; }
  lv_style_t const* body_text() const { return &m_body_text; }
  lv_style_t const* caption1() const { return &m_caption1; }

  lv_event_code_t enter_fullscreen_event() const {
    return m_enter_fullscreen_event;
  }
  lv_event_code_t exit_fullscreen_event() const {
    return m_exit_fullscreen_event;
  }

private:
  Style();

  lv_style_t m_container;
  lv_style_t m_page_container;
  lv_style_t m_divider;

  lv_style_t m_large_text;
  lv_style_t m_medium_text;
  lv_style_t m_body_text;
  lv_style_t m_caption1;

  lv_event_code_t m_enter_fullscreen_event;
  lv_event_code_t m_exit_fullscreen_event;
};

lv_obj_t* flex_container(lv_obj_t* parent = nullptr);
lv_obj_t* large_text(lv_obj_t* parent = nullptr);
lv_obj_t* body_text(lv_obj_t* parent = nullptr);
lv_obj_t* caption1(lv_obj_t* parent = nullptr);
lv_obj_t* divider(lv_obj_t* parent = nullptr);

class Page {
public:
  explicit Page(lv_obj_t* parent, uint32_t update_interval = 0);
  ~Page();

  lv_obj_t* page_container() const { return m_container; }

protected:
  virtual void update() {}

private:
  lv_obj_t* m_container;
  lv_timer_t* m_update_timer;
};

class ClockPage : public Page {
public:
  explicit ClockPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  static constexpr uint32_t UPDATE_INTERVAL_MS = 1000;

  lv_obj_t* m_time;
  lv_obj_t* m_battery_text;

  unsigned long m_press_start = 0;
  lv_point_t m_press_start_point;
  bool m_in_fullscreen = false;

  int m_hour = 13;
  int m_minute = 30;
};

class TimerPage : public Page {
public:
  explicit TimerPage(lv_obj_t* parent);

protected:
  void update() override;
};

class AirQualityPage : public Page {
public:
  explicit AirQualityPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  lv_obj_t* m_container;
  lv_obj_t* m_co2_ppm;
  lv_obj_t* m_temperature;
  lv_obj_t* m_warning_circle;
};

class CompassPage : public Page {
public:
  explicit CompassPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  lv_obj_t* m_cardinal_direction;
  lv_obj_t* m_angle;

  lv_obj_t* m_north;
  lv_obj_t* m_south;
  lv_obj_t* m_east;
  lv_obj_t* m_west;
};

class RotaryInputPage : public Page {
public:
  explicit RotaryInputPage(lv_obj_t* parent, int& value);
  ~RotaryInputPage();

protected:
  void update() override;

private:
  static constexpr float MIN_ROTATION_THRESHOLD = 1.0;
  static constexpr int32_t BUTTON_OFFSET = 50;

  unsigned long m_last_update = 0;
  float m_angle = 0;

  int m_value = 0;
  int& m_target_value;

  lv_obj_t* m_text;
  lv_obj_t* m_confirm_button;
  lv_obj_t* m_cancel_button;
};

} // namespace ui
