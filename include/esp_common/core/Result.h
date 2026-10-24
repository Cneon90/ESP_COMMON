// File: include/esp_common/core/Result.h
#pragma once
/// @file Result.h
/// @brief Единый тип ошибок (Status/Result) и утилиты времени для таймаутов.

#include <stdint.h>

#if defined(ARDUINO)
  #include <Arduino.h>
#else
  #include <chrono>
#endif

namespace esp_common {

/// Коды результатов всех слоёв (транспорт, протокол, сессия, команды).
/// Значения передаются по проводу в ответе с flags.error (первый байт тела),
/// поэтому нумерация — часть протокола.
enum class Status : uint8_t {
  Ok = 0,             ///< Успех.
  InvalidArgument,    ///< Некорректные аргументы.
  NeedMoreData,       ///< Кодеку недостаточно данных (ждём остальную часть фрейма).
  ProtocolError,      ///< Нарушение протокола (магия/версия/размер/режим).
  CrcError,           ///< Несовпадение CRC-16.
  ReplayDetected,     ///< Повтор/откат seq в сессии.
  CryptoError,        ///< Ошибка криптографии (ключ, mbedTLS rc).
  AuthFailed,         ///< Отказ аутентификации handshake.
  Unsupported,        ///< Режим/функция не поддерживается.
  NotImplemented,     ///< Команда не реализована (ответ на неизвестный cmd).
  NotFound,           ///< Не найдено.
  Busy,               ///< Ресурс занят.
  NoMemory,           ///< Не хватает памяти.
  Timeout,            ///< Истёк таймаут сетевой операции.
  Disconnected,       ///< Соединение закрыто/потеряно.
  IoError,            ///< Ошибка ввода-вывода.
  NotAddressed,       ///< Пакет адресован другому устройству (handshake на
                      ///< мультиточке) — обработка прервана без ответа.
};

/// Результат операции: статус + необязательный доп. код (например, rc mbedTLS).
struct Result {
  Status status = Status::Ok;
  int32_t code = 0; ///< Доп. код (rc mbedTLS, errno и т.п.).

  bool isOk() const { return status == Status::Ok; }

  static Result ok() { return Result{}; }
  static Result err(Status s, int32_t c = 0) { Result r; r.status = s; r.code = c; return r; }
};

/// Человекочитаемое имя статуса (для логов и отладки).
inline const char* statusName(Status s) {
  switch (s) {
    case Status::Ok:             return "Ok";
    case Status::InvalidArgument: return "InvalidArgument";
    case Status::NeedMoreData:   return "NeedMoreData";
    case Status::ProtocolError:  return "ProtocolError";
    case Status::CrcError:       return "CrcError";
    case Status::ReplayDetected: return "ReplayDetected";
    case Status::CryptoError:    return "CryptoError";
    case Status::AuthFailed:     return "AuthFailed";
    case Status::Unsupported:    return "Unsupported";
    case Status::NotImplemented: return "NotImplemented";
    case Status::NotFound:       return "NotFound";
    case Status::Busy:           return "Busy";
    case Status::NoMemory:       return "NoMemory";
    case Status::Timeout:        return "Timeout";
    case Status::Disconnected:   return "Disconnected";
    case Status::IoError:        return "IoError";
    case Status::NotAddressed:   return "NotAddressed";
  }
  return "Unknown";
}

/// Текущее время в мс (Arduino millis() / std::chrono на нативных тестах).
inline uint32_t nowMs() {
#if defined(ARDUINO)
  return static_cast<uint32_t>(millis());
#else
  using namespace std::chrono;
  return static_cast<uint32_t>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}

/// Истёк ли дедлайн (корректно переживает переполнение uint32_t).
inline bool isExpired(uint32_t deadlineMs) {
  return static_cast<int32_t>(nowMs() - deadlineMs) >= 0;
}

} // namespace esp_common
