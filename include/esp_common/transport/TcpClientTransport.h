// File: include/esp_common/transport/TcpClientTransport.h
#pragma once
/// @file TcpClientTransport.h
/// @brief TCP-клиент (WiFiClient): подключение к чужому серверу.

#if defined(ARDUINO) && !defined(ESP_COMMON_NO_TCP)

#include <string>

#include <WiFiClient.h>

#include "esp_common/transport/ITransport.h"

namespace esp_common {

/// @brief Исходящее TCP-соединение (роль Client, вид Tcp).
///
/// Логика команд та же, что у сервера: handshake выполняется после open(),
/// дальше — обычные фреймы.
class TcpClientTransport final : public ITransport {
 public:
  TcpClientTransport(const std::string& host, uint16_t port);

  bool open(uint32_t timeoutMs) override;
  void close() override;
  bool isConnected() const override { return m_client.connected(); }
  bool hasInput() const override { return m_client.available() > 0; }

  Result read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs, size_t& bytesRead) override;
  Result write(const uint8_t* buf, size_t len, uint32_t timeoutMs) override;

  TransportRole role() const override { return TransportRole::Client; }
  TransportKind kind() const override { return TransportKind::Tcp; }

 private:
  /// mutable: WiFiClient::connected() не const в core 2.x, а isConnected() const.
  mutable WiFiClient m_client;
  std::string m_host;
  uint16_t m_port;
};

} // namespace esp_common

#endif // ARDUINO
