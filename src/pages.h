#pragma once

#include "ui.h"
#include <lvgl.h>

namespace ui {

class ClockPage : public Page {
public:
  class ClockSettingsPage : public Page {
  public:
    explicit ClockSettingsPage();
    ~ClockSettingsPage();

  protected:
    void update() override;

  private:
    tm m_time;

    lv_obj_t* m_hour_label;
    lv_obj_t* m_minute_label;
    lv_obj_t* m_second_label;
  };

  explicit ClockPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  static constexpr uint32_t UPDATE_INTERVAL_MS = 1000;

  lv_obj_t* m_time;
  lv_obj_t* m_battery_text;

  unsigned long m_press_start = 0;
  lv_point_t m_press_start_point;
  bool m_in_fullscreen = false;
};

class TimerPage : public Page {
public:
  class TimerOverlay : public Page {
  public:
    explicit TimerOverlay();

  protected:
    void update() override;

  private:
    static constexpr uint32_t UPDATE_INTERVAL_MS = 10;

    int m_original_timer_duration = 0;
    bool m_timer_was_running = false;

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

  lv_obj_t* m_container;
  lv_obj_t* m_co2_ppm;
  lv_obj_t* m_temperature;
  lv_obj_t* m_warning_circle;
};

class ExtendedEnvironmentInfoPage : public Page {
public:
  explicit ExtendedEnvironmentInfoPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  static constexpr uint32_t UPDATE_INTERVAL_MS = 2 * 1000;

  lv_obj_t* m_humidity;
  lv_obj_t* m_pressure;
};

class CompassPage : public Page {
public:
  explicit CompassPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  lv_obj_t* m_cardinal_direction;
  lv_obj_t* m_angle;

  lv_obj_t* m_north;
  lv_obj_t* m_south;
  lv_obj_t* m_east;
  lv_obj_t* m_west;
};

class RotaryInputPage : public Page {
public:
  using UpdateLabelCallback = std::function<void(lv_obj_t*, int)>;

  explicit RotaryInputPage(int& value, float units_per_angle = 1.0 / 10.0);

  void set_max(int max) { m_max = max; }
  void set_min(int min) { m_min = min; }

  void set_update_label_callback(UpdateLabelCallback const& update_label) {
    m_update_label = update_label;
  }

protected:
  void update() override;

private:
  static constexpr float MIN_ROTATION_THRESHOLD = 1.0;
  static constexpr int32_t BUTTON_OFFSET = 50;

  unsigned long m_last_update = 0;
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
