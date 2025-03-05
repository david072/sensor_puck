#pragma once

#include <nvs_handle.hpp>
#include <types.h>

class Preferences {
public:
  static constexpr char const* DEFAULT_NAMESPACE = "sensor_puck";

  static Preferences instance();

  Preferences(std::unique_ptr<nvs::NVSHandle> handle)
      : m_handle(std::move(handle)) {}

  ~Preferences();

  void set_bool(char const* key, bool b);
  std::optional<bool> get_bool(char const* key);

private:
  std::unique_ptr<nvs::NVSHandle> const m_handle;
};
