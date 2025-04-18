#pragma once

#include <constants.h>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <nvs_flash.h>
#include <sync.h>
#include <types.h>
#include <ui/ui.h>
#include <util.h>
#include <vector>

class UserTimer {
public:
  void recover(int original_duration, int remaining_duration);

  void start(int duration);
  void resume() const;
  void stop() const;
  void reset();

  int remaining_duration_ms() const;
  int original_duration_ms() const { return m_original_duration; }
  bool is_running() const;

private:
  TimerHandle_t m_timer{};
  int m_original_duration = 0;
};

class UserStopwatch {
public:
  void recover(int previously_elapsed_ms, long long start_timestamp,
               bool running);

  void resume();
  void pause();
  void reset();

  int elapsed_ms() const;
  bool is_running() const;

  long long start_timestamp_ms() const { return m_start_timestamp_ms; }
  int previously_elapsed_ms() const { return m_previously_elapsed_ms; }

private:
  /// This keeps track of the amount of milliseconds elapsed before the
  /// stopwatch was paused, to allow properly resuming later.
  int m_previously_elapsed_ms = 0;
  long long m_start_timestamp_ms = 0;
  bool m_running = false;
};

void initialize_nvs_flash();

/// Apply voltage transformation of the voltage divider on the battery read
/// pin (D0) of the SeeedStudio display.
/// `v` is in volts.
constexpr float battery_voltage_to_measured_voltage(float v) {
  constexpr float R1 = 470000.f;
  constexpr float R2 = 470000.f;
  return (v * 1000.f / (R1 + R2)) * R2;
}

class Iaq {
public:
  static Iaq Excellent;
  static Iaq Fine;
  static Iaq Moderate;
  static Iaq Poor;
  static Iaq VeryPoor;
  static Iaq Severe;

  constexpr Iaq(u8 index, lv_color_t color, bool is_light_color)
      : index(index),
        color(color),
        is_light_color(is_light_color) {}

  constexpr bool operator==(Iaq const& other) { return index == other.index; }
  constexpr bool operator!=(Iaq const& other) {
    return !(index == other.index);
  }

  constexpr bool operator<(Iaq const& other) { return index < other.index; }
  constexpr bool operator<=(Iaq const& other) {
    return *this < other || *this == other;
  }

  constexpr bool operator>(Iaq const& other) { return index > other.index; }
  constexpr bool operator>=(Iaq const& other) {
    return *this > other || *this == other;
  }

  u8 const index;
  lv_color_t const color;
  bool const is_light_color;
};

ESP_EVENT_DECLARE_BASE(DATA_EVENT_BASE);

class Data {
public:
  enum Event : i32 {
    BatteryChargeUpdated,
    EnvironmentDataUpdated,
    StatusUpdated,
    TimeChanged,
    UserTimerStarted,
    UserTimerExpired,
    SetDownGesture,
    BluetoothEnabled,
    BluetoothDisabled,
    BluetoothConnected,
    WifiEnabled,
    WifiConnected,
    WifiDisabled,
    Inactivity,
    PrepareDeepSleep,
  };

  static constexpr u32 BLUETOOTH_ADVERSISEMENT_DURATION_MS = 1 * 60 * 1000;

  /// https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#measure-battery-voltage-pins
  /// 3.7V (nominal voltage of the battery), when converted by the voltage
  /// converter on the display's PCB.
  /// [mV]
  static constexpr float MAX_BATTERY_VOLTAGE =
      battery_voltage_to_measured_voltage(3.7);
  /// 3.05V (ESP min is 3V), when converted by the voltage
  /// converter on the display's PCB.
  /// [mV]
  static constexpr float MIN_BATTERY_VOLTAGE =
      battery_voltage_to_measured_voltage(3.05);

  /// Temperature correction offset in °C
  static constexpr float TEMPERATURE_OFFSET = 0;
  /// Humidity correction offset in %
  static constexpr float HUMIDITY_OFFSET = 0;

  /// Gravitational acceleration to correct the accelerometer's values
  static constexpr Vector3 GRAVITATIONAL_ACCELERATION = Vector3(0, 9.71, 0);

  /// SDG (set-down-gesture) values

  /// Downwards acceleration to exceed for a SDG to be detected
  static constexpr float SDG_DOWNWARDS_ACCELERATION_THRESHOLD = 2.5;
  /// Value the absolute vertical acceleration has to be below to finish a SDG
  static constexpr float SDG_END_ACCELERATION = 0.5;
  /// Minimum time for vertical acceleration to return back to
  /// SDG_END_ACCELERATION
  static constexpr ulong SDG_DOWNWARDS_ACCELERATION_MIN_DURATION_MS = 80;
  /// Maximum time for vertical acceleration to return back to
  /// SDG_END_ACCELERATION
  static constexpr ulong SDG_DOWNWARDS_ACCELERATION_MAX_DURATION_MS = 300;
  /// Minimum vertical acceleration for the cooldown to activate. This is to
  /// prevent triggering a SDG while slowing the device down after lifting it
  /// up.
  static constexpr float SDG_COOLDOWN_ACCELERATION_THRESHOLD = 0.5;
  /// Minimum time to wait after SDG_COOLDOWN_ACCELERATION_THRESHOLD has been
  /// exceeded before allowing SDGs to be detected again
  static constexpr ulong SDG_COOLDOWN_AFTER_UPWARDS_ACCELERATION_MS = 200;

  /// Hard- and soft-iron offset for the magnetometer measurements.
  ///
  /// Determined by moving the device to as many different angles as possible,
  /// plotting the points and finding the offset and scale of the ellipsoid
  /// they form.
  static constexpr Vector3 HARD_IRON_OFFSET = Vector3(-.387f, .25f, .35f);
  static constexpr Vector3 SOFT_IRON_OFFSET =
      Vector3(1.f / 1.12f, 1.f / 1.3f, 1.f / 1.3f);
  /// Angle the compass heading is rotated by.
  static constexpr float COMPASS_HEADING_OFFSET = 0.f;

  static constexpr char const* HISTORY_FILE_PATH = BASE_PATH "/history";
  static constexpr i64 TIME_BETWEEN_HISTORY_ENTRIES_S = 30 * 60;
  static constexpr size_t MAX_HISTORY_ENTRIES = 24;

  static constexpr char const* PREFERENCES_IS_MUTED = "muted";

  struct RawHistoryEntry {
    i64 timestamp;
    u16 co2_ppm;
    i16 temp;
    i16 hum;
  };

  struct HistoryEntry {
    i64 timestamp;
    u16 co2_ppm;
    float temp;
    float hum;
  };

  static Mutex<Data>::Guard the();

  void initialize();

  void update_battery_voltage(uint32_t voltage);
  void update_inertial_measurements(Vector3 accel, Vector3 gyro, Vector3 mag);
  void update_temperature(float temp);
  void update_humidity(float hum);
  void update_co2_ppm(u16 co2_ppm);
  void update_voc_index(u16 voc_index);
  void update_nox_index(u16 nox_index);

  static tm get_time();
  static tm get_utc_time();
  static void set_time(tm time);
  static void set_time(time_t unix_timestamp);

  // Currently not used.
  // Lock::Guard lock_lvgl() { return m_lvgl_lock.lock(); }

  static void enable_bluetooth();
  static void disable_bluetooth();
  static bool bluetooth_enabled();

  static void enable_wifi();
  static void disable_wifi();
  static bool wifi_enabled();

  UserTimer& user_timer() { return m_user_timer; }
  UserStopwatch& user_stopwatch() { return m_user_stopwatch; }

  void set_muted(bool muted);
  bool muted() { return m_muted; }

  uint8_t battery_percentage() const { return m_battery_percentage; }

  Vector3 const& gyroscope() const { return m_gyroscope; }
  Vector3 const& acceleration() const { return m_acceleration; }
  Vector3 const& magnetic() const { return m_magnetic; }
  float const& compass_heading() const { return m_compass_heading; }

  float temperature() const { return m_temperature; }
  float humidity() const { return m_humidity; }
  u16 co2_ppm() const { return m_co2_ppm; }

  Iaq co2_iaq() const;
  Iaq voc_iaq() const;
  Iaq nox_iaq() const;
  Iaq iaq() const;

  std::vector<HistoryEntry> history() const;

  void disable_sdg_detection() { m_disable_sdg_detection = true; }
  void enable_sdg_detection() { m_disable_sdg_detection = false; }

  bool is_upside_down() const;

private:
  bool set_down_gesture_detected();
  bool sdg_cooldown_exceeded() {
    return millis() - m_sdg_cooldown >=
           SDG_COOLDOWN_AFTER_UPWARDS_ACCELERATION_MS;
  }

  void update_history();
  std::optional<RawHistoryEntry> last_history_entry() const;

  // Lock m_lvgl_lock;

  UserTimer m_user_timer;
  UserStopwatch m_user_stopwatch;

  bool m_muted;

  uint8_t m_battery_percentage;

  // Axis definitions (looking at the front of the board with the USB-port to
  // the left):
  //
  // positive x-axis: left
  // positive y-axis: down
  // positive z-axis: forward

  /// Rotation around the respective axes °/s.
  Vector3 m_gyroscope;
  /// Acceleration in the respective axes in m/s^2.
  Vector3 m_acceleration;
  /// Magnetic field strength in the respective axes in Gauss.
  Vector3 m_magnetic;
  /// Current compass heading of the device in degrees.
  ///
  /// 0° means the positive z-axis is pointing north.
  float m_compass_heading;

  float m_sdg_cooldown = 0;
  u32 m_set_down_gesture_start = 0;

  bool m_disable_sdg_detection = false;

  /// °C
  float m_temperature = 0;
  /// %
  float m_humidity = 0;
  /// ppm
  u16 m_co2_ppm = 0;

  /// VOC (volatile organic compounds) index. Unitless and ranges from 0
  /// (best) to 500 (worst). 100 is average.
  u16 m_voc_index = 0;
  /// NOx (nitrogen oxides) index. Unitless and ranges from 0 (best) to 500
  /// (worst). 1 is average.
  u16 m_nox_index = 0;
};

/// Counting semaphore to keep track of how many tasks need to perform an
/// action before deep sleep can be entered.
static SemaphoreHandle_t s_prepare_deep_sleep_counter;
/// Event group where the first x bits, where x is the count of
/// g_prepare_deep_sleep_counter, indicate which tasks are ready for deep
/// sleep. When all x bits are set, we can safely enter deep sleep.
static EventGroupHandle_t s_deep_sleep_ready_event_group;

/// Helper struct for tasks to report when they are ready for deep sleep.
struct DeepSleepPreparation {
  DeepSleepPreparation(bool enable_notification = true) {
    index = uxSemaphoreGetCount(s_prepare_deep_sleep_counter);
    xSemaphoreGive(s_prepare_deep_sleep_counter);
    if (enable_notification) {
      esp_event_handler_register(
          DATA_EVENT_BASE, Data::Event::PrepareDeepSleep,
          [](void* task, esp_event_base_t, int32_t, void*) {
            xTaskNotify(static_cast<TaskHandle_t>(task), 1,
                        eSetValueWithOverwrite);
          },
          xTaskGetCurrentTaskHandle());
    }
  }

  bool wait_for_event(u32 ticks_to_wait) {
    return ulTaskNotifyTake(true, ticks_to_wait) != 0;
  }

  void ready() {
    xEventGroupSetBits(s_deep_sleep_ready_event_group, 1 << index);
  }

private:
  u32 index;
};
