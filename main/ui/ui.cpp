#include "ui.h"
#include "pages.h"
#include <util.h>

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
    ui.emplace(Ui());
  return *ui;
}

Ui::Style::Style()
    : colors({
          .background = make_color(0x15),
          .on_background = make_color(0xF6),
          .primary = make_color(0xFE, 0x44, 0x95),
          .secondary = make_color(0x0B, 0x81, 0x89),
          .caption = make_color(0x6A),
      }) {
  {
    lv_style_init(&m_container);
    lv_style_set_bg_opa(&m_container, LV_OPA_TRANSP);
    lv_style_set_text_color(&m_container, colors.on_background);
    lv_style_set_border_width(&m_container, 0);
    lv_style_set_pad_all(&m_container, 0);
    lv_style_set_margin_top(&m_container, 0);
    lv_style_set_margin_bottom(&m_container, 0);
    lv_style_set_margin_left(&m_container, 0);
    lv_style_set_margin_right(&m_container, 0);
    lv_style_set_radius(&m_container, 0);
  }

  {
    lv_style_init(&m_button);
    lv_style_set_bg_color(&m_button, colors.primary);
  }

  {
    lv_style_init(&m_headline1);
    lv_style_set_text_color(&m_headline1, colors.on_background);
    lv_style_set_text_font(&m_headline1, &font_montagu_slab_40);
  }

  {
    lv_style_init(&m_headline2);
    lv_style_set_text_color(&m_headline2, colors.on_background);
    lv_style_set_text_font(&m_headline2, &font_montagu_slab_32_bold);
  }

  {
    lv_style_init(&m_headline3);
    lv_style_set_text_color(&m_headline3, colors.on_background);
    lv_style_set_text_font(&m_headline3, &font_montagu_slab_24);
  }

  {
    lv_style_init(&m_body_text);
    lv_style_set_text_color(&m_body_text, colors.on_background);
    lv_style_set_text_font(&m_body_text, &font_montagu_slab_20);
  }

  {
    lv_style_init(&m_caption);
    lv_style_set_text_color(&m_caption, colors.caption);
    lv_style_set_text_font(&m_caption, &font_montagu_slab_16);
  }

  {
    lv_style_init(&m_icon);
    lv_style_set_text_color(&m_icon, colors.on_background);
    lv_style_set_text_font(&m_icon, &lv_font_montserrat_20);
  }
}

Ui::Ui()
    : m_style() {
  m_enter_fullscreen_event = register_lv_event_id();
  m_exit_fullscreen_event = register_lv_event_id();
  m_pop_fullscreen_event = register_lv_event_id();
}

void Ui::initialize() {
  m_home_screen = new HomeScreen();
  lv_screen_load(m_home_screen->page_container());

  lv_refr_now(NULL);
}

void Ui::enter_fullscreen(lv_obj_t* source, Screen* screen) {
  if (!in_fullscreen())
    lv_obj_send_event(source, m_enter_fullscreen_event, NULL);
  m_sub_screens.push_back({.screen = screen, .source = source});
  lv_screen_load(screen->page_container());
}

void Ui::exit_fullscreen() {
  if (!in_fullscreen())
    return;

  auto page = m_sub_screens.back();
  if (m_sub_screens.size() == 1)
    lv_obj_send_event(page.source, m_exit_fullscreen_event, NULL);
  lv_obj_send_event(page.source, m_pop_fullscreen_event, NULL);

  delete page.screen;
  m_sub_screens.pop_back();

  if (in_fullscreen()) {
    lv_screen_load(m_sub_screens.end()->screen->page_container());
  } else {
    lv_screen_load(m_home_screen->page_container());
  }
}

Page::Page(lv_obj_t* parent, u32 update_period) {
  m_container = lv_obj_create(parent);
  lv_obj_set_size(m_container, lv_obj_get_width(lv_scr_act()),
                  lv_obj_get_height(lv_scr_act()));
  lv_obj_add_style(m_container, Ui::the().style().container(), 0);
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

  // m_update_timer waits for it's period to be over before calling update()
  // the first time. By registering this additional timer, we can make the LVGL
  // task call update() immediately when it runs. By having the LVGL task do
  // this, we avoid any potential deadlocks if update() wants to access Data.
  auto immediate_update_timer = lv_timer_create(
      [](lv_timer_t* timer) {
        static_cast<Page*>(lv_timer_get_user_data(timer))->update();
      },
      0, this);
  lv_timer_set_repeat_count(immediate_update_timer, 1);

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

Screen::Screen(u32 update_interval)
    : Page(NULL, update_interval) {
  lv_obj_set_style_bg_opa(page_container(), LV_OPA_100, 0);
  lv_obj_set_style_bg_color(page_container(),
                            Ui::the().style().colors.background, 0);
}

Overlay::Overlay(lv_obj_t* parent, u32 update_interval)
    : Page(parent, update_interval) {
  lv_obj_remove_flag(page_container(), LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(page_container(), LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_set_style_bg_opa(page_container(), LV_OPA_0, 0);
}

lv_obj_t* flex_container(lv_obj_t* parent) {
  auto* cont = lv_obj_create(parent);
  lv_obj_add_style(cont, Ui::the().style().container(), 0);
  lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  return cont;
}

lv_obj_t* text_button(lv_obj_t* parent, char const* text,
                      lv_event_cb_t on_short_click, void* user_data) {
  auto* button = lv_button_create(parent);
  lv_obj_add_style(button, Ui::the().style().button(), 0);
  lv_obj_add_event_cb(button, on_short_click, LV_EVENT_SHORT_CLICKED,
                      user_data);
  auto* label = body_text(button);
  lv_label_set_text(label, text);
  return button;
}

lv_obj_t* headline1(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().headline1(), 0);
  return text;
}

lv_obj_t* headline2(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().headline2(), 0);
  return text;
}

lv_obj_t* headline3(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().headline3(), 0);
  return text;
}

lv_obj_t* body_text(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().body_text(), 0);
  return text;
}

lv_obj_t* caption(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Ui::the().style().caption(), 0);
  return text;
}

} // namespace ui
