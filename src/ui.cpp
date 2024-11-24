#include "ui.h"
#include <Arduino.h>

namespace ui {

lv_event_code_t register_lv_event_id() {
  return static_cast<lv_event_code_t>(lv_event_register_id());
}

lv_point_t get_last_touch_point() {
  lv_indev_t* indev = lv_indev_get_act();
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  return p;
}

void on_long_press(lv_obj_t* obj, std::function<void()> callback) {
  // TODO: We are getting a lot of RELEASED events, even while the finger is
  // still on the display, which makes long pressing kind of awkward. Fixable?

  struct Args {
    enum class State {
      WaitingForPress,
      Pressing,
      WaitingForRelease,
    };

    long press_start = -1;
    State state = State::WaitingForPress;
    std::function<void()> callback;
  };

  auto* args = new Args{.callback = callback};

  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        auto* args = static_cast<Args*>(lv_event_get_user_data(event));
        // for some reason, LV_EVENT_PRESSED is also dispatched *during* a
        // press...
        if (args->state != Args::State::WaitingForPress)
          return;

        args->press_start = millis();
        args->state = Args::State::Pressing;
      },
      LV_EVENT_PRESSED, args);

  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        auto* args = static_cast<Args*>(lv_event_get_user_data(event));
        if (args->state != Args::State::Pressing)
          return;

        if (lv_indev_get_press_moved(lv_indev_active())) {
          args->state = Args::State::WaitingForRelease;
          return;
        }

        if (millis() - args->press_start >= LONG_PRESS_DURATION_MS) {
          args->callback();
          lv_indev_wait_release(lv_indev_active());
          args->state = Args::State::WaitingForRelease;
        }
      },
      LV_EVENT_PRESSING, args);

  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        auto* args = static_cast<Args*>(lv_event_get_user_data(event));
        args->state = Args::State::WaitingForPress;
      },
      LV_EVENT_RELEASED, args);
}

Ui& Ui::the() {
  static std::optional<Ui> ui;
  if (!ui)
    ui = Ui();
  return *ui;
}

Ui::Style::Style() {
  {
    lv_style_init(&m_container);
    lv_style_set_bg_opa(&m_container, LV_OPA_TRANSP);
    lv_style_set_text_color(&m_container, lv_color_white());
    lv_style_set_border_width(&m_container, 0);
    lv_style_set_pad_all(&m_container, 0);
    lv_style_set_radius(&m_container, 0);
  }

  {
    lv_style_init(&m_page_container);
    lv_style_set_bg_color(&m_page_container, lv_color_black());
    lv_style_set_text_color(&m_page_container, lv_color_white());
    lv_style_set_border_width(&m_page_container, 0);
    lv_style_set_pad_all(&m_page_container, 0);
    lv_style_set_radius(&m_page_container, 0);
  }

  {
    lv_style_init(&m_divider);
    lv_style_set_bg_color(&m_divider, ACCENT_COLOR);
    lv_style_set_border_width(&m_divider, 0);
  }

  {
    lv_style_init(&m_large_text);
    lv_style_set_text_color(&m_large_text, lv_color_white());
    lv_style_set_text_font(&m_large_text, &lv_font_montserrat_40);
  }

  {
    lv_style_init(&m_medium_text);
    lv_style_set_text_color(&m_medium_text, lv_color_white());
    lv_style_set_text_font(&m_medium_text, &lv_font_montserrat_26);
  }

  {
    lv_style_init(&m_caption1);
    lv_style_set_text_color(&m_caption1, CAPTION_COLOR);
    lv_style_set_text_font(&m_caption1, &lv_font_montserrat_18);
  }

  {
    lv_style_init(&m_body_text);
    lv_style_set_text_color(&m_body_text, lv_color_white());
    lv_style_set_text_font(&m_body_text, &lv_font_montserrat_18);
  }
}

Ui::Ui()
    : m_style() {
  m_enter_fullscreen_event = register_lv_event_id();
  m_exit_fullscreen_event = register_lv_event_id();
  m_pop_fullscreen_event = register_lv_event_id();
}

void Ui::enter_fullscreen(lv_obj_t* source, Page* page) {
  if (!in_fullscreen())
    lv_obj_send_event(source, m_enter_fullscreen_event, NULL);
  m_fullscreen_pages.push_back({.page = page, .source = source});
}

void Ui::exit_fullscreen() {
  if (!in_fullscreen())
    return;

  auto page = m_fullscreen_pages.back();
  if (m_fullscreen_pages.size() == 1)
    lv_obj_send_event(page.source, m_exit_fullscreen_event, NULL);
  lv_obj_send_event(page.source, m_pop_fullscreen_event, NULL);

  delete page.page;
  m_fullscreen_pages.pop_back();
}

lv_obj_t* flex_container(lv_obj_t* parent) {
  auto* cont = lv_obj_create(parent);
  lv_obj_add_style(cont, Ui::the().style().container(), 0);
  lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  return cont;
}

lv_obj_t* large_text(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().large_text(), 0);
  return text;
}

lv_obj_t* body_text(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().body_text(), 0);
  return text;
}

lv_obj_t* caption1(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().caption1(), 0);
  return text;
}

lv_obj_t* divider(lv_obj_t* parent) {
  auto* divider = lv_obj_create(parent);
  lv_obj_add_style(divider, Ui::the().style().divider(), 0);
  lv_obj_set_size(divider, 82, 2);
  return divider;
}

lv_obj_t* spacer(lv_obj_t* parent, int width, int height) {
  auto* obj = lv_obj_create(parent);
  lv_obj_add_style(obj, Ui::the().style().container(), 0);
  lv_obj_set_size(obj, width, height);
  return obj;
}

lv_obj_t* fullscreen_back_button(lv_obj_t* parent) {
  auto* button = lv_button_create(parent);
  auto* label = body_text(button);
  lv_label_set_text(label, LV_SYMBOL_LEFT);

  lv_obj_add_event_cb(
      button, [](auto) { Ui::the().exit_fullscreen(); }, LV_EVENT_SHORT_CLICKED,
      NULL);

  return button;
}

lv_obj_t* text_button(lv_obj_t* parent, char const* text,
                      lv_event_cb_t on_short_click, void* user_data) {
  auto* button = lv_button_create(parent);
  lv_obj_add_event_cb(button, on_short_click, LV_EVENT_SHORT_CLICKED,
                      user_data);
  auto* label = body_text(button);
  lv_label_set_text(label, text);
  return button;
}

Page::Page(lv_obj_t* parent, uint32_t update_period) {
  if (!parent)
    parent = lv_scr_act();
  m_container = lv_obj_create(parent);
  lv_obj_set_size(m_container, lv_obj_get_width(lv_scr_act()),
                  lv_obj_get_height(lv_scr_act()));
  lv_obj_add_style(m_container, Ui::the().style().page_container(), 0);
  lv_obj_add_flag(m_container, LV_OBJ_FLAG_SNAPPABLE);
  lv_obj_add_flag(m_container, LV_OBJ_FLAG_EVENT_BUBBLE);

  m_update_timer = lv_timer_create(
      [](lv_timer_t* timer) {
        auto* thiss = static_cast<Page*>(lv_timer_get_user_data(timer));
        thiss->update();
        thiss->m_visible = false;
      },
      update_period, this);
  if (update_period == 0) {
    lv_timer_pause(m_update_timer);
  }

  lv_timer_set_repeat_count(m_update_timer, -1);

  lv_obj_add_event_cb(
      m_container,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<Page>(event);
        p->m_visible = true;
      },
      LV_EVENT_DRAW_MAIN, this);
}

Page::~Page() {
  lv_obj_delete(m_container);
  lv_timer_delete(m_update_timer);
}

void Page::make_overlay() const {
  lv_obj_remove_flag(m_container,
                     static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE |
                                                LV_OBJ_FLAG_CLICK_FOCUSABLE));
  lv_obj_set_style_bg_opa(m_container, LV_OPA_0, 0);
}

} // namespace ui
