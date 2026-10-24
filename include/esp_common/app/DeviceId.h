// File: include/esp_common/app/DeviceId.h
#pragma once
/// @file DeviceId.h
/// @brief Хелпер deviceId (8 байт): hex-строки, сравнение, генерация при первом старте.
///
/// deviceId хранится как сырой uint8_t[8] внутри SystemConfig (pack(1)) и в
/// wire-структурах handshake; эта обёртка добавляет только удобные операции.
/// Нулевое значение — «не задан»: handshake-запрос с нулевым targetDeviceId
/// означает broadcast (отвечает любой), SSDP USN строится из hex-представления.

#include <string.h>

#include <string>

#include "esp_common/proto/ProtocolDefs.h"

namespace esp_common {

/// Обёртка над 8-байтовым deviceId (совместима по памяти с SystemConfig.deviceId).
struct DeviceId {
  uint8_t bytes[kDeviceIdLen] = {0};

  bool isZero() const { return deviceIdIsZero(bytes); }

  bool operator==(const DeviceId& o) const { return memcmp(bytes, o.bytes, kDeviceIdLen) == 0; }
  bool operator!=(const DeviceId& o) const { return !(*this == o); }

  /// Hex-строка из 16 символов (нижний регистр).
  std::string toString() const {
    char buf[2 * kDeviceIdLen + 1];
    deviceIdToHex(bytes, buf);
    return std::string(buf);
  }

  /// Разбор hex-строки (ровно 16 hex-символов, регистр не важен); false — мусор.
  static bool fromString(const char* s, DeviceId& out) {
    return deviceIdFromHex(s, out.bytes);
  }
};

/// @brief Если @p id нулевой («не задан») — сгенерировать и записать на место.
///
/// Формат: старшие 6 байт — заводской MAC (esp_efuse_mac_get_default, стабилен
/// между перепрошивками), младшие 2 — случайные (esp_random, защита от
/// коллизий при дублированном MAC). На нативной сборке — xorshift-заглушка.
/// Непустое значение не трогается (повторный вызов безопасен).
///
/// TODO: persistence сгенерированного значения в NVS — сейчас deviceId живёт
/// до перезагрузки, если секция System не была сохранена через SET_CONFIG.
void ensureDeviceId(uint8_t (&id)[kDeviceIdLen]);

} // namespace esp_common
