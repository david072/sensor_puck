#pragma once

#include <functional>
#include <lvgl.h>
#include <types.h>
#include <vector>

constexpr lv_color_t make_color(uint8_t r, uint8_t g, uint8_t b) {
  return {.blue = b, .green = g, .red = r};
}

constexpr lv_color_t make_color(uint8_t v) { return make_color(v, v, v); }

LV_FONT_DECLARE(font_montagu_slab_40);
LV_FONT_DECLARE(font_montagu_slab_32_bold);
LV_FONT_DECLARE(font_montagu_slab_24);
LV_FONT_DECLARE(font_montagu_slab_20);
LV_FONT_DECLARE(font_montagu_slab_16);

// Unicodes from fontawesome.com converted to bytes using
// https://www.cogsci.ed.ac.uk/~richard/utf-8.cgi
#define SYMBOL_ARROW_LEFT "\xEF\x81\xA0" // 0xF060
#define SYMBOL_CLOCK "\xEF\x80\x97"      // 0xF017
#define SYMBOL_CALENDAR "\xEF\x84\xB3"   // 0xF133

namespace ui {

constexpr unsigned long LONG_PRESS_DURATION_MS = 300;

lv_event_code_t register_lv_event_id();
lv_point_t get_last_touch_point();

template <typename T>
T* get_event_user_data(lv_event_t* event) {
  return static_cast<T*>(lv_event_get_user_data(event));
}

class Page {
public:
  explicit Page(lv_obj_t* parent = NULL, u32 update_interval = 0);
  virtual ~Page();

  lv_obj_t* page_container() const { return m_container; }

protected:
  virtual void update() {}

  bool is_visible() const { return m_visible; }

private:
  lv_obj_t* m_container;
  lv_timer_t* m_update_timer;

  bool m_visible;
};

class Screen : public Page {
public:
  explicit Screen(u32 update_interval = 0);
  virtual ~Screen() {}
};

class Overlay : public Page {
public:
  explicit Overlay(lv_obj_t* parent, u32 update_interval = 0);
  virtual ~Overlay() {}
};

class Ui {
public:
  class Style {
  public:
    struct Colors {
      lv_color_t background;
      lv_color_t on_background;

      lv_color_t primary;
      lv_color_t secondary;
      lv_color_t caption;

      lv_color_t warning;
      lv_color_t error;
    };

    lv_style_t const* container() const { return &m_container; }
    lv_style_t const* button() const { return &m_button; }
    lv_style_t const* checkbox_main() const { return &m_checkbox_main; }
    lv_style_t const* checkbox_indicator() const {
      return &m_checkbox_indicator;
    }

    lv_style_t const* headline1() const { return &m_headline1; }
    lv_style_t const* headline2() const { return &m_headline2; }
    lv_style_t const* headline3() const { return &m_headline3; }
    lv_style_t const* body_text() const { return &m_body_text; }
    lv_style_t const* caption() const { return &m_caption; }

    lv_style_t const* icon() const { return &m_icon; }

    Colors const colors;

  private:
    friend class Ui;

    Style();

    lv_style_t m_container;
    lv_style_t m_button;
    lv_style_t m_checkbox_main;
    lv_style_t m_checkbox_indicator;

    lv_style_t m_headline1;
    lv_style_t m_headline2;
    lv_style_t m_headline3;
    lv_style_t m_body_text;
    lv_style_t m_caption;

    lv_style_t m_icon;
  };

  Style const& style() const { return m_style; }

  lv_event_code_t data_event() const { return m_data_event; }
  lv_event_code_t enter_fullscreen_event() const {
    return m_enter_fullscreen_event;
  }
  lv_event_code_t exit_fullscreen_event() const {
    return m_exit_fullscreen_event;
  }
  lv_event_code_t pop_fullscreen_event() const {
    return m_pop_fullscreen_event;
  }

  static Ui& the();

  void initialize();

  void enter_fullscreen(lv_obj_t* source, Screen* screen);
  void exit_fullscreen();
  bool in_fullscreen() { return !m_sub_screens.empty(); }

  void add_overlay(Page* overlay) { m_overlays.push_back(overlay); }

  lv_obj_t* data_event_obj() { return m_data_event_obj; }

private:
  Ui();

  Style m_style;

  lv_obj_t* m_data_event_obj;
  Screen* m_home_screen;

  struct SubScreens {
    Screen* screen;
    /// Used to send enter/exit fullscreen events
    lv_obj_t* source;
  };

  std::vector<SubScreens> m_sub_screens{};
  lv_event_code_t m_data_event;
  lv_event_code_t m_enter_fullscreen_event;
  lv_event_code_t m_exit_fullscreen_event;
  lv_event_code_t m_pop_fullscreen_event;

  std::vector<Page*> m_overlays;
};

lv_obj_t* headline1(lv_obj_t* parent);
lv_obj_t* headline2(lv_obj_t* parent);
lv_obj_t* headline3(lv_obj_t* parent);
lv_obj_t* body_text(lv_obj_t* parent = nullptr);
lv_obj_t* caption(lv_obj_t* parent);

lv_obj_t* flex_container(lv_obj_t* parent = nullptr);
lv_obj_t* text_button(lv_obj_t* parent, char const* text,
                      lv_event_cb_t on_short_click, void* user_data);
lv_obj_t* fullscreen_back_button(lv_obj_t* parent);
lv_obj_t* checkbox(lv_obj_t* parent);

} // namespace ui
