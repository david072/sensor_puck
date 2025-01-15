#pragma once

#include <cstdint>

#include <ctime>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <nvs_flash.h>
#include <sync.h>
#include <types.h>
#include <util.h>

void initialize_nvs_flash();

/// Apply voltage transformation of the voltage divider on the battery read
/// pin (D0) of the SeeedStudio display.
/// `v` is in volts.
constexpr float battery_voltage_to_measured_voltage(float v) {
  constexpr float R1 = 470000.f;
  constexpr float R2 = 470000.f;
  return (v * 1000.f / (R1 + R2)) * R2;
}

ESP_EVENT_DECLARE_BASE(DATA_EVENT_BASE);

class Data {
public:
  enum Event : int32_t {
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
  static constexpr float TEMPERATURE_OFFSET = -11;
  /// Humidity correction offset in %
  static constexpr float HUMIDITY_OFFSET = 12;

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

  static Mutex<Data>::Guard the();

  void update_battery_voltage(uint32_t voltage);
  void update_inertial_measurements(Vector3 accel, Vector3 gyro);
  void update_temperature(float temp);
  void update_humidity(float hum);
  void update_pressure(float pressure);
  void update_co2_ppm(u16 co2_ppm);

  static tm get_time();
  static tm get_utc_time();
  static void set_time(tm time);
  static void set_time(time_t unix_timestamp);

  Lock::Guard lock_i2c() { return m_i2c_lock.lock(); }
  Lock::Guard lock_lvgl() { return m_lvgl_lock.lock(); }

  static void enable_bluetooth();
  static void disable_bluetooth();
  static bool bluetooth_enabled();

  static void enable_wifi();
  static void disable_wifi();
  static bool wifi_enabled();

  void recover_timer(int original_duration, int remaining_duration);
  void start_timer(int duration);
  void stop_timer() const;
  void resume_timer() const;
  void delete_timer();
  int remaining_timer_duration_ms() const;
  int original_timer_duration_ms() const { return m_original_timer_duration; }
  bool is_timer_running() const;
  bool timer_exists() const { return m_current_timer != NULL; }

  uint8_t battery_percentage() const { return m_battery_percentage; }

  Vector3 const& gyroscope() const { return m_gyroscope; }
  Vector3 const& acceleration() const { return m_acceleration; }

  float temperature() const { return m_temperature; }
  float humidity() const { return m_humidity; }
  float pressure() const { return m_pressure; }
  u16 co2_ppm() const { return m_co2_ppm; }

private:
  bool set_down_gesture_detected();
  bool sdg_cooldown_exceeded() {
    return millis() - m_sdg_cooldown >=
           SDG_COOLDOWN_AFTER_UPWARDS_ACCELERATION_MS;
  }

  Lock m_i2c_lock;
  Lock m_lvgl_lock;

  TimerHandle_t m_current_timer{};
  int m_original_timer_duration = 0;

  uint8_t m_battery_percentage;

  /// °/s
  Vector3 m_gyroscope;
  /// m/s^2
  /// positive x-axis: from the LSM away from the ESP
  /// positive y-axis: upwards, towards the display
  /// positive z-axis: parallelt to the USB-port, pointing towards it
  Vector3 m_acceleration;
  float m_sdg_cooldown = 0;
  u32 m_set_down_gesture_start = 0;

  /// °C
  float m_temperature = 0;
  /// %
  float m_humidity = 0;
  /// hPa
  float m_pressure = 0;
  /// ppm
  u16 m_co2_ppm;
};
