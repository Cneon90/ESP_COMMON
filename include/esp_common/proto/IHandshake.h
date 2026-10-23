// File: include/esp_common/proto/IHandshake.h
#pragma once
/// @file IHandshake.h
/// @brief Интерфейс handshake: X25519 + HKDF-SHA256, ключ по сети не передаётся.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "esp_common/core/Result.h"
#include "esp_common/proto/ProtocolDefs.h"

namespace esp_common {

/// Таймаут побайтового обмена handshake по умолчанию.
constexpr uint32_t kHandshakeTimeoutMs = 5000;

/// @brief Результат handshake — вход для создания Session.
///
/// Содержит всё, что нужно для шифрования сессии; сам ключ по сети не передаётся:
/// обе стороны получают его из ECDH + HKDF-SHA256.
struct HandshakeResult {
  uint8_t sessionKey[32] = {0}; ///< Ключ сессии (используются первые keyLength байт).
  size_t keyLength = 0;         ///< 0 (plain), 16 (AES-128), 32 (AES-256/ChaCha20).
  uint8_t baseIv[kBaseIvLen] = {0}; ///< Базовый IV сессии (из ответа).
  uint8_t token[kTokenLen] = {0};   ///< Токен сессии (из ответа).
  uint8_t sessionId = 0;            ///< Идентификатор сессии (из ответа).
  CryptoMode cryptoMode = CryptoPlain; ///< Согласованный режим шифрования.
  uint16_t maxFrameSize = static_cast<uint16_t>(kMaxFrameSize); ///< Лимит тела фрейма.
};

/// @brief Учётные данные и политика для handshake.
///
/// На сервере — ожидаемые значения и разрешённые режимы; на клиенте —
/// отправляемые значения и рекламируемая маска поддержки.
struct HandshakeCredentials {
  std::string userName; ///< Имя пользователя (NUL-паддинг на проводе).
  std::string password; ///< Пароль (NUL-паддинг на проводе).
  /// Разрешённые/поддерживаемые режимы: бит i = режим i (см. CryptoMode).
  uint8_t allowedCryptoMask = kCryptoSupportAll;
  uint16_t deviceType = 0;      ///< Тип устройства (информационно).
  NetType netType = NetTcp;     ///< Тип сети (информационно).
};

/// @brief Минимальный поток байтов, нужный handshake (I/O без знания транспорта).
///
/// Прото-слой не зависит от transport-слоя; связку ITransport → IHandshakeIo
/// делает адаптер в session-слое.
class IHandshakeIo {
 public:
  virtual ~IHandshakeIo() = default;

  /// Записать ровно @p len байт (или ошибка/таймаут).
  virtual Result writeAll(const uint8_t* buf, size_t len, uint32_t timeoutMs) = 0;
  /// Прочитать ровно @p len байт (или ошибка/таймаут).
  virtual Result readAll(uint8_t* buf, size_t len, uint32_t timeoutMs) = 0;
};

/// @brief Обмен handshake до ключа сессии.
///
/// Пакетные методы (startAsServer/startAsClient) сами ведут обмен через
/// IHandshakeIo. «Чистые» методы (processServerRequest / createClientRequest +
/// finishClient) работают с готовыми wire-структурами — их используют тесты
/// и альтернативные транспорты.
class IHandshake {
 public:
  virtual ~IHandshake() = default;

  /// @brief Роль сервера: принять запрос, проверить аутентификацию,
  ///        согласовать режим, отправить ответ. По возврату Ok — ключи в @p out.
  virtual Result startAsServer(IHandshakeIo& io, HandshakeResult& out) = 0;

  /// @brief Роль клиента: отправить запрос, получить ответ, вывести ключи в @p out.
  virtual Result startAsClient(IHandshakeIo& io, const HandshakeCredentials& creds,
                               HandshakeResult& out) = 0;

  /// Чистая серверная часть: разобрать запрос и построить ответ (без I/O).
  virtual Result processServerRequest(const HandshakeRequestWire& req,
                                      HandshakeResponseWire& resp, HandshakeResult& out) = 0;

  /// Чистая клиентская часть №1: сгенерировать пару ключей и заполнить запрос.
  virtual Result createClientRequest(HandshakeRequestWire& req) = 0;

  /// Чистая клиентская часть №2: разобрать ответ сервера и вывести ключи.
  virtual Result finishClient(const HandshakeResponseWire& resp, HandshakeResult& out) = 0;
};

} // namespace esp_common
