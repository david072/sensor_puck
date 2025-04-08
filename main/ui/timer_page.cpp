#include "timer_page.h"
#include "pages.h"
#include <data.h>
#include <esp_log.h>

namespace ui {

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

void format_label_with_minutes_and_seconds(lv_obj_t* label, int ms) {
  auto minutes = ms / (1000 * 60);
  auto seconds = ms / 1000 - minutes * 60;
  lv_label_set_text_fmt(label, "%02d:%02d", minutes, seconds);
}

TimerPage::TimerPage(lv_obj_t* parent)
    : Page(parent, UPDATE_INTERVAL_MS),
      m_timer_page(page_container()),
      m_stopwatch_page(page_container()) {
  auto* bottom_arc = lv_arc_create(page_container());
  lv_obj_set_style_bg_opa(bottom_arc, LV_OPA_0, 0);
  lv_obj_set_style_arc_color(bottom_arc, Ui::the().style().colors.on_background,
                             LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(bottom_arc, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(bottom_arc, LV_OPA_20, LV_PART_INDICATOR);
  lv_arc_set_rotation(bottom_arc, 90);
  lv_arc_set_angles(bottom_arc, -30, 30);
  lv_obj_remove_flag(bottom_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_style(bottom_arc, NULL, LV_PART_KNOB);
  lv_obj_remove_style(bottom_arc, NULL, LV_PART_MAIN);
  lv_obj_set_size(bottom_arc, LV_PCT(100), LV_PCT(100));
  lv_obj_align(bottom_arc, LV_ALIGN_CENTER, 0, 0);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::SetDownGesture,
      [](void* handler_arg, esp_event_base_t, int32_t, void*) {
        auto* p = static_cast<TimerPage*>(handler_arg);
        if (!p->is_visible())
          return;

        switch (p->m_mode) {
        case DisplayMode::Timer:
          p->m_timer_page.on_set_down_gesture();
          break;
        case DisplayMode::Stopwatch:
          p->m_stopwatch_page.on_set_down_gesture();
          break;
        }
      },
      this);

  lv_obj_remove_flag(page_container(), LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(
      page_container(),
      [](lv_event_t* event) {
        auto* timer_page = get_event_user_data<TimerPage>(event);
        auto dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_TOP) {
          Ui::the().enter_fullscreen(
              NULL,
              new ModeSelectScreen(timer_page->m_mode, [timer_page](auto mode) {
                timer_page->set_display_mode(mode);
              }));
        }
      },
      LV_EVENT_GESTURE, this);
}

void TimerPage::set_display_mode(DisplayMode mode) {
  m_mode = mode;
  m_timer_page.hide();
  m_stopwatch_page.hide();
  update();
}

void TimerPage::update() {
  switch (m_mode) {
  case DisplayMode::Timer:
    m_timer_page.show();
    m_timer_page.update();
    break;
  case DisplayMode::Stopwatch:
    m_stopwatch_page.show();
    m_stopwatch_page.update();
    break;
  }
}

TimerPageContent::TimerPageContent(lv_obj_t* parent) {
  m_container = flex_container(parent);
  lv_obj_align(m_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_layout(m_container, LV_LAYOUT_NONE);
  lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));

  m_edit_button = text_button(
      m_container, LV_SYMBOL_EDIT,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPageContent>(event);
        p->m_prev_duration_ms = p->m_duration_ms;
        auto* rp = new RotaryInputScreen(p->m_duration_ms, 1000.0);
        rp->set_min(0);
        rp->set_update_label_callback(format_label_with_minutes_and_seconds);
        Ui::the().enter_fullscreen(p->m_container, rp);
      },
      this);
  lv_obj_align(m_edit_button, LV_ALIGN_TOP_MID, 0, 46);

  lv_obj_add_event_cb(
      m_container,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPageContent>(event);
        if (p->m_prev_duration_ms != p->m_duration_ms) {
          Data::the()->user_timer().reset();
        }
      },
      Ui::the().pop_fullscreen_event(), this);

  m_time_label = headline1(m_container);
  lv_obj_align(m_time_label, LV_ALIGN_CENTER, 0, 0);

  auto* controls_container = flex_container(m_container);
  lv_obj_align(controls_container, LV_ALIGN_CENTER, 0, 55);
  lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);

  m_play_pause_button = text_button(
      controls_container, LV_SYMBOL_PLAY,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPageContent>(event);
        p->toggle_timer();
      },
      this);

  m_play_pause_button_label = lv_obj_get_child(m_play_pause_button, 0);
  lv_obj_add_style(m_play_pause_button_label, Ui::the().style().icon(), 0);

  text_button(
      controls_container, LV_SYMBOL_REFRESH,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<TimerPageContent>(event);
        Data::the()->user_timer().reset();
        p->m_duration_ms = 0;
      },
      this);

  m_time_blink_timer = lv_timer_create(
      blink_timer_cb, TimerPage::BLINK_TIMER_PERIOD_MS, m_time_label);
  lv_timer_pause(m_time_blink_timer);
  lv_timer_set_auto_delete(m_time_blink_timer, false);

  esp_event_handler_register(
      DATA_EVENT_BASE, Data::Event::UserTimerExpired,
      [](void* handler_arg, esp_event_base_t, int32_t, void*) {
        auto* p = static_cast<TimerPageContent*>(handler_arg);
        p->m_duration_ms = 0;
        lv_timer_reset(p->m_time_blink_timer);
        // each repetition of the timer toggles the opacity of the label,
        // therefore, for n blinks, we need n * 2 repetitions of the timer
        lv_timer_set_repeat_count(p->m_time_blink_timer,
                                  TimerPage::BLINK_TIMER_REPEAT_COUNT * 2);
        lv_timer_resume(p->m_time_blink_timer);
      },
      this);

  hide();
}

void TimerPageContent::show() {
  lv_obj_remove_flag(m_container, LV_OBJ_FLAG_HIDDEN);
}
void TimerPageContent::hide() {
  lv_obj_add_flag(m_container, LV_OBJ_FLAG_HIDDEN);
}

void TimerPageContent::on_set_down_gesture() { toggle_timer(); }

void TimerPageContent::update() {
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

void TimerPageContent::toggle_timer() {
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

StopwatchPageContent::StopwatchPageContent(lv_obj_t* parent) {
  m_container = flex_container(parent);
  lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(m_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_layout(m_container, LV_LAYOUT_NONE);

  m_time_label = headline1(m_container);
  lv_obj_align(m_time_label, LV_ALIGN_CENTER, 0, 0);

  auto* controls_container = flex_container(m_container);
  lv_obj_align(controls_container, LV_ALIGN_CENTER, 0, 55);
  lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_ROW);

  m_play_pause_button = text_button(
      controls_container, LV_SYMBOL_PLAY,
      [](lv_event_t* event) {
        auto* p = get_event_user_data<StopwatchPageContent>(event);
        p->toggle_stopwatch();
      },
      this);

  m_play_pause_button_label = lv_obj_get_child(m_play_pause_button, 0);
  lv_obj_add_style(m_play_pause_button_label, Ui::the().style().icon(), 0);

  text_button(
      controls_container, LV_SYMBOL_REFRESH,
      [](lv_event_t* event) { Data::the()->user_stopwatch().reset(); }, NULL);

  hide();
}

void StopwatchPageContent::show() {
  lv_obj_remove_flag(m_container, LV_OBJ_FLAG_HIDDEN);
}
void StopwatchPageContent::hide() {
  lv_obj_add_flag(m_container, LV_OBJ_FLAG_HIDDEN);
}

void StopwatchPageContent::on_set_down_gesture() { toggle_stopwatch(); }

void StopwatchPageContent::toggle_stopwatch() {
  auto d = Data::the();
  if (d->user_stopwatch().is_running()) {
    d->user_stopwatch().pause();
  } else {
    d->user_stopwatch().resume();
  }
}

void StopwatchPageContent::update() {
  auto d = Data::the();

  if (d->user_stopwatch().is_running()) {
    lv_label_set_text(m_play_pause_button_label, LV_SYMBOL_PAUSE);
  } else {
    lv_label_set_text(m_play_pause_button_label, LV_SYMBOL_PLAY);
  }

  auto elapsed_ms = static_cast<float>(d->user_stopwatch().elapsed_ms());

  auto elapsed_minutes = static_cast<int>(floor(elapsed_ms / (1000.f * 60.f)));
  auto elapsed_seconds =
      static_cast<int>(floor(elapsed_ms / 1000.f - elapsed_minutes * 60.f));
  auto elapsed_milliseconds = static_cast<int>(
      elapsed_ms - elapsed_minutes * 1000 * 60 - elapsed_seconds * 1000);
  auto elapsed_fract_seconds =
      static_cast<int>(round(elapsed_milliseconds / 10.f));

  if (elapsed_minutes > 0) {
    lv_label_set_text_fmt(m_time_label, "%02d:%02d.%02d", elapsed_minutes,
                          elapsed_seconds, elapsed_fract_seconds);
  } else {
    lv_label_set_text_fmt(m_time_label, "%02d.%02d", elapsed_seconds,
                          elapsed_fract_seconds);
  }
}

TimerPage::ModeSelectScreen::ModeSelectScreen(
    DisplayMode current_mode, ModeSelectScreen::SelectCallback cbk)
    : Screen(),
      m_on_selected(cbk) {
  auto* container = flex_container(page_container());
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_size(container, LV_PCT(60), LV_PCT(100));

  auto list_item = [](lv_obj_t* parent, char const* text) -> lv_obj_t* {
    auto* container = flex_container(parent);
    lv_obj_set_width(container, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(container, 10, 0);

    auto* label = body_text(container);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return container;
  };

#define LIST_ITEM(parent, text, mode)                                          \
  {                                                                            \
    auto item = list_item(parent, text);                                       \
    if (current_mode == mode) {                                                \
      lv_obj_set_style_text_color(lv_obj_get_child(item, 0),                   \
                                  Ui::the().style().colors.warning, 0);        \
    }                                                                          \
    lv_obj_add_event_cb(                                                       \
        item,                                                                  \
        [](lv_event_t* event) {                                                \
          auto* screen = get_event_user_data<ModeSelectScreen>(event);         \
          screen->m_on_selected(mode);                                         \
          Ui::the().exit_fullscreen();                                         \
        },                                                                     \
        LV_EVENT_SHORT_CLICKED, this);                                         \
  }

  auto divider = [](lv_obj_t* parent) {
    auto* divider = flex_container(parent);
    lv_obj_set_style_bg_opa(divider, LV_OPA_100, 0);
    lv_obj_set_style_bg_color(divider, Ui::the().style().colors.caption, 0);
    lv_obj_set_size(divider, LV_PCT(100), 2);
  };

  LIST_ITEM(container, "Timer", DisplayMode::Timer);
  divider(container);
  LIST_ITEM(container, "Stoppuhr", DisplayMode::Stopwatch);

#undef LIST_ITEM

  lv_obj_remove_flag(page_container(), LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(
      page_container(),
      [](lv_event_t* event) {
        auto dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_BOTTOM) {
          Ui::the().exit_fullscreen();
        }
      },
      LV_EVENT_GESTURE, NULL);
}

TimerPage::TimerOverlay::TimerOverlay(lv_obj_t* parent)
    : Overlay(parent, UPDATE_INTERVAL_MS) {
  auto make_arc = [](lv_obj_t* parent, lv_color_t color) {
    auto* arc = lv_arc_create(parent);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_style(arc, NULL, LV_PART_MAIN);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);

    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_set_size(arc, lv_pct(100), lv_pct(100));
    lv_obj_center(arc);

    return arc;
  };

  m_timer_arc = make_arc(page_container(), Ui::the().style().colors.primary);
  m_stopwatch_arc =
      make_arc(page_container(), Ui::the().style().colors.warning);

  lv_arc_set_value(m_timer_arc, 100);
  lv_obj_add_flag(m_timer_arc, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(m_stopwatch_arc, LV_OBJ_FLAG_HIDDEN);

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
      BLINK_TIMER_PERIOD_MS, m_timer_arc);
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
    lv_arc_set_value(m_timer_arc, 100 * remaining / m_original_timer_duration);
    lv_obj_remove_flag(m_timer_arc, LV_OBJ_FLAG_HIDDEN);
  } else {
    // only hide the arc once, to allow e.g. blinking of the arc to indicate the
    // timer having expired
    if (d->user_timer().remaining_duration_ms() == 0 &&
        m_previous_timer_duration != 0) {
      lv_arc_set_value(m_timer_arc, 100);
      lv_obj_add_flag(m_timer_arc, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (d->user_stopwatch().is_running()) {
    auto remaining_in_minute = d->user_stopwatch().elapsed_ms() % (60 * 1000);
    lv_arc_set_value(m_stopwatch_arc, 100 * remaining_in_minute / (60 * 1000));
    lv_obj_remove_flag(m_stopwatch_arc, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (d->user_stopwatch().elapsed_ms() == 0) {
      lv_obj_add_flag(m_stopwatch_arc, LV_OBJ_FLAG_HIDDEN);
    }
  }

  m_previous_timer_duration = d->user_timer().remaining_duration_ms();
}

} // namespace ui
