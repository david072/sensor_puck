#include "pages.h"
#include "data.h"
#include "layouts/flex/lv_flex.h"
#include "ui.h"
#include <cmath>
#include <constants.h>
#include <esp_log.h>
#include <sys/param.h>

namespace ui {

#define DATA_EVENT_LISTENER(Type, cb, user_data)                               \
  lv_obj_add_event_cb(                                                         \
      Ui::the().data_event_obj(),                                              \
      [](lv_event_t* lv_event) {                                               \
        auto event = *static_cast<Data::Event*>(lv_event_get_param(lv_event)); \
        (cb)(event, static_cast<Type*>(lv_event_get_user_data(lv_event)));     \
      },                                                                       \
      Ui::the().data_event(), user_data);

/// Timer callback to toggle the opacity of the object in the timer's user data
/// between 0 and 100
void blink_timer_cb(lv_timer_t* timer) {
  auto obj = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
  if (lv_obj_get_style_opa(obj, 0) == LV_OPA_0) {
    lv_obj_set_style_opa(obj, LV_OPA_100, 0);
  } else {
    lv_obj_set_style_opa(obj, LV_OPA_0, 0);
  }
}

HomeScreen::HomeScreen()
    : Screen() {
  // battery percentage
  {
    m_battery_percentage = caption(page_container());
    lv_label_set_text(m_battery_percentage, LV_SYMBOL_BATTERY_FULL " ---%");
    lv_obj_set_style_text_color(m_battery_percentage,
                                Ui::the().style().colors.on_background, 0);
    lv_obj_align(m_battery_percentage, LV_ALIGN_TOP_MID, 0, 15);
  }

  // status icons
  {
    m_muted_icon = caption(page_container());
    lv_obj_set_style_text_color(m_muted_icon,
                                ui::Ui::the().style().colors.on_background, 0);
    lv_label_set_text(m_muted_icon, LV_SYMBOL_MUTE);
    lv_obj_align(m_muted_icon, LV_ALIGN_TOP_MID, -50, 18);
  }

  update_status_icons();

  // pages
  {
    m_pages_container = flex_container(page_container());
    lv_obj_set_flex_align(m_pages_container, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_size(m_pages_container, lv_obj_get_width(page_container()),
                    lv_obj_get_height(page_container()));
    lv_obj_set_scrollbar_mode(m_pages_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(m_pages_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(m_pages_container, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(m_pages_container, LV_SCROLL_SNAP_START);
    lv_obj_add_flag(m_pages_container, LV_OBJ_FLAG_SCROLL_ONE);

    new AirQualityPage(m_pages_container);
    new ClockPage(m_pages_container);
    new TimerPage(m_pages_container);
    new CompassPage(m_pages_container);
  }

  // overlays
  { new TimerPage::TimerOverlay(page_container()); }

  // page indicator
  {
    float total_angle = PAGE_INDICATOR_GAP_ANGLE * (PAGE_COUNT - 1);
    float pos_delta =
        120 - PAGE_INDICATOR_DIAMETER / 2.f - PAGE_INDICATOR_EDGE_PADDING;

    float middle_angle = 270 * DEG_TO_RAD;
    for (size_t i = 0; i < PAGE_COUNT; ++i) {
      float angle =
          middle_angle - total_angle / 2.f + PAGE_INDICATOR_GAP_ANGLE * i;
      auto x = cos(angle) * pos_delta;
      auto y = -sin(angle) * pos_delta;

      auto* circle = lv_obj_create(page_container());
      lv_obj_add_style(circle, Ui::the().style().container(), 0);
      lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_opa(circle, LV_OPA_100, 0);
      lv_obj_set_style_bg_color(circle, Ui::the().style().colors.caption, 0);
      lv_obj_set_style_bg_color(circle, Ui::the().style().colors.on_background,
                                LV_STATE_CHECKED);
      lv_obj_set_size(circle, PAGE_INDICATOR_DIAMETER, PAGE_INDICATOR_DIAMETER);
      lv_obj_set_style_transform_pivot_x(circle, LV_PCT(50), 0);
      lv_obj_set_style_transform_pivot_y(circle, LV_PCT(50), 0);
      lv_obj_add_flag(circle, LV_OBJ_FLAG_FLOATING);
      lv_obj_align(circle, LV_ALIGN_CENTER, x, y);

      m_page_indicators[i] = circle;
    }

    lv_obj_add_event_cb(
        m_pages_container,
        [](lv_event_t* event) {
          static_cast<HomeScreen*>(lv_event_get_user_data(event))
              ->on_pages_container_scroll();
        },
        LV_EVENT_SCROLL_END, this);

    on_pages_container_scroll();
  }

  DATA_EVENT_LISTENER(
      HomeScreen,
      [](Data::Event e, HomeScreen* home_screen) {
        if (e == Data::Event::StatusUpdated) {
          home_screen->update_status_icons();
          return;
        } else if (e != Data::Event::BatteryChargeUpdated)
          return;
        home_screen->update();
      },
      this);
} // namespace ui

void HomeScreen::update() {
  auto battery_percentage = Data::the()->battery_percentage();

#define MAKE_FORMAT(symbol) symbol " %d%%"

  char const* format = "";

  if (battery_percentage > 75)
    format = MAKE_FORMAT(LV_SYMBOL_BATTERY_FULL);
  else if (battery_percentage > 50)
    format = MAKE_FORMAT(LV_SYMBOL_BATTERY_3);
  else if (battery_percentage > 25)
    format = MAKE_FORMAT(LV_SYMBOL_BATTERY_2);
  else if (battery_percentage > 0)
    format = MAKE_FORMAT(LV_SYMBOL_BATTERY_1);
  else
    format = MAKE_FORMAT(LV_SYMBOL_BATTERY_EMPTY);

  lv_label_set_text_fmt(m_battery_percentage, format, battery_percentage);

#undef MAKE_FORMAT
}

void HomeScreen::update_status_icons() {
  auto d = Data::the();
  lv_obj_set_style_opa(m_muted_icon, d->muted() ? LV_OPA_100 : LV_OPA_0, 0);
}

void HomeScreen::on_pages_container_scroll() {
  // for some reason, this is in multiples of 248 (i.e. 1st page: 0, 2nd: 248,
  // 3rd: 496, etc.)
  auto offset = lv_obj_get_scroll_x(m_pages_container);
  auto visible_page = offset / 248;

  for (size_t i = 0; i < PAGE_COUNT; ++i) {
    lv_obj_remove_state(m_page_indicators[i], LV_STATE_CHECKED);
  }

  lv_obj_add_state(m_page_indicators[visible_page], LV_STATE_CHECKED);
}

SettingsScreen::SettingsScreen()
    : Screen() {
  lv_obj_set_layout(page_container(), LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page_container(), LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(page_container(), 0, 0);

  fullscreen_back_button(page_container());

  auto* cont = flex_container(page_container());
  lv_obj_set_width(cont, LV_PCT(60));
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_grow(cont, 1);
  lv_obj_set_scroll_dir(cont, LV_DIR_VER);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  auto list_item = [](lv_obj_t* parent, char const* icon, char const* label,
                      lv_event_cb_t on_click, void* user_data) {
    auto* container = flex_container(parent);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(container, 15, 0);
    lv_obj_set_style_pad_ver(container, 10, 0);

    lv_obj_add_event_cb(container, on_click, LV_EVENT_SHORT_CLICKED, user_data);

    auto* icon_text = body_text(container);
    lv_label_set_text(icon_text, icon);
    auto* label_text = body_text(container);
    lv_label_set_text(label_text, label);
  };

  auto divider = [](lv_obj_t* parent) {
    auto* divider = flex_container(parent);
    lv_obj_set_style_bg_opa(divider, LV_OPA_100, 0);
    lv_obj_set_style_bg_color(divider, Ui::the().style().colors.caption, 0);
    lv_obj_set_size(divider, LV_PCT(100), 2);
  };

  list_item(
      cont, SYMBOL_CLOCK, "Uhrzeit",
      [](lv_event_t* event) {
        Ui::the().enter_fullscreen(NULL, new TimeSettingsScreen());
      },
      this);
  divider(cont);
  list_item(
      cont, SYMBOL_CALENDAR, "Datum",
      [](lv_event_t*) {
        Ui::the().enter_fullscreen(NULL, new DateSettingsScreen());
      },
      NULL);
  divider(cont);

  auto* cb = checkbox(cont);
  lv_checkbox_set_text_static(cb, "Mute");
  if (Data::the()->muted())
    lv_obj_add_state(cb, LV_STATE_CHECKED);
  lv_obj_add_event_cb(
      cb,
      [](lv_event_t* e) {
        auto muted =
            lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
        Data::the()->set_muted(muted);
      },
      LV_EVENT_VALUE_CHANGED, NULL);
}

SettingsScreen::DateSettingsScreen::DateSettingsScreen()
    : Screen() {
  lv_obj_set_layout(page_container(), LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page_container(), LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(page_container(), 0, 0);

  fullscreen_back_button(page_container());

  auto* cont = flex_container(page_container());
  lv_obj_set_width(cont, LV_PCT(100));
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_grow(cont, 1);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  auto time = Data::the()->get_time();
  m_day = time.tm_mday;
  m_month = time.tm_mon + 1;
  m_year = time.tm_year + 1900;

  auto dot = [](lv_obj_t* parent) {
    auto* text = body_text(parent);
    lv_label_set_text(text, ".");
  };

  auto* btn = text_button(
      cont, "",
      [](lv_event_t* event) {
        auto* p = get_event_user_data<DateSettingsScreen>(event);
        auto* rp = new RotaryInputScreen(p->m_day);
        rp->set_min(1);
        rp->set_max(31);
        Ui::the().enter_fullscreen(p->page_container(), rp);
      },
      this);
  m_day_label = lv_obj_get_child(btn, 0);
  dot(cont);
  btn = text_button(
      cont, "",
      [](lv_event_t* event) {
        auto* p = get_event_user_data<DateSettingsScreen>(event);
        auto* rp = new RotaryInputScreen(p->m_month);
        rp->set_min(1);
        rp->set_max(12);
        Ui::the().enter_fullscreen(p->page_container(), rp);
      },
      this);
  m_month_label = lv_obj_get_child(btn, 0);
  dot(cont);
  btn = text_button(
      cont, "",
      [](lv_event_t* event) {
        auto* p = get_event_user_data<DateSettingsScreen>(event);
        auto* rp = new RotaryInputScreen(p->m_year);
        rp->set_min(2000);
        rp->set_max(3000);
        Ui::the().enter_fullscreen(p->page_container(), rp);
      },
      this);
  m_year_label = lv_obj_get_child(btn, 0);

  lv_obj_add_event_cb(
      page_container(),
      [](lv_event_t* event) {
        auto* p = get_event_user_data<DateSettingsScreen>(event);
        auto d = Data::the();
        auto time = d->get_time();
        time.tm_mday = p->m_day;
        time.tm_mon = p->m_month - 1;
        time.tm_year = p->m_year - 1900;

        d->set_time(time);
        p->update_labels();
      },
      Ui::the().pop_fullscreen_event(), this);

  update_labels();
}

void SettingsScreen::DateSettingsScreen::update_labels() {
  lv_label_set_text_fmt(m_day_label, "%02d", m_day);
  lv_label_set_text_fmt(m_month_label, "%02d", m_month);
  lv_label_set_text_fmt(m_year_label, "%02d", m_year);
}

SettingsScreen::TimeSettingsScreen::TimeSettingsScreen()
    : Screen() {
  lv_obj_set_layout(page_container(), LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page_container(), LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(page_container(), 0, 0);

  fullscreen_back_button(page_container());

  auto* cont = flex_container(page_container());
  lv_obj_set_width(cont, LV_PCT(100));
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_grow(cont, 1);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  auto time = Data::the()->get_time();
  m_hour = time.tm_hour;
  m_minute = time.tm_min;
  m_second = time.tm_sec;

  auto colon = [](lv_obj_t* parent) {
    auto* text = body_text(parent);
    lv_label_set_text(text, ":");
  };

  auto* btn = text_button(
      cont, "",
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimeSettingsScreen>(event);
        auto* rp = new RotaryInputScreen(p->m_hour);
        rp->set_max(23);
        rp->set_min(0);
        Ui::the().enter_fullscreen(p->page_container(), rp);
      },
      this);
  colon(cont);
  m_hour_label = lv_obj_get_child(btn, 0);
  btn = text_button(
      cont, "",
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimeSettingsScreen>(event);
        auto* rp = new RotaryInputScreen(p->m_minute);
        rp->set_min(0);
        rp->set_max(59);
        Ui::the().enter_fullscreen(p->page_container(), rp);
      },
      this);
  m_minute_label = lv_obj_get_child(btn, 0);
  colon(cont);
  btn = text_button(
      cont, "",
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimeSettingsScreen>(event);
        auto* rp = new RotaryInputScreen(p->m_second);
        rp->set_min(0);
        rp->set_max(59);
        Ui::the().enter_fullscreen(p->page_container(), rp);
      },
      this);
  m_second_label = lv_obj_get_child(btn, 0);

  lv_obj_add_event_cb(
      page_container(),
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimeSettingsScreen>(event);
        auto d = Data::the();
        auto time = d->get_time();
        time.tm_hour = p->m_hour;
        time.tm_min = p->m_minute;
        time.tm_sec = p->m_second;

        d->set_time(time);
        p->update_labels();
      },
      Ui::the().pop_fullscreen_event(), this);

  update_labels();
}

void SettingsScreen::TimeSettingsScreen::update_labels() {
  lv_label_set_text_fmt(m_hour_label, "%02d", m_hour);
  lv_label_set_text_fmt(m_minute_label, "%02d", m_minute);
  lv_label_set_text_fmt(m_second_label, "%02d", m_second);
}

ClockPage::ClockPage(lv_obj_t* parent)
    : Page(parent, UPDATE_INTERVAL_MS) {
  lv_obj_set_layout(page_container(), LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page_container(), LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  m_time = headline1(page_container());
  lv_label_set_text(m_time, "--:--");

  m_date = body_text(page_container());
  lv_obj_set_style_text_color(m_date, Ui::the().style().colors.caption, 0);
  lv_label_set_text(m_date, "----");

  auto* settings_button = text_button(
      page_container(), LV_SYMBOL_SETTINGS,
      [](lv_event_t* event) {
        auto* page = get_event_user_data<ClockPage>(event);
        Ui::the().enter_fullscreen(page->page_container(),
                                   new SettingsScreen());
      },
      this);
  lv_obj_add_flag(settings_button, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(settings_button, LV_ALIGN_BOTTOM_MID, 0, -35);
  lv_obj_set_style_bg_color(settings_button, Ui::the().style().colors.secondary,
                            0);
}

void ClockPage::update() {
  auto time = Data::the()->get_time();
  lv_label_set_text_fmt(m_time, "%02d:%02d", time.tm_hour, time.tm_min);
  // from https://cplusplus.com/reference/ctime/tm/
  // tm_mon has the range 0-11
  // tm_year is the years since 1900
  lv_label_set_text_fmt(m_date, "%02d.%02d.%02d", time.tm_mday, time.tm_mon + 1,
                        time.tm_year + 1900);
}

void format_label_with_minutes_and_seconds(lv_obj_t* label, int ms) {
  auto minutes = ms / (1000 * 60);
  auto seconds = ms / 1000 - minutes * 60;
  lv_label_set_text_fmt(label, "%02d:%02d", minutes, seconds);
}

TimerPage::TimerPage(lv_obj_t* parent)
    : Page(parent, UPDATE_INTERVAL_MS) {
  lv_obj_set_layout(page_container(), LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page_container(), LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(page_container(), 20, 0);

  m_edit_button = text_button(
      page_container(), LV_SYMBOL_EDIT,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPage>(event);
        p->m_prev_duration_ms = p->m_duration_ms;
        auto* rp = new RotaryInputScreen(p->m_duration_ms, 1000.0);
        rp->set_min(0);
        rp->set_update_label_callback(format_label_with_minutes_and_seconds);
        Ui::the().enter_fullscreen(p->page_container(), rp);
      },
      this);

  lv_obj_add_event_cb(
      page_container(),
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPage>(event);
        if (p->m_prev_duration_ms != p->m_duration_ms) {
          Data::the()->user_timer().reset();
        }
      },
      Ui::the().pop_fullscreen_event(), this);

  m_time_label = headline1(page_container());
  lv_obj_align(m_time_label, LV_ALIGN_CENTER, 0, 0);

  auto* controls_container = flex_container(page_container());
  lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  m_play_pause_button = text_button(
      controls_container, LV_SYMBOL_PLAY,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPage>(event);
        p->toggle_timer();
      },
      this);

  m_play_pause_button_label = lv_obj_get_child(m_play_pause_button, 0);
  lv_obj_add_style(m_play_pause_button_label, Ui::the().style().icon(), 0);

  text_button(
      controls_container, LV_SYMBOL_REFRESH,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPage>(event);
        Data::the()->user_timer().reset();
        p->m_duration_ms = 0;
      },
      this);

  m_time_blink_timer =
      lv_timer_create(blink_timer_cb, BLINK_TIMER_PERIOD_MS, m_time_label);
  lv_timer_pause(m_time_blink_timer);
  lv_timer_set_auto_delete(m_time_blink_timer, false);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::UserTimerExpired,
      [](void* handler_arg, esp_event_base_t, int32_t, void*) {
        auto* p = static_cast<TimerPage*>(handler_arg);
        p->m_duration_ms = 0;
        lv_timer_reset(p->m_time_blink_timer);
        // each repetition of the timer toggles the opacity of the label,
        // therefore, for n blinks, we need n * 2 repetitions of the timer
        lv_timer_set_repeat_count(p->m_time_blink_timer,
                                  BLINK_TIMER_REPEAT_COUNT * 2);
        lv_timer_resume(p->m_time_blink_timer);
      },
      this);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::SetDownGesture,
      [](void* handler_arg, esp_event_base_t, int32_t, void*) {
        auto* p = static_cast<TimerPage*>(handler_arg);
        if (!p->is_visible())
          return;
        p->toggle_timer();
      },
      this);
}

void TimerPage::toggle_timer() {
  if (m_duration_ms == 0)
    return;

  auto d = Data::the();
  if (d->user_timer().is_running()) {
    d->user_timer().stop();
  } else {
    if (m_duration_ms != m_prev_duration_ms) {
      d->user_timer().start(m_duration_ms);
    } else {
      d->user_timer().resume();
    }
  }
}

void TimerPage::update() {
  auto d = Data::the();

  if (d->user_timer().is_running()) {
    m_duration_ms = d->user_timer().remaining_duration_ms();
    m_prev_duration_ms = m_duration_ms;
    lv_label_set_text(m_play_pause_button_label, LV_SYMBOL_PAUSE);
  } else {
    lv_label_set_text(m_play_pause_button_label, LV_SYMBOL_PLAY);
  }
  lv_obj_set_state(m_edit_button, LV_STATE_DISABLED,
                   d->user_timer().is_running());
  lv_obj_set_state(m_play_pause_button, LV_STATE_DISABLED, m_duration_ms == 0);

  format_label_with_minutes_and_seconds(m_time_label, m_duration_ms);
}

TimerPage::TimerOverlay::TimerOverlay(lv_obj_t* parent)
    : Overlay(parent, UPDATE_INTERVAL_MS) {
  m_arc = lv_arc_create(page_container());
  lv_arc_set_rotation(m_arc, 270);
  lv_arc_set_bg_angles(m_arc, 0, 360);
  lv_obj_remove_style(m_arc, NULL, LV_PART_KNOB);
  lv_obj_remove_style(m_arc, NULL, LV_PART_MAIN);
  lv_obj_remove_flag(m_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(m_arc, 5, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(m_arc, Ui::the().style().colors.primary,
                             LV_PART_INDICATOR);
  lv_obj_set_size(m_arc, lv_pct(100), lv_pct(100));
  lv_obj_center(m_arc);

  lv_arc_set_value(m_arc, 100);
  lv_obj_add_flag(m_arc, LV_OBJ_FLAG_HIDDEN);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::UserTimerStarted,
      [](void* handler_arg, esp_event_base_t, int32_t, void* event_data) {
        auto* to = static_cast<TimerOverlay*>(handler_arg);
        to->m_original_timer_duration = *static_cast<int*>(event_data);
      },
      this);

  m_blink_timer = lv_timer_create(
      [](lv_timer_t* timer) {
        auto obj = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
          lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
      },
      BLINK_TIMER_PERIOD_MS, m_arc);
  lv_timer_pause(m_blink_timer);
  lv_timer_set_auto_delete(m_blink_timer, false);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::UserTimerExpired,
      [](void* handler_arg, esp_event_base_t, int32_t, void*) {
        auto* p = static_cast<TimerOverlay*>(handler_arg);
        lv_timer_reset(p->m_blink_timer);
        // each repetition of the timer toggles the opacity of the overlay,
        // therefore, for n blinks, we need n * 2 repetitions of the timer
        lv_timer_set_repeat_count(p->m_blink_timer,
                                  BLINK_TIMER_REPEAT_COUNT * 2);
        lv_timer_resume(p->m_blink_timer);
      },
      this);
}

void TimerPage::TimerOverlay::update() {
  auto d = Data::the();
  if (d->user_timer().is_running() && m_original_timer_duration != 0) {
    auto remaining = d->user_timer().remaining_duration_ms();
    lv_arc_set_value(m_arc, 100 * remaining / m_original_timer_duration);
    lv_obj_remove_flag(m_arc, LV_OBJ_FLAG_HIDDEN);
  } else {
    // only hide the arc once, to allow e.g. blinking of the arc to indicate the
    // timer having expired
    if (d->user_timer().remaining_duration_ms() == 0 &&
        m_previous_timer_duration != 0) {
      lv_arc_set_value(m_arc, 100);
      lv_obj_add_flag(m_arc, LV_OBJ_FLAG_HIDDEN);
    }
  }

  m_previous_timer_duration = d->user_timer().remaining_duration_ms();
}

AirQualityPage::AirQualityPage(lv_obj_t* parent)
    : Page(parent) {
  lv_obj_set_layout(page_container(), LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page_container(), LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(page_container(), 30, 0);

  {
    m_iaq_container = lv_obj_create(page_container());
    lv_obj_add_flag(m_iaq_container, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(m_iaq_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(m_iaq_container, 35, 35);
    lv_obj_add_style(m_iaq_container, Ui::the().style().container(), 0);
    lv_obj_set_style_bg_opa(m_iaq_container, LV_OPA_100, 0);
    lv_obj_set_style_bg_color(m_iaq_container, make_color(0x2d, 0xbf, 0x1f), 0);
    lv_obj_set_style_radius(m_iaq_container, LV_RADIUS_CIRCLE, 0);

    m_iaq_label = headline3(m_iaq_container);
    lv_label_set_text(m_iaq_label, "2");
    lv_obj_align(m_iaq_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(m_iaq_label,
                                Ui::the().style().colors.on_background, 0);

    lv_obj_set_style_opa(m_iaq_container, LV_OPA_0, 0);
  }

  // CO2 PPM
  {
    auto* cont = flex_container(page_container());
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    m_co2_ppm = headline2(cont);
    lv_label_set_text(m_co2_ppm, "---");

    auto* co2_ppm_unit = caption(cont);
    lv_label_set_text(co2_ppm_unit, "ppm");
    lv_obj_set_style_translate_y(co2_ppm_unit, -10, 0);
  }

  auto circle_flex_container = [](lv_obj_t* parent) {
    auto* cont = flex_container(parent);
    lv_obj_set_size(cont, 75, 75);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(cont, Ui::the().style().colors.secondary, 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    return cont;
  };

  // Temperature and relative humidity
  {
    auto* cont = flex_container(page_container());
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(cont, 30, 0);

    // Temperature
    {
      auto* temp_container = circle_flex_container(cont);
      m_temperature = headline3(temp_container);
      lv_label_set_text(m_temperature, "---");

      auto* temp_unit = caption(temp_container);
      lv_label_set_text(temp_unit, "°C");
    }

    // Humidity
    {
      auto* hum_container = circle_flex_container(cont);
      m_humidity = headline3(hum_container);
      lv_label_set_text(m_humidity, "---");

      auto* hum_unit = caption(hum_container);
      lv_label_set_text(hum_unit, "% RH");
    }
  }

  DATA_EVENT_LISTENER(
      AirQualityPage,
      [](Data::Event e, AirQualityPage* aqp) {
        if (e != Data::Event::EnvironmentDataUpdated)
          return;
        aqp->update();
      },
      this);
}

void AirQualityPage::update() {
  auto data = Data::the();
  lv_label_set_text_fmt(m_co2_ppm, "%d", data->co2_ppm());
  lv_label_set_text_fmt(m_temperature, "%.1f", data->temperature());
  lv_label_set_text_fmt(m_humidity, "%.1f", data->humidity());

  auto co2_iaq = data->co2_iaq();

  if (co2_iaq >= Iaq::Moderate) {
    lv_obj_set_style_text_color(m_co2_ppm, Ui::the().style().colors.warning, 0);
    if (co2_iaq >= Iaq::VeryPoor) {
      lv_obj_set_style_text_color(m_co2_ppm, Ui::the().style().colors.error, 0);
    }
  } else {
    lv_obj_set_style_text_color(m_co2_ppm,
                                Ui::the().style().colors.on_background, 0);
  }

  auto iaq = data->iaq();
  lv_obj_set_style_bg_color(m_iaq_container, iaq.color, 0);
  lv_obj_set_style_opa(m_iaq_container, LV_OPA_100, 0);
  lv_label_set_text_fmt(m_iaq_label, "%d", iaq.index);
  if (iaq.is_light_color) {
    lv_obj_set_style_text_color(
        m_iaq_label, Ui::the().style().colors.on_light_background, 0);
  } else {
    lv_obj_set_style_text_color(m_iaq_label,
                                Ui::the().style().colors.on_background, 0);
  }
}

CompassPage::CompassPage(lv_obj_t* parent)
    : Page(parent, UPDATE_INTERVAL_MS) {
  auto* circle = flex_container(page_container());
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_opa(circle, LV_OPA_100, 0);
  lv_obj_set_style_border_color(circle, Ui::the().style().colors.primary, 0);
  lv_obj_set_style_border_width(circle, 3, 0);
  lv_obj_set_size(circle, 80, 80);
  lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);

  auto* center_marker = flex_container(page_container());
  lv_obj_set_style_bg_opa(center_marker, LV_OPA_100, 0);
  lv_obj_set_style_bg_color(center_marker, Ui::the().style().colors.primary, 0);
  lv_obj_set_size(center_marker, 3, 10);
  lv_obj_set_style_transform_pivot_y(center_marker, LV_PCT(50), 0);
  lv_obj_align(center_marker, LV_ALIGN_CENTER, 0, -43);

  auto cardinal_direction = [](lv_obj_t* parent, char const* label) {
    auto* dir = body_text(parent);
    lv_label_set_text(dir, label);
    lv_obj_align(dir, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_transform_pivot_x(dir, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(dir, LV_PCT(50), 0);
    return dir;
  };

  m_n = cardinal_direction(page_container(), "N");
  m_e = cardinal_direction(page_container(), "O");
  m_s = cardinal_direction(page_container(), "S");
  m_w = cardinal_direction(page_container(), "W");

  m_heading_label = caption(page_container());
  lv_label_set_text(m_heading_label, "0° N");
  lv_obj_align(m_heading_label, LV_ALIGN_BOTTOM_MID, 0, -30);

  auto* center_line_vert = flex_container(page_container());
  lv_obj_set_style_bg_opa(center_line_vert, LV_OPA_100, 0);
  lv_obj_set_style_bg_color(center_line_vert, Ui::the().style().colors.caption,
                            0);
  lv_obj_set_size(center_line_vert, 1, 60);
  lv_obj_align(center_line_vert, LV_ALIGN_CENTER, 0, 0);

  auto* center_line_horz = flex_container(page_container());
  lv_obj_set_style_bg_opa(center_line_horz, LV_OPA_100, 0);
  lv_obj_set_style_bg_color(center_line_horz, Ui::the().style().colors.caption,
                            0);
  lv_obj_set_size(center_line_horz, 60, 1);
  lv_obj_align(center_line_horz, LV_ALIGN_CENTER, 0, 0);

  m_crosshair = flex_container(page_container());
  lv_obj_set_layout(m_crosshair, LV_LAYOUT_NONE);
  lv_obj_set_style_bg_opa(m_crosshair, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(m_crosshair, Ui::the().style().colors.caption, 0);
  lv_obj_set_style_radius(m_crosshair, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_size(m_crosshair, 40, 40);
  lv_obj_align(m_crosshair, LV_ALIGN_CENTER, 0, 0);

  auto* crosshair_vert = flex_container(m_crosshair);
  lv_obj_set_style_bg_opa(crosshair_vert, LV_OPA_100, 0);
  lv_obj_set_style_bg_color(crosshair_vert,
                            Ui::the().style().colors.on_background, 0);
  lv_obj_set_size(crosshair_vert, 1, 30);
  lv_obj_align(crosshair_vert, LV_ALIGN_CENTER, 0, 0);

  auto* crosshair_horz = flex_container(m_crosshair);
  lv_obj_set_style_bg_opa(crosshair_horz, LV_OPA_100, 0);
  lv_obj_set_style_bg_color(crosshair_horz,
                            Ui::the().style().colors.on_background, 0);
  lv_obj_set_size(crosshair_horz, 30, 1);
  lv_obj_align(crosshair_horz, LV_ALIGN_CENTER, 0, 0);
}

void CompassPage::update() {
  auto d = Data::the();
  auto heading = d->compass_heading();

  auto position = [](lv_obj_t* obj, float angle) {
    // we need to do 360° - angle, since we need to "un-rotate" the UI elements
    angle = (360.f - angle - 90.f) * DEG_TO_RAD;
    lv_obj_set_pos(obj, static_cast<i32>(cosf(angle) * 60.f),
                   static_cast<i32>(sinf(angle) * 60.f));
  };

  position(m_n, heading);
  position(m_e, heading + 270.f);
  position(m_s, heading + 180.f);
  position(m_w, heading + 90.f);

  char const* dir_name = "";
  for (int i = 0; i < NUM_DIRECTION_NAMES; ++i) {
    auto lower = -DIRECTION_SLICE_SIZE / 2 + DIRECTION_SLICE_SIZE * i;
    auto upper = lower + DIRECTION_SLICE_SIZE;

    if (heading >= lower && heading <= upper) {
      dir_name = DIRECTION_NAMES[i];
      break;
    }
  }

  // special case, since the first one overlaps the 360°/0° boundary
  if (heading >= 360.f - DIRECTION_SLICE_SIZE / 2.f ||
      heading <= DIRECTION_SLICE_SIZE / 2.f) {
    dir_name = DIRECTION_NAMES[0];
  }

  lv_label_set_text_fmt(m_heading_label, "%.0f° %s", heading, dir_name);

  auto acceleration = d->acceleration();
  auto dx = -acceleration.x * 10;
  auto dy = acceleration.z * 10;
  lv_obj_set_pos(m_crosshair, dx, dy);

  // fade crosshair out when its too far from the center of the screen
  auto offset = sqrtf(dx * dx + dy * dy);
  auto fadeout_start_dist = offset - CROSSHAIR_DISTANCE_FADEOUT_START;
  auto opa = static_cast<u8>(LV_OPA_100) *
             std::clamp(CROSSHAIR_FADE_DISTANCE - fadeout_start_dist, 0.f,
                        CROSSHAIR_FADE_DISTANCE) /
             CROSSHAIR_FADE_DISTANCE;
  lv_obj_set_style_opa(m_crosshair, opa, 0);
}

RotaryInputScreen::RotaryInputScreen(int& value, float units_per_angle)
    : Screen(50),
      m_units_per_angle(units_per_angle),
      m_target_value(value) {
  m_text = body_text(page_container());
  lv_obj_align(m_text, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_transform_pivot_x(m_text, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(m_text, lv_pct(50), 0);

  m_confirm_button = text_button(
      page_container(), LV_SYMBOL_OK,
      [](lv_event_t* event) {
        auto* thiss =
            static_cast<RotaryInputScreen*>(lv_event_get_user_data(event));
        thiss->m_target_value += thiss->m_value;
        Ui::the().exit_fullscreen();
      },
      this);
  lv_obj_align(m_confirm_button, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_transform_pivot_x(m_confirm_button, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(m_confirm_button, lv_pct(50), 0);

  m_cancel_button = text_button(
      page_container(), LV_SYMBOL_CLOSE,
      [](auto) { Ui::the().exit_fullscreen(); }, NULL);
  lv_obj_align(m_cancel_button, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_transform_pivot_x(m_cancel_button, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(m_cancel_button, lv_pct(50), 0);

  m_last_update = millis();
}

void RotaryInputScreen::update() {
  auto elapsed = static_cast<float>(millis() - m_last_update) / 1000.0;
  auto yaw_speed = Data::the()->gyroscope().y;

  if (abs(yaw_speed) >= MIN_ROTATION_THRESHOLD) {
    m_angle += yaw_speed * elapsed;
    m_value = round(m_angle * m_units_per_angle);
    if (m_min)
      m_value = MAX(m_target_value + m_value, *m_min) - m_target_value;
    if (m_max)
      m_value = MIN(m_target_value + m_value, *m_max) - m_target_value;
  }

  if (m_update_label) {
    m_update_label(m_text, m_target_value + m_value);
  } else {
    lv_label_set_text_fmt(m_text, "%d", m_target_value + m_value);
  }
  lv_obj_set_style_transform_rotation(m_text, -m_angle * 10.0, 0);

  // Transform buttons to be on a vertical line, relative to the current
  // rotation. This can be done by adding 270° to the angle and using sin/cos
  // to calculate x- and y-coordinates for a "down" vector. By adding 270°,
  // when the angle is 0°, the vector will be pointing downwards. The confirm
  // button is translated downwards, while the cancel button is translated
  // upwards (note that downwards is not -y).

  lv_obj_set_pos(m_confirm_button,
                 cos((270 + m_angle) * DEG_TO_RAD) * BUTTON_OFFSET,
                 -sin((270 + m_angle) * DEG_TO_RAD) * BUTTON_OFFSET);
  lv_obj_set_style_transform_rotation(m_confirm_button, -m_angle * 10.0, 0);

  lv_obj_set_pos(m_cancel_button,
                 -cos((270 + m_angle) * DEG_TO_RAD) * BUTTON_OFFSET,
                 sin((270 + m_angle) * DEG_TO_RAD) * BUTTON_OFFSET);
  lv_obj_set_style_transform_rotation(m_cancel_button, -m_angle * 10.0, 0);

  m_last_update = millis();
}

} // namespace ui
