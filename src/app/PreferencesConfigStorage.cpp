// File: src/app/PreferencesConfigStorage.cpp
#if defined(ARDUINO)

#include "esp_common/app/PreferencesConfigStorage.h"

#include <Preferences.h>

namespace esp_common {

const char* PreferencesConfigStorage::kNamespace = "esp-common";

Result PreferencesConfigStorage::save(const char* key, const void* data, size_t len) {
  if (key == nullptr || (data == nullptr && len > 0)) {
    return Result::err(Status::InvalidArgument);
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return Result::err(Status::IoError);
  }
  const size_t written = prefs.putBytes(key, data, len);
  prefs.end();
  return (written == len) ? Result::ok() : Result::err(Status::IoError);
}

Result PreferencesConfigStorage::load(const char* key, void* data, size_t maxLen, size_t& outLen) {
  outLen = 0;
  if (key == nullptr || data == nullptr) {
    return Result::err(Status::InvalidArgument);
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return Result::err(Status::IoError);
  }
  outLen = prefs.getBytesLength(key);
  if (outLen == 0) {
    prefs.end();
    return Result::err(Status::NotFound);
  }
  if (outLen > maxLen) {
    prefs.end();
    return Result::err(Status::InvalidArgument);
  }
  prefs.getBytes(key, data, outLen);
  prefs.end();
  return Result::ok();
}

bool PreferencesConfigStorage::exists(const char* key) {
  if (key == nullptr) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }
  const bool found = prefs.isKey(key);
  prefs.end();
  return found;
}

} // namespace esp_common

#endif // ARDUINO
