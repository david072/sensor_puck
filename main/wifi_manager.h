#include <freertos/FreeRTOS.h>
#include <nvs_handle.hpp>
#include <optional>
#include <string>
#include <types.h>

class WifiManager {
public:
  static WifiManager& the() {
    static WifiManager instance;
    return instance;
  }

  bool enable_wifi();
  void disable_wifi();
  bool wifi_enabled() const { return m_enabled; }

  void set_ssid(std::string const& ssid);
  std::optional<std::string> ssid();

  void set_password(std::string const& password);

private:
  static constexpr char const* NVS_NAMESPACE = "wifi";
  static constexpr char const* NVS_SSID_KEY = "ssid";
  static constexpr char const* NVS_PASSWORD_KEY = "password";
  static constexpr u8 CONNECTION_ATTEMPTS = 5;

  WifiManager();

  std::optional<std::string> password();

  std::unique_ptr<nvs::NVSHandle> open_nvs();

  SemaphoreHandle_t m_mutex;
  u8 m_connection_retries = 0;
  bool m_enabled = false;
};
