// File: include/esp_common/app/DiscoveryTask.h
#pragma once
/// @file DiscoveryTask.h
/// @brief SSDP-обнаружение (упрощённое, без desc.xml): ответ на M-SEARCH.
///
/// Задача публикует устройство в локальной сети: слушает multicast-группу
/// SSDP 239.255.255.250:1900 и отвечает unicast на M-SEARCH с ST
/// `urn:esp-common:device` или `ssdp:all`. deviceId устройства входит в USN:
///
///   HTTP/1.1 200 OK
///   CACHE-CONTROL: max-age=1800
///   EXT:
///   SERVER: esp-common/1.0
///   ST: urn:esp-common:device
///   USN: uuid:esp-common-<16 hex-символов deviceId>
///
/// Ограничения упрощения (осознанно):
///  - нет desc.xml и LOCATION, нет NOTIFY-пакетов (alive/byebye);
///  - ответ отправляется сразу, без случайной задержки 0..MX из спецификации
///    (кооперативный цикл без delay; при большом числе устройств в сети можно
///    добавить отложенную отправку по дедлайну);
///  - TTL multicast-пакетов не выставляется (WiFiUDP arduino-esp32 2.x не
///    отдаёт fd сокета); на unicast-ответы TTL не влияет.

#if defined(ARDUINO) && !defined(ESP_COMMON_NO_TCP)

#include <WiFiUdp.h>

#include "esp_common/app/AppConfig.h"
#include "esp_common/core/ILogger.h"
#include "esp_common/core/ITask.h"

namespace esp_common {

/// @brief Кооперативная задача SSDP: один оборот loop() — один poll без блокировок.
class DiscoveryTask final : public ITask {
 public:
  DiscoveryTask(const AppConfig& config, ILogger& logger);

  const char* name() const override { return "discovery"; }
  void setup() override;
  void loop() override;

 private:
  bool netUp() const;        ///< WiFi готов (STA подключён или AP поднят).
  void joinMulticast();      ///< Вступить в группу 239.255.255.250:1900 (неблокирующе).
  void handleSearch();       ///< Разобрать входящий дейтаграмм; ответить на наш M-SEARCH.
  void sendResponse();       ///< Unicast-ответ адресату последнего пакета.

  const AppConfig& m_config;
  ILogger& m_logger;
  WiFiUDP m_udp;
  bool m_joined = false;     ///< Группа присоединена (сокет жив).
  uint32_t m_nextJoinMs = 0; ///< Не раньше следующей попытки join (WiFi мог не подняться).

  static constexpr uint16_t kSsdpPort = 1900;
  static constexpr uint32_t kJoinRetryMs = 5000; ///< Пауза между попытками join.
  static constexpr size_t kMaxPacket = 512;      ///< M-SEARCH практически всегда меньше.
};

} // namespace esp_common

#endif // ARDUINO && !ESP_COMMON_NO_TCP
