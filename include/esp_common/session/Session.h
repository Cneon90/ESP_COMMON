// File: include/esp_common/session/Session.h
#pragma once
/// @file Session.h
/// @brief Сессия поверх транспорта: шифрование, replay-защита, команды.

#include <stdint.h>
#include <vector>

#include "esp_common/commands/ICommand.h"
#include "esp_common/core/ILogger.h"
#include "esp_common/proto/Frame.h"
#include "esp_common/proto/ICrypto.h"
#include "esp_common/proto/IFrameCodec.h"
#include "esp_common/proto/ProtocolDefs.h"
#include "esp_common/session/ISession.h"
#include "esp_common/transport/ITransport.h"

namespace esp_common {

/// Параметры сессии, полученные из handshake.
struct SessionConfig {
  uint8_t sessionId = 0;
  CryptoMode cryptoMode = CryptoPlain;
  uint16_t maxFrameSize = static_cast<uint16_t>(kMaxFrameSize);
  uint8_t baseIv[kBaseIvLen] = {0};
  uint8_t token[kTokenLen] = {0};
};

/// @brief Логика фреймов и команд внутри одного соединения.
///
/// НЕ знает роль: pollOnce() обслуживает входящие запросы (серверная сторона),
/// sendRequest() шлёт свои (клиентская). Один набор ICommand работает в обе
/// стороны. Транспорт/кодек/крипто внедряются через конструктор.
class Session final : public ISession {
 public:
  /// @param crypto  реализация ICrypto для согласованного режима (владение сессии).
  Session(const SessionConfig& cfg, ITransport& transport, IFrameCodec& codec,
          std::unique_ptr<ICrypto> crypto, ICommandDispatcher& dispatcher, ILogger& logger);

  uint8_t id() const override { return m_cfg.sessionId; }
  bool isActive() const override { return m_active && m_transport.isConnected(); }

  Result pollOnce(uint32_t timeoutMs) override;
  Result sendRequest(uint16_t cmd, uint8_t param, const std::vector<uint8_t>& data,
                     CommandResponse& resp, uint32_t timeoutMs) override;
  void close() override;

  const SessionConfig& config() const { return m_cfg; }
  uint16_t lastRxSeq() const { return m_lastRxSeq; }

 private:
  Result sendFrame(Frame& f);
  Result recvFrame(Frame& out, uint32_t timeoutMs);
  Result handleIncomingFrame(Frame& req);
  Result sendErrorResponse(const Frame& badReq, Status code);
  Result encryptPayload(Frame& f);
  Result decryptPayload(Frame& f);

  static constexpr uint32_t kIoTimeoutMs = 1000; ///< Таймаут записи фрейма.

  SessionConfig m_cfg;
  ITransport& m_transport;
  IFrameCodec& m_codec;
  std::unique_ptr<ICrypto> m_crypto;
  ICommandDispatcher& m_dispatcher;
  ILogger& m_logger;

  bool m_active = true;
  bool m_hasRx = false;
  uint16_t m_lastRxSeq = 0; ///< Последний принятый seq запросов (replay-защита).
  uint16_t m_txSeq = 1;     ///< Счётчик собственных seq (0 не используется).

  /// Накопительный буфер чтения: заголовок + максимальное тело.
  uint8_t m_rxBuffer[kFrameHeaderSize + kMaxFrameSize];
  size_t m_rxLen = 0;
};

} // namespace esp_common
