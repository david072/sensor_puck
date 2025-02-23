#pragma once

#include "ui.h"
#include <ctime>
#include <esp_event.h>
#include <lvgl.h>
#include <optional>
#include <types.h>
#include <util.h>

namespace ui {

class HomeScreen : public Screen {
public:
  explicit HomeScreen();

private:
  static constexpr u32 UPDATE_INTERVAL_MS = 5 * 1000;
  static constexpr size_t PAGE_COUNT = 4;

  static constexpr float PAGE_INDICATOR_GAP_ANGLE = 6 * DEG_TO_RAD;
  static constexpr u32 PAGE_INDICATOR_DIAMETER = 6;
  static constexpr u32 PAGE_INDICATOR_EDGE_PADDING = 10;

  void update() override;

  void on_pages_container_scroll();

  lv_obj_t* m_pages_container;
  lv_obj_t* m_battery_percentage;

  lv_obj_t* m_page_indicators[PAGE_COUNT] = {};
};

class SettingsScreen : public Screen {
public:
  class DateSettingsScreen : public Screen {
  public:
    explicit DateSettingsScreen();

  private:
    void update_labels();

    lv_obj_t* m_day_label;
    lv_obj_t* m_month_label;
    lv_obj_t* m_year_label;

    int m_day;
    int m_month;
    int m_year;
  };

  class TimeSettingsScreen : public Screen {
  public:
    explicit TimeSettingsScreen();

  private:
    void update_labels();

    lv_obj_t* m_hour_label;
    lv_obj_t* m_minute_label;
    lv_obj_t* m_second_label;

    int m_hour;
    int m_minute;
    int m_second;
  };

  explicit SettingsScreen();
};

class ClockPage : public Page {
public:
  explicit ClockPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  static constexpr uint32_t UPDATE_INTERVAL_MS = 100;

  lv_obj_t* m_time;
  lv_obj_t* m_date;
};

class TimerPage : public Page {
public:
  class TimerOverlay : public Overlay {
  public:
    explicit TimerOverlay(lv_obj_t* parent);

  protected:
    void update() override;

  private:
    static constexpr uint32_t UPDATE_INTERVAL_MS = 10;

    int m_original_timer_duration = 0;
    int m_previous_timer_duration = 0;

    lv_obj_t* m_arc;
    lv_timer_t* m_blink_timer;
  };

  static constexpr uint32_t UPDATE_INTERVAL_MS = 100;

  explicit TimerPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  /// Each repetition of the timer toggles the opacity of the time label.
  /// Therefore, n repetitions will blink the label n/2 times.
  static constexpr uint32_t BLINK_TIMER_REPEAT_COUNT = 8;
  static constexpr uint32_t BLINK_TIMER_PERIOD_MS = 500;

  void toggle_timer();

  int m_duration_ms = 0;
  int m_prev_duration_ms = 0;
  bool m_duration_changed = false;

  lv_obj_t* m_time_label;
  lv_obj_t* m_edit_button;
  lv_obj_t* m_play_pause_button;
  lv_obj_t* m_play_pause_button_label;

  lv_timer_t* m_time_blink_timer;
};

class AirQualityPage : public Page {
public:
  explicit AirQualityPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  static constexpr uint32_t UPDATE_INTERVAL_MS = 2 * 1000;

  lv_obj_t* m_co2_ppm;
  lv_obj_t* m_temperature;
  lv_obj_t* m_humidity;
};

class CompassPage : public Page {
public:
  explicit CompassPage(lv_obj_t* parent);

private:
  static constexpr u32 UPDATE_INTERVAL_MS = 50;

  static constexpr int NUM_DIRECTION_NAMES = 8;
  static constexpr char const* DIRECTION_NAMES[NUM_DIRECTION_NAMES] = {
      "N", "NO", "O", "SO", "S", "SW", "W", "NW"};
  /// How many degrees one item in DIRECTION_NAMES takes up.
  static constexpr float DIRECTION_SLICE_SIZE = 360.f / NUM_DIRECTION_NAMES;

  /// How far the crosshair has to be from the center of the screen for it to
  /// start fading out.
  static constexpr float CROSSHAIR_DISTANCE_FADEOUT_START = 50.f;
  /// How much further from CROSSHAIR_DISTANCE_FADEOUT_START the crosshair has
  /// to be from the center for it to be completely hidden.
  static constexpr float CROSSHAIR_FADE_DISTANCE = 20.f;

  void update() override;

  lv_obj_t* m_n;
  lv_obj_t* m_e;
  lv_obj_t* m_s;
  lv_obj_t* m_w;
  lv_obj_t* m_heading_label;

  lv_obj_t* m_crosshair;
};

class RotaryInputScreen : public Screen {
public:
  using UpdateLabelCallback = std::function<void(lv_obj_t*, int)>;

  explicit RotaryInputScreen(int& value, float units_per_angle = 1.0 / 10.0);

  void set_max(int max) { m_max = max; }
  void set_min(int min) { m_min = min; }

  void set_update_label_callback(UpdateLabelCallback update_label) {
    m_update_label = update_label;
  }

protected:
  void update() override;

private:
  static constexpr float MIN_ROTATION_THRESHOLD = 1.0;
  static constexpr int32_t BUTTON_OFFSET = 50;

  u32 m_last_update = 0;
  float m_angle = 0;

  std::optional<int> m_max{};
  std::optional<int> m_min{};
  /// Rate of change of m_value in units/°
  float m_units_per_angle = 1.0 / 10.0;

  int m_value = 0;
  int& m_target_value;

  UpdateLabelCallback m_update_label;

  lv_obj_t* m_text;
  lv_obj_t* m_confirm_button;
  lv_obj_t* m_cancel_button;
};

} // namespace ui
