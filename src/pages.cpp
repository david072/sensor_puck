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

TimerPage::TimerPage(lv_obj_t* parent)
    : Page(parent) {
  auto* text = large_text(page_container());
  lv_label_set_text(text, "TimerPage");
  lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
}

void TimerPage::update() {}

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
  lv_label_set_text(m_text, "Hello");
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

  lv_label_set_text_fmt(m_text, "%d", m_target_value + m_value);
  lv_obj_set_style_transform_rotation(m_text, -m_angle * 10.0, 0);

  // Transform buttons to be on a vertical line, relative to the current
  // rotation. This can be done by adding 270° to the angle and using sin/cos to
  // calculate x- and y-coordinates for a "down" vector. By adding 270°, when
  // the angle is 0°, the vector will be pointing downwards. The confirm button
  // is translated downwards, while the cancel button is translated upwards
  // (note that downwards is not -y).

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
