// File: src/app/DeviceId.cpp
#include "esp_common/app/DeviceId.h"

#include <string.h>

#include "esp_common/core/Result.h"

#if defined(ARDUINO)
#include <esp_efuse.h>
#include <esp_system.h>
#endif

namespace esp_common {

void ensureDeviceId(uint8_t (&id)[kDeviceIdLen]) {
  if (!deviceIdIsZero(id)) {
    return; // уже задан (загружен из NVS или сгенерирован в этой сессии)
  }

#if defined(ARDUINO)
  // Старшие 6 байт — заводской MAC (первым байтом — как в строке MAC-адреса),
  // младшие 2 — случайные.
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);
  for (size_t i = 0; i < 6; ++i) {
    id[i] = mac[i];
  }
  const uint32_t rnd = esp_random();
  id[6] = static_cast<uint8_t>(rnd >> 8);
  id[7] = static_cast<uint8_t>(rnd);
#else
  // Нативная сборка (тесты): xorshift от времени — заглушка, не криптостойкая.
  uint32_t s = nowMs() ^ 0x9E3779B9u;
  for (size_t i = 0; i < kDeviceIdLen; ++i) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    id[i] = static_cast<uint8_t>(s >> ((i % 4u) * 8u));
  }
#endif

  if (deviceIdIsZero(id)) { // вырожденный источник (MAC=all-0 + rnd=0) — не допускаем
    id[kDeviceIdLen - 1] = 1;
  }
}

} // namespace esp_common
