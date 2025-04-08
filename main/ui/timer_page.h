#pragma once

#include "ui.h"

namespace ui {

class TimerPageContent {
public:
  explicit TimerPageContent(lv_obj_t* parent);

  void show();
  void hide();

  void on_set_down_gesture();
  void update();

private:
  void toggle_timer();

  int m_duration_ms = 0;
  int m_prev_duration_ms = 0;
  bool m_duration_changed = false;

  lv_obj_t* m_container;

  lv_obj_t* m_time_label;
  lv_obj_t* m_edit_button;
  lv_obj_t* m_play_pause_button;
  lv_obj_t* m_play_pause_button_label;

  lv_timer_t* m_time_blink_timer;
};

class StopwatchPageContent {
public:
  explicit StopwatchPageContent(lv_obj_t* parent);

  void show();
  void hide();

  void on_set_down_gesture();
  void update();

private:
  void toggle_stopwatch();

  lv_obj_t* m_container;

  lv_obj_t* m_time_label;
  lv_obj_t* m_play_pause_button;
  lv_obj_t* m_play_pause_button_label;
};

class TimerPage : public Page {
public:
  enum class DisplayMode { Timer, Stopwatch };

  class ModeSelectScreen : public Screen {
  public:
    using SelectCallback = std::function<void(DisplayMode)>;

    explicit ModeSelectScreen(DisplayMode current_mode, SelectCallback cbk);

  private:
    SelectCallback const m_on_selected;
  };

  class TimerOverlay : public Overlay {
  public:
    explicit TimerOverlay(lv_obj_t* parent);

  protected:
    void update() override;

  private:
    static constexpr uint32_t UPDATE_INTERVAL_MS = 50;

    int m_original_timer_duration = 0;
    int m_previous_timer_duration = 0;

    lv_obj_t* m_timer_arc;
    lv_obj_t* m_stopwatch_arc;

    lv_timer_t* m_blink_timer;
  };

  static constexpr uint32_t UPDATE_INTERVAL_MS = 100;

  static constexpr uint32_t BLINK_TIMER_REPEAT_COUNT = 4;
  static constexpr uint32_t BLINK_TIMER_PERIOD_MS = 500;

  explicit TimerPage(lv_obj_t* parent);

protected:
  void update() override;

private:
  void set_display_mode(DisplayMode mode);

  DisplayMode m_mode = DisplayMode::Timer;

  TimerPageContent m_timer_page;
  StopwatchPageContent m_stopwatch_page;
};

} // namespace ui
