#include "wifi_manager.h"
#include <data.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/sys.h>

WifiManager::WifiManager() {
  initialize_nvs_flash();
  m_mutex = xSemaphoreCreateMutex();
}

bool WifiManager::enable_wifi() {
  xSemaphoreTake(m_mutex, portMAX_DELAY);

  ESP_LOGI("WiFi", "Enabling wifi...");

  auto ssid = this->ssid();
  auto pw = this->password();

  if (!ssid && !pw)
    return false;

  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_create_default_wifi_sta();

  auto size = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  auto free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  printf("free heap: %d, largest free block: %d\n", size, free_block);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_START,
      [](auto, auto, auto, auto) { esp_wifi_connect(); }, NULL);

  esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
      [](auto, auto, auto, auto) {
        ESP_LOGE("WiFi", "STA disconnected :c");
        auto& wm = WifiManager::the();
        ++wm.m_connection_retries;
        if (wm.m_connection_retries >= CONNECTION_ATTEMPTS) {
          wm.disable_wifi();
          return;
        }
        esp_wifi_connect();
      },
      NULL);

  esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP,
      [](auto, auto, auto, void* event_data) {
        auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI("WiFi", "Got IP: %d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        esp_event_post(DATA_EVENT_BASE, Data::Event::WifiConnected, NULL, 0,
                       portMAX_DELAY);
        WifiManager::the().m_connection_retries = 0;
      },
      NULL);

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = "d",
              .password = "d",
              .scan_method = WIFI_ALL_CHANNEL_SCAN,
              .threshold =
                  {
                      .authmode = WIFI_AUTH_WPA2_PSK,
                  },
              .failure_retry_cnt = 10,
          },
  };
  memset(&wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
  memset(&wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
  memcpy(&wifi_config.sta.ssid, ssid->c_str(), ssid->length());
  memcpy(&wifi_config.sta.password, pw->c_str(), pw->length());
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  m_enabled = false;
  ESP_LOGI("WiFi", "WiFi successfully enabled");

  esp_event_post(DATA_EVENT_BASE, Data::Event::WifiEnabled, NULL, 0,
                 portMAX_DELAY);

  return true;
}

void WifiManager::disable_wifi() {
  ESP_LOGI("WiFi", "Disabling wifi...");
  if (auto rc = esp_wifi_disconnect(); rc == ESP_FAIL)
    ESP_ERROR_CHECK(rc);
  esp_wifi_stop();
  esp_wifi_deinit();

  m_enabled = false;
  ESP_LOGI("WiFi", "WiFi successfully disabled");

  esp_event_post(DATA_EVENT_BASE, Data::Event::WifiDisabled, NULL, 0,
                 portMAX_DELAY);
}

void WifiManager::set_ssid(std::string const& ssid) {
  auto handle = open_nvs();
  ESP_ERROR_CHECK(handle->set_string(NVS_SSID_KEY, ssid.c_str()));
  ESP_ERROR_CHECK(handle->commit());
}

std::optional<std::string> WifiManager::ssid() {
  auto handle = open_nvs();
  size_t size = 0;
  handle->get_item_size(nvs::ItemType::SZ, NVS_SSID_KEY, size);
  if (size == 0)
    return std::nullopt;

  std::string s(size, '-');
  ESP_ERROR_CHECK(handle->get_string(NVS_SSID_KEY, s.data(), size));
  return s;
}

void WifiManager::set_password(std::string const& password) {
  auto handle = open_nvs();
  ESP_ERROR_CHECK(handle->set_string(NVS_PASSWORD_KEY, password.c_str()));
  ESP_ERROR_CHECK(handle->commit());
}

std::optional<std::string> WifiManager::password() {
  auto handle = open_nvs();
  size_t size = 0;
  handle->get_item_size(nvs::ItemType::SZ, NVS_PASSWORD_KEY, size);
  if (size == 0)
    return std::nullopt;

  std::string s(size, '-');
  ESP_ERROR_CHECK(handle->get_string(NVS_PASSWORD_KEY, s.data(), size));
  return s;
}

std::unique_ptr<nvs::NVSHandle> WifiManager::open_nvs() {
  esp_err_t err;
  auto handle = nvs::open_nvs_handle(NVS_NAMESPACE, NVS_READWRITE, &err);
  ESP_ERROR_CHECK(err);
  return handle;
}
