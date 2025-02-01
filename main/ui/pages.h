#pragma once

#include "ui.h"
#include <ctime>
#include <esp_event.h>
#include <lvgl.h>
#include <optional>
#include <types.h>

namespace ui {

class HomeScreen : public Screen {
public:
  explicit HomeScreen();

private:
  static constexpr u32 UPDATE_INTERVAL_MS = 5 * 1000;

  void update() override;

  lv_obj_t* m_pages_container;
  lv_obj_t* m_battery_percentage;
};

// class SettingsPage : public Page {
// public:
//   explicit SettingsPage();
//   ~SettingsPage();

//   static void async_check_checkbox(void* handler_arg, esp_event_base_t,
//   int32_t,
//                                    void*);
//   static void async_uncheck_checkbox(void* handler_arg, esp_event_base_t,
//                                      int32_t, void*);

// protected:
//   void update() override {}

// private:
//   static constexpr u32 BLUETOOTH_CHECKBOX_DEBOUNCE_MS = 200;

//   u32 m_last_bluetooth_toggle = 0;
//   lv_obj_t* m_bluetooth_checkbox;
//   lv_obj_t* m_wifi_checkbox;
// };

class ClockPage : public Page {
public:
  // class ClockSettingsPage : public Page {
  // public:
  //   explicit ClockSettingsPage();

  // protected:
  //   void update() override;

  // private:
  //   tm m_time;

  //   lv_obj_t* m_hour_label;
  //   lv_obj_t* m_minute_label;
  //   lv_obj_t* m_second_label;
  // };

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

  void test(lv_event_t* event);

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
