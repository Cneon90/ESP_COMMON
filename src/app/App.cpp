// File: src/app/App.cpp
#include "esp_common/app/App.h"

#if defined(ARDUINO)

// Профиль ESP_COMMON_NO_TCP: только UART/RS485 — из прошивки исключаются
// WiFi/lwip/TCP-транспорты (см. env:esp32dev_serial).
#if !defined(ESP_COMMON_NO_TCP)
#define ESP_COMMON_HAS_TCP 1
#include <WiFi.h>
#include "esp_common/app/DiscoveryTask.h"
#include "esp_common/transport/TcpClientTransport.h"
#include "esp_common/transport/TcpServerTransport.h"
#endif

#include "esp_common/app/PreferencesConfigStorage.h"
#include "esp_common/app/DeviceId.h"
#include "esp_common/commands/GetConfigCommand.h"
#include "esp_common/commands/PingCommand.h"
#include "esp_common/commands/RebootCommand.h"
#include "esp_common/commands/SetConfigCommand.h"
#include "esp_common/transport/SerialTransport.h"

namespace esp_common {

App::App(AppConfig& config, ILogger& logger)
    : m_config(config),
      m_logger(logger),
      m_dispatcher(logger),
      m_handshake(makeHandshakeCredentials(config.cryptoPolicy, config.system.deviceType,
                                           config.system.deviceId),
                  logger),
      m_sessions(m_codec, m_handshake, m_dispatcher, logger) {
  // NVS-хранилище на устройстве; RAM-заглушка для нативной сборки.
  m_configStorage.reset(new PreferencesConfigStorage());
}

bool App::init() {
  // 1) Настройки: дефолты + сохранённые в NVS секции (их могли править по UART).
  loadAppConfig(*m_configStorage, m_config);
  ensureDeviceId(m_config.system.deviceId); // генерация при первом старте (нули)
  m_handshake.setServerCredentials(handshakeCredentials());

  char deviceIdHex[2 * kDeviceIdLen + 1];
  deviceIdToHex(m_config.system.deviceId, deviceIdHex);
  m_logger.log(LogLevel::Info, "App",
               "init: device=%s id=%s netType=%u netRole=%u tcpPort=%u baud=%lu",
               reinterpret_cast<const char*>(m_config.system.deviceName), deviceIdHex,
               static_cast<unsigned>(m_config.system.netType),
               static_cast<unsigned>(m_config.system.netRole),
               static_cast<unsigned>(m_config.system.tcpPort),
               static_cast<unsigned long>(m_config.system.uartBaud));

  // 2) Набор команд собирается здесь; диспетчер и сессии конкретных команд не знают.
  m_registry.add(std::unique_ptr<ICommand>(new PingCommand()));
  m_registry.add(std::unique_ptr<ICommand>(
      new SetConfigCommand(m_config, *m_configStorage, m_logger)));
  m_registry.add(std::unique_ptr<ICommand>(new GetConfigCommand(m_config)));
  m_registry.add(std::unique_ptr<ICommand>(new RebootCommand(m_restartAtMs)));
  m_registry.installTo(m_dispatcher);

  // 3) Каналы по конфигу.
  const uint8_t netType = m_config.system.netType;
  if (netType == static_cast<uint8_t>(NetTcp) || netType == static_cast<uint8_t>(NetDual)) {
    initWifi();
  }
  if (netType == static_cast<uint8_t>(NetSerial) || netType == static_cast<uint8_t>(NetDual)) {
    m_serialTransport.reset(new SerialTransport(
        Serial, m_config.system.uartBaud, m_config.system.uartRxPin, m_config.system.uartTxPin,
        m_config.system.rs485DePin));
    if (!m_serialTransport->open(0)) {
      m_logger.log(LogLevel::Error, "App", "serial open failed");
    }
  }

  // 4) SSDP-обнаружение — только там, где есть WiFi (попытка join повторяется
  //    в loop(), если STA ещё не подключился).
#if defined(ESP_COMMON_HAS_TCP)
  if (netType == static_cast<uint8_t>(NetTcp) || netType == static_cast<uint8_t>(NetDual)) {
    m_discovery.reset(new DiscoveryTask(m_config, m_logger));
    m_discovery->setup();
  }
#endif

  m_initialized = true;
  return true;
}

void App::initWifi() {
#if defined(ESP_COMMON_HAS_TCP)
  if (m_config.wifiClient.enabled != 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(reinterpret_cast<const char*>(m_config.wifiClient.ssid),
               reinterpret_cast<const char*>(m_config.wifiClient.password));
    const uint32_t deadline = nowMs() + m_config.wifiClient.connectTimeoutMs;
    while (WiFi.status() != WL_CONNECTED && !isExpired(deadline)) {
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
      m_logger.log(LogLevel::Info, "App", "WiFi STA connected: %s", WiFi.localIP().toString().c_str());
    } else {
      m_logger.log(LogLevel::Error, "App", "WiFi STA connect timeout");
    }
    return;
  }
  if (m_config.wifiAp.enabled != 0) {
    WiFi.mode(WIFI_AP);
    const bool ok = WiFi.softAP(reinterpret_cast<const char*>(m_config.wifiAp.ssid),
                                reinterpret_cast<const char*>(m_config.wifiAp.password),
                                m_config.wifiAp.channel, 0, m_config.wifiAp.maxClients);
    if (ok) {
      m_logger.log(LogLevel::Info, "App", "WiFi AP '%s' up: %s",
                   reinterpret_cast<const char*>(m_config.wifiAp.ssid),
                   WiFi.softAPIP().toString().c_str());
    } else {
      m_logger.log(LogLevel::Error, "App", "WiFi AP start failed");
    }
    return;
  }
  m_logger.log(LogLevel::Warn, "App", "TCP выбран, но WiFi не настроен (wifiClient/wifiAp)");
#endif // ESP_COMMON_HAS_TCP
}

void App::loop() {
  if (!m_initialized) {
    return;
  }
  const uint8_t netType = m_config.system.netType;
#if defined(ESP_COMMON_HAS_TCP)
  const bool isServer = m_config.system.netRole == static_cast<uint8_t>(NetRole::Server);

  if (netType == static_cast<uint8_t>(NetTcp) || netType == static_cast<uint8_t>(NetDual)) {
    if (isServer) {
      ensureListener();
      acceptClients();
    } else {
      ensureClientConnection();
    }
  }
#endif // ESP_COMMON_HAS_TCP
  if (netType == static_cast<uint8_t>(NetSerial) || netType == static_cast<uint8_t>(NetDual)) {
    ensureSerialSession(); // неблокирующе: handshake только когда пир заговорил
  }

  if (m_discovery) {
    m_discovery->loop(); // SSDP: неблокирующий poll multicast-сокета
  }

  m_sessions.tick(kPollTimeoutMs);
  checkReboot();
}

HandshakeCredentials App::handshakeCredentials() const {
  HandshakeCredentials creds = makeHandshakeCredentials(m_config.cryptoPolicy,
                                                        m_config.system.deviceType,
                                                        m_config.system.deviceId);
  creds.netType = static_cast<NetType>(m_config.system.netType);
  return creds;
}

#if defined(ESP_COMMON_HAS_TCP)

void App::ensureListener() {
  if (m_transport) {
    return;
  }
  auto listener = std::unique_ptr<ITransport>(new TcpServerTransport(m_config.system.tcpPort));
  if (!listener->open(0)) {
    m_logger.log(LogLevel::Warn, "App", "listen on port %u failed, will retry",
                 static_cast<unsigned>(m_config.system.tcpPort));
    return;
  }
  m_logger.log(LogLevel::Info, "App", "listening on TCP port %u",
               static_cast<unsigned>(m_config.system.tcpPort));
  m_transport = std::move(listener);
}

void App::acceptClients() {
  if (!m_transport) {
    return;
  }
  auto* server = static_cast<TcpServerTransport*>(m_transport.get());
  std::unique_ptr<ITransport> conn;
  if (server->accept(0, conn)) {
    const Result r = m_sessions.establishAsServer(std::move(conn));
    if (!r.isOk()) {
      m_logger.log(LogLevel::Warn, "App", "accepted connection handshake failed: %s",
                   statusName(r.status));
    }
  }
}

void App::ensureClientConnection() {
  if (m_sessions.sessionCount() > 0 || !m_transport) {
    return;
  }
  if (static_cast<int32_t>(nowMs() - m_nextRetryMs) < 0) {
    return;
  }
  auto transport = std::unique_ptr<ITransport>(new TcpClientTransport(
      reinterpret_cast<const char*>(m_config.system.serverHost), m_config.system.tcpPort));
  m_logger.log(LogLevel::Info, "App", "connecting to %s:%u",
               reinterpret_cast<const char*>(m_config.system.serverHost),
               static_cast<unsigned>(m_config.system.tcpPort));
  if (!transport->open(kConnectTimeoutMs)) {
    m_logger.log(LogLevel::Warn, "App", "connect failed, retry in %u ms",
                 static_cast<unsigned>(kReconnectDelayMs));
    m_nextRetryMs = nowMs() + kReconnectDelayMs;
    return;
  }
  // Клиент подключается по адресу (serverHost), а не по deviceId: в запросе
  // targetDeviceId = 0 (broadcast) — отвечает любой сервер, услышавший запрос.
  HandshakeCredentials creds = handshakeCredentials();
  memset(creds.deviceId, 0, sizeof(creds.deviceId));
  const Result r = m_sessions.establishAsClient(std::move(transport), creds);
  if (!r.isOk()) {
    m_logger.log(LogLevel::Warn, "App", "client handshake failed: %s, retry in %u ms",
                 statusName(r.status), static_cast<unsigned>(kReconnectDelayMs));
    m_nextRetryMs = nowMs() + kReconnectDelayMs;
  }
}
#endif // ESP_COMMON_HAS_TCP

void App::ensureSerialSession() {
  if (!m_serialTransport) {
    return;
  }
  // Сессия уже живёт поверх этого транспорта?
  if (m_sessions.findSessionByTransport(m_serialTransport.get()) != nullptr) {
    return;
  }
  // Пир молчит — не блокируем кооперативный цикл ожиданием handshake.
  if (!m_serialTransport->isConnected() || !m_serialTransport->hasInput()) {
    return;
  }
  HandshakeResult hs;
  const Result r = m_sessions.handshakeAsServer(*m_serialTransport, hs);
  if (!r.isOk()) {
    m_logger.log(LogLevel::Debug, "App", "serial handshake failed: %s", statusName(r.status));
    return; // транспорт сохранён, повтор — на следующем обращении пира
  }
  m_sessions.attachSession(*m_serialTransport, hs);
}

void App::checkReboot() {
  if (m_restartAtMs == 0 || !isExpired(m_restartAtMs)) {
    return;
  }
  m_logger.log(LogLevel::Info, "App", "rebooting to apply settings...");
  ESP.restart();
}

} // namespace esp_common

#else // !ARDUINO — нативная сборка (тесты): App не работает без Arduino.

namespace esp_common {

App::App(AppConfig& config, ILogger& logger)
    : m_config(config),
      m_logger(logger),
      m_dispatcher(logger),
      m_handshake(makeHandshakeCredentials(config.cryptoPolicy, config.system.deviceType,
                                           config.system.deviceId),
                  logger),
      m_sessions(m_codec, m_handshake, m_dispatcher, logger) {
  m_configStorage.reset(new RamConfigStorage());
}

bool App::init() {
  m_logger.log(LogLevel::Warn, "App", "App requires Arduino/ESP32 build");
  return false;
}

void App::loop() {}

} // namespace esp_common

#endif // ARDUINO
