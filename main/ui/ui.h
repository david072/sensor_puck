#pragma once

#include <functional>
#include <lvgl.h>
#include <vector>

constexpr lv_color_t make_color(uint8_t r, uint8_t g, uint8_t b) {
  return {.blue = b, .green = g, .red = r};
}

constexpr lv_color_t make_color(uint8_t v) { return make_color(v, v, v); }

namespace ui {

constexpr unsigned long LONG_PRESS_DURATION_MS = 300;

lv_event_code_t register_lv_event_id();
lv_point_t get_last_touch_point();

template <typename T>
T* get_event_user_data(lv_event_t* event) {
  return static_cast<T*>(lv_event_get_user_data(event));
}

/// Long press detection for widgets that are children of scrollable widgets
void on_long_press(lv_obj_t* obj, std::function<void()> callback);

class Page {
public:
  explicit Page(lv_obj_t* parent = NULL, uint32_t update_interval = 0);
  virtual ~Page();

  lv_obj_t* page_container() const { return m_container; }

protected:
  virtual void update() {}

  void make_overlay() const;

  bool is_visible() const { return m_visible; }

private:
  lv_obj_t* m_container;
  lv_timer_t* m_update_timer;

  bool m_visible;
};

class Ui {
public:
  class Style {
  public:
    static constexpr lv_color_t ACCENT_COLOR = make_color(0x62, 0x85, 0xF6);
    static constexpr lv_color_t CAPTION_COLOR = make_color(0x81);
    static constexpr lv_color_t ERROR_COLOR = make_color(0xEB, 0x2D, 0x2D);

    lv_style_t const* container() const { return &m_container; }
    lv_style_t const* page_container() const { return &m_page_container; }
    lv_style_t const* divider() const { return &m_divider; }

    lv_style_t const* large_text() const { return &m_large_text; }
    lv_style_t const* medium_text() const { return &m_medium_text; }
    lv_style_t const* body_text() const { return &m_body_text; }
    lv_style_t const* caption1() const { return &m_caption1; }

  private:
    friend class Ui;

    Style();

    lv_style_t m_container;
    lv_style_t m_page_container;
    lv_style_t m_divider;

    lv_style_t m_large_text;
    lv_style_t m_medium_text;
    lv_style_t m_body_text;
    lv_style_t m_caption1;
  };

  Style const& style() const { return m_style; }

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

  // TODO: This should not be heap allocated, as it causes a small lag when
  // entering fullscreen, however I don't know how to do that right now.
  void enter_fullscreen(lv_obj_t* source, Page* page);
  void exit_fullscreen();
  bool in_fullscreen() { return !m_fullscreen_pages.empty(); }

  void add_overlay(Page* overlay) { m_overlays.push_back(overlay); }

private:
  Ui();

  Style m_style;

  struct FullscreenPage {
    Page* page;
    /// Used to send enter/exit fullscreen events
    lv_obj_t* source;
  };

  std::vector<FullscreenPage> m_fullscreen_pages{};
  lv_event_code_t m_enter_fullscreen_event;
  lv_event_code_t m_exit_fullscreen_event;
  lv_event_code_t m_pop_fullscreen_event;

  std::vector<Page*> m_overlays;
};

lv_obj_t* flex_container(lv_obj_t* parent = nullptr);
lv_obj_t* large_text(lv_obj_t* parent = nullptr);
lv_obj_t* body_text(lv_obj_t* parent = nullptr);
lv_obj_t* caption1(lv_obj_t* parent = nullptr);
lv_obj_t* divider(lv_obj_t* parent = nullptr);
lv_obj_t* spacer(lv_obj_t* parent, int width, int height);

lv_obj_t* fullscreen_back_button(lv_obj_t* parent);
lv_obj_t* text_button(lv_obj_t* parent, char const* text,
                      lv_event_cb_t on_short_click, void* user_data);

} // namespace ui
