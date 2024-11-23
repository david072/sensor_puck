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
  struct Args {
    ulong press_start;
    lv_point_t point;
    std::function<void()> callback;
  };

  auto* args = new Args{.press_start = 0, .point = {}, .callback = callback};

  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        auto* args = static_cast<Args*>(lv_event_get_user_data(event));
        // for some reason, LV_EVENT_PRESSED is also dispatched *during* a
        // press...
        if (args->press_start != 0)
          return;

        args->press_start = millis();
        args->point = get_last_touch_point();
      },
      LV_EVENT_PRESSED, args);

  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        auto* args = static_cast<Args*>(lv_event_get_user_data(event));
        if (args->press_start == 0)
          return;

        auto p = get_last_touch_point();
        auto dx = p.x - args->point.x;
        auto dy = p.y - args->point.y;
        if (dx * dx + dy * dy != 0) {
          args->press_start = 0;
          return;
        }

        if (millis() - args->press_start >= LONG_PRESS_DURATION_MS) {
          args->callback();
          args->press_start = 0;
        }
      },
      LV_EVENT_PRESSING, args);

  lv_obj_add_event_cb(
      obj,
      [](lv_event_t* event) {
        auto* args = static_cast<Args*>(lv_event_get_user_data(event));
        args->press_start = 0;
      },
      LV_EVENT_PRESS_LOST, args);
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
}

void Ui::enter_fullscreen(Page* page) {
  lv_obj_send_event(page->page_container(), m_enter_fullscreen_event, NULL);
  m_fullscreen_page = page;
}

void Ui::exit_fullscreen() {
  if (!m_fullscreen_page)
    return;

  lv_obj_send_event(m_fullscreen_page->page_container(),
                    m_exit_fullscreen_event, NULL);
  delete m_fullscreen_page;
  m_fullscreen_page = NULL;
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

Page::Page(lv_obj_t* parent, uint32_t update_period) {
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
      },
      update_period, this);
  if (update_period == 0) {
    lv_timer_pause(m_update_timer);
  }

  lv_timer_set_repeat_count(m_update_timer, -1);
}

Page::~Page() {
  lv_obj_delete(m_container);
  lv_timer_delete(m_update_timer);
}

} // namespace ui
