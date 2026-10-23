// File: include/esp_common/session/ISession.h
#pragma once
/// @file ISession.h
/// @brief Интерфейс сессии. Роли (сервер/клиент) не знает — один набор команд
///        работает в обе стороны.

#include <stdint.h>
#include <vector>

#include "esp_common/commands/ICommand.h"
#include "esp_common/core/Result.h"

namespace esp_common {

/// @brief Установленная (после handshake) сессия.
///
/// pollOnce() — режим «к нам подключились»: принять запрос, исполнить команду,
/// отправить ответ. sendRequest() — режим «мы подключились»: отправить запрос
/// и дождаться ответа. Оба доступны на одном объекте — роли задаёт вызывающий.
class ISession {
 public:
  virtual ~ISession() = default;

  /// Идентификатор сессии (из handshake-ответа).
  virtual uint8_t id() const = 0;

  /// Жива ли сессия (транспорт подключён и сессия не закрыта).
  virtual bool isActive() const = 0;

  /// @brief Принять и обработать ОДИН входящий фрейм (если он есть за timeoutMs).
  /// @return Ok — фрейм обработан; Timeout — за время ничего не пришло;
  ///         прочие — ошибки канала/протокола.
  virtual Result pollOnce(uint32_t timeoutMs) = 0;

  /// @brief Отправить запрос и дождаться ответа (не дольше timeoutMs).
  virtual Result sendRequest(uint16_t cmd, uint8_t param, const std::vector<uint8_t>& data,
                             CommandResponse& resp, uint32_t timeoutMs) = 0;

  /// Локально закрыть сессию.
  virtual void close() = 0;
};

} // namespace esp_common
