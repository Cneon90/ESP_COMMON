// File: include/esp_common/transport/SerialTransport.h
#pragma once
/// @file SerialTransport.h
/// @brief Транспорт поверх UART/RS485 (только Arduino/ESP32).

#if defined(ARDUINO)

#include <HardwareSerial.h>

#include "esp_common/transport/ITransport.h"

namespace esp_common {

/// @brief UART/RS485-транспорт. Роль всегда Server: к нашему порту подключается
///        терминал/шлюз. RS485 — полудуплекс: при заданном DE-пине он
///        поднимается на время передачи и опускается после flush().
class SerialTransport final : public ITransport {
 public:
  /// @param rxPin/txPin  GPIO UART (-1 — пины по умолчанию 3/1);
  /// @param rs485DePin   GPIO драйвера DE/RE RS485 (-1 — не используется).
  SerialTransport(HardwareSerial& serial, uint32_t baud, int8_t rxPin = -1, int8_t txPin = -1,
                  int8_t rs485DePin = -1);

  bool open(uint32_t timeoutMs) override;
  void close() override;
  bool isConnected() const override;
  bool hasInput() const override;

  Result read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs, size_t& bytesRead) override;
  Result write(const uint8_t* buf, size_t len, uint32_t timeoutMs) override;

  TransportRole role() const override { return TransportRole::Server; }
  TransportKind kind() const override { return TransportKind::Serial; }

 private:
  HardwareSerial& m_serial;
  uint32_t m_baud;
  int8_t m_rxPin;
  int8_t m_txPin;
  int8_t m_rs485DePin;
};

} // namespace esp_common

#endif // ARDUINO
