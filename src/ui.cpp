#include "ui.h"
#include <optional>

namespace ui {

Style::Style() {
  {
    lv_style_init(&m_container);
    lv_style_set_bg_opa(&m_container, LV_OPA_TRANSP);
    lv_style_set_text_color(&m_container, lv_color_white());
    lv_style_set_border_width(&m_container, 0);
    lv_style_set_pad_all(&m_container, 0);
  }

  {
    lv_style_init(&m_page_container);
    lv_style_set_bg_color(&m_page_container, lv_color_black());
    lv_style_set_text_color(&m_page_container, lv_color_white());
    lv_style_set_border_width(&m_page_container, 0);
    lv_style_set_pad_all(&m_page_container, 0);
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
    lv_style_init(&m_caption1);
    lv_style_set_text_color(&m_caption1, CAPTION_COLOR);
    lv_style_set_text_font(&m_caption1, &lv_font_montserrat_18);
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
  return cont;
}

lv_obj_t* large_text(lv_obj_t* parent) {
  auto* text = lv_label_create(parent);
  lv_obj_add_style(text, Style::the().large_text(), 0);
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

Page::Page(lv_obj_t* parent) {
  m_container = lv_obj_create(parent);
  lv_obj_set_size(m_container, lv_obj_get_width(lv_scr_act()),
                  lv_obj_get_height(lv_scr_act()));
  lv_obj_add_style(m_container, Style::the().page_container(), 0);
  lv_obj_add_flag(m_container, LV_OBJ_FLAG_SNAPPABLE);
}

AirQualityPage::AirQualityPage(lv_obj_t* parent)
    : Page(parent) {
  // auto* label1 = lv_label_create(page_container());
  // lv_label_set_text(label1, "bg");
  // lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);

  m_warning_circle = lv_obj_create(page_container());
  lv_obj_add_style(m_warning_circle, Style::the().container(), 0);
  lv_obj_set_style_bg_color(m_warning_circle, Style::ERROR_COLOR, 0);
  lv_obj_set_style_bg_opa(m_warning_circle, LV_OPA_100, 0);
  lv_obj_set_size(m_warning_circle, lv_pct(100), lv_pct(100));

  m_container = flex_container(page_container());
  lv_obj_set_style_radius(m_container, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(m_container, 20, 0);
  lv_obj_set_style_bg_color(m_container, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(m_container, LV_OPA_100, 0);
  lv_obj_set_size(m_container, lv_pct(90), lv_pct(90));
  lv_obj_align(m_container, LV_ALIGN_CENTER, 0, 0);

  // auto* label = lv_label_create(m_container);
  // lv_label_set_text(label, "1");
  // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  // CO2 PPM
  {
    auto* cont = flex_container(m_container);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
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
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    m_temperature = large_text(cont);
    lv_label_set_text(m_temperature, "25");

    auto* temp_unit = caption1(cont);
    lv_label_set_text(temp_unit, "°C");
    lv_obj_set_style_translate_y(temp_unit, -10, 0);
  }
}

void AirQualityPage::update(Data const& data) {}

} // namespace ui
