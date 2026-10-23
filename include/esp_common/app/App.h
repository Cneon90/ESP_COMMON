// File: include/esp_common/app/App.h
#pragma once
/// @file App.h
/// @brief Реализация IApp: WiFi, фабрики транспорта по роли, цикл сессий.

#include <memory>

#include "esp_common/app/IApp.h"
#include "esp_common/app/IConfigStorage.h"
#include "esp_common/commands/CommandDispatcher.h"
#include "esp_common/commands/CommandRegistry.h"
#include "esp_common/proto/FrameCodec.h"
#include "esp_common/proto/Handshake.h"
#include "esp_common/session/SessionManager.h"

namespace esp_common {

/// @brief Приложение Esp_common.
///
/// Роль задаётся AppConfig.SystemConfig.netType/netRole и выбирает фабрики
/// транспорта:
///  - NetSerial          → SerialTransport (RS485), роль Server;
///  - NetTcp             → TCP по netRole: Server (accept) или Client (connect);
///  - NetDual            → TCP по netRole И SerialTransport одновременно
///                         (например, сервисный доступ/настройка через RS485).
///
/// Логика команд (CommandDispatcher) на роль и канал не влияет: один набор
/// команд обслуживается на всех сессиях.
class App final : public IApp {
 public:
  App(AppConfig& config, ILogger& logger);

  const char* name() const override { return "esp_common.App"; }
  void setup() override { init(); }
  bool init() override;
  void loop() override;
  const AppConfig& config() const override { return m_config; }

 private:
  // Реализации существуют только на Arduino (см. src/app/App.cpp).
  void initWifi();
  void ensureListener();
  void acceptClients();
  void ensureClientConnection();
  void ensureSerialSession();
  void checkReboot();
  HandshakeCredentials handshakeCredentials() const;

  AppConfig& m_config;
  ILogger& m_logger;

  FrameCodec m_codec;
  CommandDispatcher m_dispatcher;
  CommandRegistry m_registry;
  Handshake m_handshake;
  SessionManager m_sessions;
  std::unique_ptr<IConfigStorage> m_configStorage;

  std::unique_ptr<ITransport> m_transport;       // TCP: слушатель (Server)
  std::unique_ptr<ITransport> m_serialTransport; // UART/RS485 (всегда Server)
  uint32_t m_restartAtMs = 0;                    // 0 = перезагрузка не запланирована
  bool m_initialized = false;
  uint32_t m_nextRetryMs = 0;

  static constexpr uint32_t kPollTimeoutMs = 5;        ///< Тик опроса сессий.
  static constexpr uint32_t kConnectTimeoutMs = 10000; ///< Ожидание TCP-подключения.
  static constexpr uint32_t kReconnectDelayMs = 3000;  ///< Пауза между попытками.
};

} // namespace esp_common
