// File: src/transport/SerialTransport.cpp
#if defined(ARDUINO)

#include "esp_common/transport/SerialTransport.h"

#include "esp_common/core/Result.h"

namespace esp_common {

SerialTransport::SerialTransport(HardwareSerial& serial, uint32_t baud, int8_t rxPin,
                                 int8_t txPin, int8_t rs485DePin)
    : m_serial(serial), m_baud(baud), m_rxPin(rxPin), m_txPin(txPin), m_rs485DePin(rs485DePin) {}

bool SerialTransport::open(uint32_t /*timeoutMs*/) {
  m_serial.begin(m_baud, SERIAL_8N1, m_rxPin, m_txPin);
  if (m_rs485DePin >= 0) {
    // RS485: DE/RE на приём по умолчанию; на передачу поднимаем в write().
    pinMode(m_rs485DePin, OUTPUT);
    digitalWrite(m_rs485DePin, LOW);
  }
  return true;
}

void SerialTransport::close() { m_serial.end(); }

bool SerialTransport::isConnected() const { return m_serial; }

bool SerialTransport::hasInput() const { return m_serial.available() > 0; }

Result SerialTransport::read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs, size_t& bytesRead) {
  bytesRead = 0;
  if (maxLen == 0) {
    return Result::err(Status::InvalidArgument);
  }
  const uint32_t deadline = nowMs() + timeoutMs;
  while (true) {
    const int available = m_serial.available();
    if (available > 0) {
      const size_t want = (static_cast<size_t>(available) < maxLen) ? static_cast<size_t>(available)
                                                                    : maxLen;
      const size_t got = m_serial.read(buf, want);
      if (got > 0) {
        bytesRead = got;
        return Result::ok();
      }
    }
    if (isExpired(deadline)) {
      return Result::err(Status::Timeout);
    }
    yield();
  }
}

Result SerialTransport::write(const uint8_t* buf, size_t len, uint32_t timeoutMs) {
  const uint32_t deadline = nowMs() + timeoutMs;
  if (m_rs485DePin >= 0) {
    digitalWrite(m_rs485DePin, HIGH); // DE: передача
  }
  size_t written = 0;
  while (written < len) {
    written += m_serial.write(buf + written, len - written);
    if (isExpired(deadline)) {
      if (m_rs485DePin >= 0) {
        digitalWrite(m_rs485DePin, LOW);
      }
      return Result::err(Status::Timeout);
    }
    yield();
  }
  m_serial.flush(); // дождаться фактической передачи всех байт
  if (m_rs485DePin >= 0) {
    digitalWrite(m_rs485DePin, LOW); // отпустить линию
  }
  return Result::ok();
}

} // namespace esp_common

#endif // ARDUINO
