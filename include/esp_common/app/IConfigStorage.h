// File: include/esp_common/app/IConfigStorage.h
#pragma once
/// @file IConfigStorage.h
/// @brief Интерфейс хранилища настроек (NVS на ESP32, RAM в тестах).

#include <map>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include <string.h>

#include "esp_common/core/Result.h"

namespace esp_common {

/// @brief Хранилище пар ключ → байты (секции AppConfig).
class IConfigStorage {
 public:
  virtual ~IConfigStorage() = default;

  /// Сохранить/перезаписать значение ключа.
  virtual Result save(const char* key, const void* data, size_t len) = 0;

  /// @brief Прочитать значение ключа.
  /// @return Ok; NotFound — ключа нет; InvalidArgument — не влезает в буфер.
  virtual Result load(const char* key, void* data, size_t maxLen, size_t& outLen) = 0;

  /// Есть ли сохранённое значение.
  virtual bool exists(const char* key) = 0;
};

/// Хранилище в оперативной памяти (тесты, нативная сборка, fallback).
class RamConfigStorage final : public IConfigStorage {
 public:
  Result save(const char* key, const void* data, size_t len) override {
    if (key == nullptr || (data == nullptr && len > 0)) {
      return Result::err(Status::InvalidArgument);
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_data[key] = std::vector<uint8_t>(bytes, bytes + len);
    return Result::ok();
  }

  Result load(const char* key, void* data, size_t maxLen, size_t& outLen) override {
    outLen = 0;
    if (key == nullptr || data == nullptr) {
      return Result::err(Status::InvalidArgument);
    }
    const auto it = m_data.find(key);
    if (it == m_data.end()) {
      return Result::err(Status::NotFound);
    }
    if (it->second.size() > maxLen) {
      return Result::err(Status::InvalidArgument);
    }
    outLen = it->second.size();
    if (outLen > 0) {
      memcpy(data, it->second.data(), outLen);
    }
    return Result::ok();
  }

  bool exists(const char* key) override {
    return key != nullptr && m_data.count(key) != 0;
  }

 private:
  std::map<std::string, std::vector<uint8_t>> m_data;
};

} // namespace esp_common
