#include "ui.h"
#include "data.h"
#include "misc/lv_timer.h"
#include <optional>
#include <string>

namespace ui {

Style::Style() {
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

Style const& Style::the() {
  static std::optional<Style> s_style{};
  if (!s_style)
    s_style = Style();

  return *s_style;
}

lv_obj_t* flex_container(lv_obj_t* parent) {
  auto* cont = lv_obj_create(parent);
  lv_obj_add_style(cont, Style::the().container(), 0);
  lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  return cont;
}

lv_obj_t* large_text(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Style::the().large_text(), 0);
  return text;
}

lv_obj_t* body_text(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Style::the().body_text(), 0);
  return text;
}

lv_obj_t* caption1(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Style::the().caption1(), 0);
  return text;
}

lv_obj_t* divider(lv_obj_t* parent) {
  auto* divider = lv_obj_create(parent);
  lv_obj_add_style(divider, Style::the().divider(), 0);
  lv_obj_set_size(divider, 82, 2);
  return divider;
}

Page::Page(lv_obj_t* parent, uint32_t update_period) {
  m_container = lv_obj_create(parent);
  lv_obj_set_size(m_container, lv_obj_get_width(lv_scr_act()),
                  lv_obj_get_height(lv_scr_act()));
  lv_obj_add_style(m_container, Style::the().page_container(), 0);
  lv_obj_add_flag(m_container, LV_OBJ_FLAG_SNAPPABLE);

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

ClockPage::ClockPage(lv_obj_t* parent)
    : Page(parent, UPDATE_INTERVAL_MS) {
  m_battery_text = body_text(page_container());
  lv_obj_align(m_battery_text, LV_ALIGN_TOP_MID, 0, 20);
  lv_label_set_text(m_battery_text, LV_SYMBOL_BATTERY_FULL " 97%");

  auto* cont = flex_container(page_container());
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_size(cont, lv_pct(100), lv_pct(100));

  m_time = large_text(cont);
  lv_label_set_text(m_time, "13:45");
}

void ClockPage::update() {
  time_t time_info;
  time(&time_info);

  tm time;
  localtime_r(&time_info, &time);

  lv_label_set_text_fmt(m_time, "%02d:%02d", time.tm_hour, time.tm_min);
  lv_label_set_text_fmt(m_battery_text, LV_SYMBOL_BATTERY_FULL " %d%%",
                        Data::the().battery_percentage());
}

TimerPage::TimerPage(lv_obj_t* parent)
    : Page(parent) {
  auto* text = large_text(page_container());
  lv_label_set_text(text, "TimerPage");
  lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
}

void TimerPage::update() {}

AirQualityPage::AirQualityPage(lv_obj_t* parent)
    : Page(parent) {
  m_warning_circle = lv_obj_create(page_container());
  lv_obj_add_style(m_warning_circle, Style::the().container(), 0);
  lv_obj_set_style_bg_color(m_warning_circle, Style::ERROR_COLOR, 0);
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
    lv_obj_set_style_text_color(m_co2_ppm, Style::ERROR_COLOR, 0);

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

void AirQualityPage::update() {}

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
    lv_obj_set_style_bg_color(indicator_circle, Style::ACCENT_COLOR, 0);
    lv_obj_set_style_border_width(indicator_circle, 0, 0);
    lv_obj_set_size(indicator_circle, 25, 25);
    lv_obj_align(indicator_circle, LV_ALIGN_TOP_MID, 0, 0);

    // NOTE: Temporary!!
    auto dir = [*this](std::string text) {
      auto* d = lv_label_create(page_container());
      lv_obj_add_style(d, Style::the().medium_text(), 0);
      lv_label_set_text(d, text.c_str());
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

} // namespace ui
