#include "pages.h"
#include "data.h"
#include "layouts/flex/lv_flex.h"
#include <Arduino.h>
#include <ui.h>

namespace ui {

ClockPage::ClockPage(lv_obj_t* parent)
    : Page(parent, UPDATE_INTERVAL_MS) {
  m_battery_text = body_text(page_container());
  lv_obj_align(m_battery_text, LV_ALIGN_TOP_MID, 0, 20);
  lv_label_set_text(m_battery_text, LV_SYMBOL_BATTERY_FULL " 97%");

  auto* cont = flex_container(page_container());
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
  lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);

  m_time = large_text(cont);
  lv_label_set_text(m_time, "13:45");

  std::function<void()> cb = [this]() {
    if (Ui::the().in_fullscreen())
      return;
    Ui::the().enter_fullscreen(page_container(), new ClockSettingsPage());
  };
  on_long_press(page_container(), cb);

  update();
}

void ClockPage::update() {
  auto time = Data::the()->get_time();
  lv_label_set_text_fmt(m_time, "%02d:%02d", time.tm_hour, time.tm_min);
  lv_label_set_text_fmt(m_battery_text, LV_SYMBOL_BATTERY_FULL " %d%%",
                        Data::the()->battery_percentage());
}

ClockSettingsPage::ClockSettingsPage()
    : Page() {
  lv_obj_set_flex_flow(page_container(), LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  auto* time_container = flex_container(page_container());
  lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  m_time = Data::the()->get_time();

#define BUTTON_WITH_LABEL(propery_to_modify, min, max)                         \
  [this, time_container]() {                                                   \
    auto* button = lv_button_create(time_container);                           \
    lv_obj_add_event_cb(                                                       \
        button,                                                                \
        [](lv_event_t* event) {                                                \
          auto* p = get_event_user_data<ClockSettingsPage>(event);             \
          auto* rotary_input_page = new RotaryInputPage(p->propery_to_modify); \
          rotary_input_page->set_min(min);                                     \
          rotary_input_page->set_max(max);                                     \
          Ui::the().enter_fullscreen(p->page_container(), rotary_input_page);  \
        },                                                                     \
        LV_EVENT_SHORT_CLICKED, this);                                         \
    return body_text(button);                                                  \
  }()

  auto separator_colon = [this, time_container]() {
    auto* label = body_text(time_container);
    lv_label_set_text(label, ":");
  };

  m_hour_label = BUTTON_WITH_LABEL(m_time.tm_hour, 0, 24);
  separator_colon();
  m_minute_label = BUTTON_WITH_LABEL(m_time.tm_min, 0, 60);
  separator_colon();
  m_second_label = BUTTON_WITH_LABEL(m_time.tm_sec, 0, 60);

#undef BUTTON_WITH_LABEL

  spacer(page_container(), 0, 10);

  auto* controls_container = flex_container(page_container());
  lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  text_button(
      controls_container, LV_SYMBOL_CLOSE,
      [](auto) { Ui::the().exit_fullscreen(); }, NULL);

  text_button(
      controls_container, LV_SYMBOL_OK,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<ClockSettingsPage>(event);
        Data::the()->set_time(p->m_time);
        Ui::the().exit_fullscreen();
      },
      this);

  lv_obj_add_event_cb(
      page_container(),
      [](lv_event_t* event) {
        auto* p = get_event_user_data<ClockSettingsPage>(event);
        p->update();
      },
      Ui::the().pop_fullscreen_event(), this);

  update();
}

void ClockSettingsPage::update() {
  lv_label_set_text_fmt(m_hour_label, "%02d", m_time.tm_hour);
  lv_label_set_text_fmt(m_minute_label, "%02d", m_time.tm_min);
  lv_label_set_text_fmt(m_second_label, "%02d", m_time.tm_sec);
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

  m_edit_button = text_button(
      page_container(), LV_SYMBOL_EDIT,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPage>(event);
        p->m_prev_duration_ms = p->m_duration_ms;
        auto* rp = new RotaryInputPage(p->m_duration_ms, 1000.0);
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
          Data::the()->delete_timer();
        }
      },
      Ui::the().pop_fullscreen_event(), this);

  spacer(page_container(), 0, 10);

  m_time_label = large_text(page_container());
  lv_obj_align(m_time_label, LV_ALIGN_CENTER, 0, 0);

  spacer(page_container(), 0, 10);

  auto* controls_container = flex_container(page_container());
  lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(page_container(), LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  m_play_pause_button = text_button(
      controls_container, LV_SYMBOL_PLAY,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPage>(event);
        auto d = Data::the();
        if (d->is_timer_running()) {
          d->stop_timer();
        } else {
          if (p->m_duration_ms != p->m_prev_duration_ms) {
            d->start_timer(p->m_duration_ms);
          } else {
            d->resume_timer();
          }
        }
      },
      this);

  m_play_pause_button_label = lv_obj_get_child(m_play_pause_button, 0);

  auto* reset_button = text_button(
      controls_container, LV_SYMBOL_REFRESH,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPage>(event);
        Data::the()->delete_timer();
        p->m_duration_ms = 0;
      },
      this);

  m_time_blink_timer = lv_timer_create(
      [](lv_timer_t* timer) {
        auto obj = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
        if (lv_obj_get_style_opa(obj, 0) == LV_OPA_0) {
          lv_obj_set_style_opa(obj, LV_OPA_100, 0);
        } else {
          lv_obj_set_style_opa(obj, LV_OPA_0, 0);
        }
      },
      BLINK_TIMER_PERIOD_MS, m_time_label);
  lv_timer_pause(m_time_blink_timer);
  lv_timer_set_auto_delete(m_time_blink_timer, false);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::UserTimerExpired,
      [](void* handler_arg, esp_event_base_t, int32_t, void*) {
        auto* p = static_cast<TimerPage*>(handler_arg);
        p->m_duration_ms = 0;
        lv_timer_reset(p->m_time_blink_timer);
        lv_timer_set_repeat_count(p->m_time_blink_timer,
                                  BLINK_TIMER_REPEAT_COUNT);
        lv_timer_resume(p->m_time_blink_timer);
      },
      this);
}

void TimerPage::update() {
  auto d = Data::the();
  if (d->is_timer_running()) {
    m_duration_ms = d->remaining_timer_duration_ms();
    m_prev_duration_ms = m_duration_ms;
    lv_label_set_text(m_play_pause_button_label, LV_SYMBOL_PAUSE);
  } else {
    lv_label_set_text(m_play_pause_button_label, LV_SYMBOL_PLAY);
  }
  lv_obj_set_state(m_edit_button, LV_STATE_DISABLED, d->is_timer_running());
  lv_obj_set_state(m_play_pause_button, LV_STATE_DISABLED, m_duration_ms == 0);

  format_label_with_minutes_and_seconds(m_time_label, m_duration_ms);
}

TimerPage::TimerOverlay::TimerOverlay()
    : Page(NULL, UPDATE_INTERVAL_MS) {
  make_overlay();

  m_arc = lv_arc_create(page_container());
  lv_arc_set_rotation(m_arc, 270);
  lv_arc_set_bg_angles(m_arc, 0, 360);
  lv_obj_remove_style(m_arc, NULL, LV_PART_KNOB);
  lv_obj_remove_flag(m_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(m_arc, 5, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(m_arc, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(m_arc, LV_OPA_50, LV_PART_MAIN);
  lv_obj_set_style_arc_color(m_arc, make_color(0x64, 0x64, 0x64), LV_PART_MAIN);
  lv_obj_set_size(m_arc, lv_pct(100), lv_pct(100));
  lv_obj_center(m_arc);

  lv_arc_set_value(m_arc, 0);
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
        lv_timer_set_repeat_count(p->m_blink_timer, BLINK_TIMER_REPEAT_COUNT);
        lv_timer_resume(p->m_blink_timer);
      },
      this);
}

void TimerPage::TimerOverlay::update() {
  auto d = Data::the();
  if (d->is_timer_running() && m_original_timer_duration != 0) {
    auto remaining = d->remaining_timer_duration_ms();
    lv_arc_set_value(m_arc, 100 * remaining / m_original_timer_duration);
    lv_obj_remove_flag(m_arc, LV_OBJ_FLAG_HIDDEN);
  } else {
    // For some reason, doing this in the event handler for UserTimerExpired
    // halts the program completely, triggering the watchdog. Doing it here
    // (slightly awkward) makes it work...
    if (m_timer_was_running) {
      lv_arc_set_value(m_arc, 100);
      lv_obj_add_flag(m_arc, LV_OBJ_FLAG_HIDDEN);
    }
  }

  m_timer_was_running = d->is_timer_running();
}

AirQualityPage::AirQualityPage(lv_obj_t* parent)
    : Page(parent, UPDATE_INTERVAL_MS) {
  m_warning_circle = lv_obj_create(page_container());
  lv_obj_add_style(m_warning_circle, Ui::the().style().container(), 0);
  lv_obj_set_style_bg_color(m_warning_circle, Ui::Style::ERROR_COLOR, 0);
  lv_obj_set_style_bg_opa(m_warning_circle, LV_OPA_100, 0);
  lv_obj_set_size(m_warning_circle, lv_pct(100), lv_pct(100));

  m_container = flex_container(page_container());
  lv_obj_set_style_radius(m_container, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(m_container, 10, 0);
  lv_obj_set_style_bg_color(m_container, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(m_container, LV_OPA_100, 0);
  lv_obj_set_size(m_container, lv_pct(90), lv_pct(90));
  lv_obj_align(m_container, LV_ALIGN_CENTER, 0, 0);

  // CO2 PPM
  {
    auto* cont = flex_container(m_container);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    m_co2_ppm = large_text(cont);
    lv_label_set_text(m_co2_ppm, "10.000");
    lv_obj_set_style_text_color(m_co2_ppm, Ui::Style::ERROR_COLOR, 0);

    auto* co2_ppm_unit = caption1(cont);
    lv_label_set_text(co2_ppm_unit, "ppm, CO2");
    lv_obj_set_style_translate_y(co2_ppm_unit, -10, 0);
  }

  // divider
  divider(m_container);

  // temperature
  {
    auto* cont = flex_container(m_container);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    m_temperature = large_text(cont);
    lv_label_set_text(m_temperature, "25");

    auto* temp_unit = caption1(cont);
    lv_label_set_text(temp_unit, "°C");
    lv_obj_set_style_translate_y(temp_unit, -10, 0);
  }
}

void AirQualityPage::update() {
  lv_label_set_text_fmt(m_temperature, "%.1f", Data::the()->temperature());
}

CompassPage::CompassPage(lv_obj_t* parent)
    : Page(parent) {
  auto* cont = flex_container(page_container());
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_size(cont, lv_pct(100), lv_pct(100));

  // center information section
  {
    m_cardinal_direction = large_text(cont);
    lv_label_set_text(m_cardinal_direction, "N");

    divider(cont);

    m_angle = large_text(cont);
    lv_label_set_text(m_angle, "0°");
  }

  // cardinal direction labels
  {
    auto* indicator_circle = lv_obj_create(page_container());
    lv_obj_set_style_radius(indicator_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(indicator_circle, Ui::Style::ACCENT_COLOR, 0);
    lv_obj_set_style_border_width(indicator_circle, 0, 0);
    lv_obj_set_size(indicator_circle, 25, 25);
    lv_obj_align(indicator_circle, LV_ALIGN_TOP_MID, 0, 0);

    // NOTE: Temporary!!
    auto dir = [*this](char const* text) {
      auto* d = lv_label_create(page_container());
      lv_obj_add_style(d, Ui::the().style().medium_text(), 0);
      lv_label_set_text(d, text);
      return d;
    };

    m_north = dir("N");
    lv_obj_align(m_north, LV_ALIGN_TOP_MID, 0, 0);
    m_east = dir("E");
    lv_obj_align(m_east, LV_ALIGN_RIGHT_MID, 0, 0);
    m_south = dir("S");
    lv_obj_align(m_south, LV_ALIGN_BOTTOM_MID, 0, 0);
    m_west = dir("W");
    lv_obj_align(m_west, LV_ALIGN_LEFT_MID, 0, 0);
  }
}

void CompassPage::update() {}

RotaryInputPage::RotaryInputPage(int& value, float units_per_angle)
    : Page(NULL, 50),
      m_target_value(value),
      m_units_per_angle(units_per_angle) {
  m_text = body_text(page_container());
  lv_obj_align(m_text, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_transform_pivot_x(m_text, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(m_text, lv_pct(50), 0);

  m_confirm_button = lv_button_create(page_container());
  lv_obj_align(m_confirm_button, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_transform_pivot_x(m_confirm_button, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(m_confirm_button, lv_pct(50), 0);
  lv_obj_add_event_cb(
      m_confirm_button,
      [](lv_event_t* event) {
        auto* thiss =
            static_cast<RotaryInputPage*>(lv_event_get_user_data(event));
        thiss->m_target_value += thiss->m_value;
        Ui::the().exit_fullscreen();
      },
      LV_EVENT_CLICKED, this);

  auto* label = body_text(m_confirm_button);
  lv_label_set_text(label, LV_SYMBOL_OK);

  m_cancel_button = lv_button_create(page_container());
  lv_obj_align(m_cancel_button, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_transform_pivot_x(m_cancel_button, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(m_cancel_button, lv_pct(50), 0);
  lv_obj_add_event_cb(
      m_cancel_button, [](auto) { Ui::the().exit_fullscreen(); },
      LV_EVENT_CLICKED, NULL);

  label = body_text(m_cancel_button);
  lv_label_set_text(label, LV_SYMBOL_CLOSE);

  m_last_update = millis();
}

void RotaryInputPage::update() {
  auto elapsed = static_cast<float>(millis() - m_last_update) / 1000.0;
  auto yaw_speed = Data::the()->gyroscope().y;

  if (abs(yaw_speed) >= MIN_ROTATION_THRESHOLD) {
    m_angle += yaw_speed * elapsed;
    m_value = round(m_angle * m_units_per_angle);
    if (m_min)
      m_value = max(m_target_value + m_value, *m_min) - m_target_value;
    if (m_max)
      m_value = min(m_target_value + m_value, *m_max) - m_target_value;
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
