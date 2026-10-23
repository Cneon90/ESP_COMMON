// File: include/esp_common/session/SessionManager.h
#pragma once
/// @file SessionManager.h
/// @brief Установление сессий (handshake) и реестр активных сессий.

#include <memory>
#include <vector>

#include "esp_common/proto/CryptoFactory.h"
#include "esp_common/proto/IHandshake.h"
#include "esp_common/session/Session.h"

namespace esp_common {

/// @brief Адаптер ITransport → IHandshakeIo (побайтовый обмен с таймаутами).
class TransportHandshakeIo final : public IHandshakeIo {
 public:
  explicit TransportHandshakeIo(ITransport& transport) : m_transport(transport) {}

  Result writeAll(const uint8_t* buf, size_t len, uint32_t timeoutMs) override {
    if (!m_transport.isConnected()) {
      return Result::err(Status::Disconnected);
    }
    return m_transport.write(buf, len, timeoutMs);
  }

  Result readAll(uint8_t* buf, size_t len, uint32_t timeoutMs) override;

 private:
  ITransport& m_transport;
};

/// Активная сессия; транспорт либо принадлежит записи (TCP-приём), либо
/// находится снаружи (UART — транспорт живёт в App постоянно).
struct SessionEntry {
  ITransport* transport = nullptr;
  std::unique_ptr<ISession> session;
  std::unique_ptr<ITransport> owned; ///< Пусто, если транспорт внешний.
};

/// @brief Владелец сессий. Роль задаётся тем, какой метод установления вызван:
///        handshakeAsServer() — «к нам подключились», handshakeAsClient() —
///        «мы подключились». Логика команд на роль не влияет. Одновременно
///        могут жить сессии разных каналов (TCP + UART).
class SessionManager {
 public:
  SessionManager(IFrameCodec& codec, IHandshake& handshake, ICommandDispatcher& dispatcher,
                 ILogger& logger);

  /// @brief Провести handshake как сервер (без смены владения транспортом).
  Result handshakeAsServer(ITransport& io, HandshakeResult& out);

  /// @brief Провести handshake как клиент (без смены владения транспортом).
  Result handshakeAsClient(ITransport& io, const HandshakeCredentials& creds, HandshakeResult& out);

  /// @brief Зарегистрировать сессию поверх готового handshake (транспорт — наш).
  Result attachSession(std::unique_ptr<ITransport> io, const HandshakeResult& hs,
                       ISession** out = nullptr);

  /// @brief Зарегистрировать сессию поверх готового handshake (внешний транспорт).
  Result attachSession(ITransport& io, const HandshakeResult& hs, ISession** out = nullptr);

  /// @brief Провести handshake как сервер и attach (удобство для принятого TCP).
  Result establishAsServer(std::unique_ptr<ITransport> io, ISession** out = nullptr);

  /// @brief Провести handshake как клиент и attach (удобство для исходящего TCP).
  Result establishAsClient(std::unique_ptr<ITransport> io, const HandshakeCredentials& creds,
                           ISession** out = nullptr);

  /// @brief Обслужить все сессии; отвалившиеся — удалить.
  void tick(uint32_t frameTimeoutMs);

  size_t sessionCount() const { return m_entries.size(); }

  /// Сессия, работающая поверх данного транспорта (nullptr — нет).
  ISession* findSessionByTransport(const ITransport* transport) const;

 private:
  Result registerSession(ITransport* transport, std::unique_ptr<ITransport> owned,
                         const HandshakeResult& hs, ISession** out);

  IFrameCodec& m_codec;
  IHandshake& m_handshake;
  ICommandDispatcher& m_dispatcher;
  ILogger& m_logger;
  std::vector<SessionEntry> m_entries;
};

} // namespace esp_common
