// File: include/esp_common/transport/ITransport.h
#pragma once
/// @file ITransport.h
/// @brief Интерфейс транспорта. О протоколе (фреймы, handshake) не знает ничего.

#include <stddef.h>
#include <stdint.h>

#include "esp_common/core/Result.h"

namespace esp_common {

/// Роль транспорта: мы принимаем соединения или подключаемся сами.
enum class TransportRole : uint8_t {
  Server, ///< К нам подключаются (слушаем/принимаем).
  Client, ///< Мы подключаемся к чужому серверу.
};

/// Вид транспорта.
enum class TransportKind : uint8_t {
  Tcp,   ///< WiFi/TCP.
  Serial,///< UART.
};

/// @brief Байтовый транспорт с таймаутами.
///
/// Контракты:
///  - open(): сервер начинает слушать, клиент устанавливает соединение;
///  - read(): читает от 1 до maxLen байт за timeoutMs; Ok если прочитан хотя бы 1
///    байт, Timeout если не пришло ничего, Disconnected при закрытом канале;
///  - write(): записывает ВСЕ len байт; Timeout если не успели, Disconnected при
///    закрытом канале.
///
/// Реализации не буферизуют протокол — буферизацию ведёт Session.
class ITransport {
 public:
  virtual ~ITransport() = default;

  virtual bool open(uint32_t timeoutMs) = 0;
  virtual void close() = 0;
  virtual bool isConnected() const = 0;

  /// Есть ли байты во входном буфере БЕЗ чтения (для неблокирующих опросов).
  virtual bool hasInput() const = 0;

  /// Прочитать до @p maxLen байт (не блокируя дольше @p timeoutMs).
  virtual Result read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs, size_t& bytesRead) = 0;

  /// Записать ровно @p len байт.
  virtual Result write(const uint8_t* buf, size_t len, uint32_t timeoutMs) = 0;

  virtual TransportRole role() const = 0;
  virtual TransportKind kind() const = 0;
};

} // namespace esp_common
