#include "preferences.h"

Preferences Preferences::instance() {
  esp_err_t err;
  auto handle = nvs::open_nvs_handle(DEFAULT_NAMESPACE, NVS_READWRITE, &err);
  ESP_ERROR_CHECK(err);
  return handle;
}

Preferences::~Preferences() { ESP_ERROR_CHECK(m_handle->commit()); }

void Preferences::set_bool(char const* key, bool b) {
  ESP_ERROR_CHECK(m_handle->set_item(key, b));
}

std::optional<bool> Preferences::get_bool(char const* key) {
  bool b;
  auto result = m_handle->get_item(key, b);
  if (result == ESP_ERR_NVS_NOT_FOUND)
    return std::nullopt;
  ESP_ERROR_CHECK(result);
  return b;
}
