// File: include/esp_common/app/PreferencesConfigStorage.h
#pragma once
/// @file PreferencesConfigStorage.h
/// @brief Хранилище настроек поверх NVS (Preferences, Arduino-ESP32).

#if defined(ARDUINO)

#include "esp_common/app/IConfigStorage.h"

namespace esp_common {

/// Секции AppConfig в NVS-пространстве "esp-common" (ключи — configSectionKey).
class PreferencesConfigStorage final : public IConfigStorage {
 public:
  PreferencesConfigStorage() = default;

  Result save(const char* key, const void* data, size_t len) override;
  Result load(const char* key, void* data, size_t maxLen, size_t& outLen) override;
  bool exists(const char* key) override;

 private:
  static const char* kNamespace;
};

} // namespace esp_common

#endif // ARDUINO
