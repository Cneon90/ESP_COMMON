// File: include/esp_common/core/ILogger.h
#pragma once
/// @file ILogger.h
/// @brief Интерфейс логирования + SerialLogger для Arduino/ESP32.

#include <stdarg.h>
#include <stdio.h>

#include "esp_common/core/Result.h"

namespace esp_common {

/// Уровни логирования (порядок — важен: сравниваются как числа).
enum class LogLevel : uint8_t {
  None = 0, ///< Не логировать вовсе.
  Error = 1,
  Warn = 2,
  Info = 3,
  Debug = 4,
};

/// Человекочитаемое имя уровня.
inline const char* logLevelName(LogLevel l) {
  switch (l) {
    case LogLevel::None:  return "NONE";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Debug: return "DEBUG";
  }
  return "?";
}

/// @brief Интерфейс логгера.
///
/// Все слои зависят только от этого интерфейса (DI через конструктор).
/// Формат — printf-стиль.
class ILogger {
 public:
  virtual ~ILogger() = default;

  /// Записать сообщение уровня @p level от компонента @p tag.
#if defined(__GNUC__)
  __attribute__((format(printf, 4, 5)))
#endif
  virtual void log(LogLevel level, const char* tag, const char* format, ...) = 0;
};

#if defined(ARDUINO)

/// @brief Реализация ILogger поверх HardwareSerial (по умолчанию — USB/UART0).
class SerialLogger final : public ILogger {
 public:
  explicit SerialLogger(LogLevel minLevel = LogLevel::Info, HardwareSerial& out = Serial)
      : m_minLevel(minLevel), m_out(out) {}

  void log(LogLevel level, const char* tag, const char* format, ...) override {
    if (level > m_minLevel) {
      return;
    }
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(m_buf, sizeof(m_buf), format, args);
    va_end(args);
    if (written <= 0) {
      return;
    }
    m_out.printf("[%lu][%s][%s] %s\r\n", static_cast<unsigned long>(nowMs()),
                 logLevelName(level), tag, m_buf);
  }

 private:
  LogLevel m_minLevel;
  HardwareSerial& m_out;
  char m_buf[192];
};

#endif // ARDUINO

} // namespace esp_common
