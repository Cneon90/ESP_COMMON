// File: src/app/DiscoveryTask.cpp
#include "esp_common/app/DiscoveryTask.h"

#if defined(ARDUINO) && !defined(ESP_COMMON_NO_TCP)

#include <stdio.h>
#include <string.h>

#include <WiFi.h>

namespace esp_common {
namespace {

const char kStSelf[] = "urn:esp-common:device";
const char kStAll[] = "ssdp:all";

/// ASCII-символ в нижний регистр (только латиница, без ctype/locale).
char asciiLower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/// Регистронезависимое сравнение C-строк.
bool ieq(const char* a, const char* b) {
  while (*a != '\0' && *b != '\0') {
    if (asciiLower(*a) != asciiLower(*b)) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == *b;
}

/// @brief Значение HTTP-заголовка @p name из пакета (регистронезависимо).
/// @return false, если заголовка нет; значение копируется в @p out с NUL.
bool headerValue(const char* buf, size_t len, const char* name, char* out, size_t outCap) {
  const size_t nameLen = strlen(name);
  for (size_t i = 0; i < len; ++i) {
    if (i != 0 && buf[i - 1] != '\n') {
      continue; // значение ищем только в начале строки
    }
    size_t j = 0;
    while (j < nameLen && i + j < len && asciiLower(buf[i + j]) == asciiLower(name[j])) {
      ++j;
    }
    if (j != nameLen || i + j >= len || buf[i + j] != ':') {
      continue;
    }
    size_t v = i + j + 1;
    while (v < len && (buf[v] == ' ' || buf[v] == '\t')) {
      ++v;
    }
    size_t e = v;
    while (e < len && buf[e] != '\r' && buf[e] != '\n') {
      ++e;
    }
    const size_t vLen = (e - v < outCap - 1) ? (e - v) : (outCap - 1);
    memcpy(out, buf + v, vLen);
    out[vLen] = '\0';
    return true;
  }
  return false;
}

} // namespace

DiscoveryTask::DiscoveryTask(const AppConfig& config, ILogger& logger)
    : m_config(config), m_logger(logger) {}

void DiscoveryTask::setup() {
  joinMulticast();
}

bool DiscoveryTask::netUp() const {
  return WiFi.status() == WL_CONNECTED || WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
}

void DiscoveryTask::joinMulticast() {
  if (m_joined || !isExpired(m_nextJoinMs)) {
    return;
  }
  m_nextJoinMs = nowMs() + kJoinRetryMs; // при неудаче повторим не раньше
  if (!netUp()) {
    return;
  }
  m_udp.stop(); // чистый сокет на каждую попытку
  if (m_udp.beginMulticast(IPAddress(239, 255, 255, 250), kSsdpPort) != 1) {
    m_logger.log(LogLevel::Debug, name(), "multicast join failed (retry in %u ms)",
                 static_cast<unsigned>(kJoinRetryMs));
    return;
  }
  m_joined = true;
  m_logger.log(LogLevel::Info, name(), "SSDP listening on 239.255.255.250:%u",
               static_cast<unsigned>(kSsdpPort));
}

void DiscoveryTask::loop() {
  if (!netUp()) {
    // WiFi пропал (STA отключился) — пересоздадим сокет, когда сеть вернётся.
    if (m_joined) {
      m_joined = false;
      m_udp.stop();
      m_nextJoinMs = nowMs() + kJoinRetryMs;
      m_logger.log(LogLevel::Debug, name(), "WiFi down, will rejoin");
    }
    return;
  }
  if (!m_joined) {
    joinMulticast();
    return;
  }
  handleSearch();

  // Самоанонс: без M-SEARCH со стороны клиента устройство в сети «невидимо»,
  // поэтому периодически шлём NOTIFY ssdp:alive в multicast-группу — устройство
  // появляется в списках discovery само, сразу после загрузки и далее каждые
  // kNotifyPeriodMs. Первый анонс уходит в первый же оборот loop() после join.
  if (isExpired(m_nextNotifyMs)) {
    m_nextNotifyMs = nowMs() + kNotifyPeriodMs;
    sendNotifyAlive();
  }
}

void DiscoveryTask::sendNotifyAlive() {
  char idHex[2 * kDeviceIdLen + 1];
  deviceIdToHex(m_config.system.deviceId, idHex);

  char body[224];
  const int len =
      snprintf(body, sizeof(body),
               "NOTIFY * HTTP/1.1\r\n"
               "HOST: 239.255.255.250:1900\r\n"
               "CACHE-CONTROL: max-age=1800\r\n"
               "NT: urn:esp-common:device\r\n"
               "NTS: ssdp:alive\r\n"
               "SERVER: esp-common/1.0\r\n"
               "USN: uuid:esp-common-%s\r\n"
               "\r\n",
               idHex);
  if (len <= 0 || static_cast<size_t>(len) >= sizeof(body)) {
    return;
  }
  if (m_udp.beginPacket(IPAddress(239, 255, 255, 250), kSsdpPort) != 1) {
    m_logger.log(LogLevel::Debug, name(), "notify beginPacket failed");
    return;
  }
  m_udp.write(reinterpret_cast<const uint8_t*>(body), static_cast<size_t>(len));
  if (m_udp.endPacket() != 1) {
    m_logger.log(LogLevel::Debug, name(), "notify alive failed");
    return;
  }
  m_logger.log(LogLevel::Info, name(), "NOTIFY ssdp:alive sent (deviceId=%s)", idHex);
}

void DiscoveryTask::handleSearch() {
  const int size = m_udp.parsePacket();
  if (size <= 0) {
    return;
  }
  uint8_t buf[kMaxPacket + 1]; // +1 — место под NUL
  const size_t want = (static_cast<size_t>(size) < kMaxPacket) ? static_cast<size_t>(size)
                                                               : kMaxPacket;
  const int n = m_udp.read(buf, want);
  if (n <= 0) {
    return;
  }
  buf[n] = '\0';

  // HTTP-методы регистрозависимы: только "M-SEARCH" в начале пакета.
  if (static_cast<size_t>(n) < 8 || memcmp(buf, "M-SEARCH", 8) != 0) {
    return;
  }
  char st[64];
  if (!headerValue(reinterpret_cast<const char*>(buf), static_cast<size_t>(n), "ST", st,
                   sizeof(st))) {
    return;
  }
  if (!ieq(st, kStSelf) && !ieq(st, kStAll)) {
    m_logger.log(LogLevel::Debug, name(), "M-SEARCH ST=%s: not our type, ignore", st);
    return;
  }
  sendResponse();
}

void DiscoveryTask::sendResponse() {
  char idHex[2 * kDeviceIdLen + 1];
  deviceIdToHex(m_config.system.deviceId, idHex);

  static const char kStLine[] = "ST: urn:esp-common:device\r\n";
  char body[192];
  const int len =
      snprintf(body, sizeof(body),
               "HTTP/1.1 200 OK\r\n"
               "CACHE-CONTROL: max-age=1800\r\n"
               "EXT:\r\n"
               "SERVER: esp-common/1.0\r\n"
               "%s"
               "USN: uuid:esp-common-%s\r\n"
               "\r\n",
               kStLine, idHex);
  if (len <= 0 || static_cast<size_t>(len) >= sizeof(body)) {
    return;
  }
  if (m_udp.beginPacket(m_udp.remoteIP(), m_udp.remotePort()) != 1) {
    m_logger.log(LogLevel::Warn, name(), "beginPacket failed");
    return;
  }
  m_udp.write(reinterpret_cast<const uint8_t*>(body), static_cast<size_t>(len));
  if (m_udp.endPacket() != 1) {
    m_logger.log(LogLevel::Warn, name(), "SSDP reply failed");
    return;
  }
  m_logger.log(LogLevel::Info, name(), "M-SEARCH from %s:%u -> deviceId=%s",
               m_udp.remoteIP().toString().c_str(), static_cast<unsigned>(m_udp.remotePort()),
               idHex);
}

} // namespace esp_common

#endif // ARDUINO && !ESP_COMMON_NO_TCP
