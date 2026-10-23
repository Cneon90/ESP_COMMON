// File: include/esp_common/transport/TcpServerTransport.h
#pragma once
/// @file TcpServerTransport.h
/// @brief TCP-сервер (WiFiServer) + принятое соединение как ITransport.

#if defined(ARDUINO) && !defined(ESP_COMMON_NO_TCP)

#include <WiFiClient.h>
#include <WiFiServer.h>

#include <memory>

#include "esp_common/transport/ITransport.h"

namespace esp_common {

/// @brief Принятое сервером соединение (роль Server, вид Tcp).
class TcpServerConnection final : public ITransport {
 public:
  explicit TcpServerConnection(WiFiClient client);

  bool open(uint32_t /*timeoutMs*/) override { return m_client.connected(); }
  void close() override;
  bool isConnected() const override { return m_client.connected(); }
  bool hasInput() const override;

  Result read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs, size_t& bytesRead) override;
  Result write(const uint8_t* buf, size_t len, uint32_t timeoutMs) override;

  TransportRole role() const override { return TransportRole::Server; }
  TransportKind kind() const override { return TransportKind::Tcp; }

 private:
  /// mutable: WiFiClient::connected() не const в core 2.x, а isConnected() const.
  mutable WiFiClient m_client;
};

/// @brief Слушающий TCP-сокет (роль Server).
///
/// Сам по себе ввод-вывод не выполняет: read/write возвращают Unsupported.
/// Новые клиенты — через accept().
class TcpServerTransport final : public ITransport {
 public:
  explicit TcpServerTransport(uint16_t port);

  bool open(uint32_t timeoutMs) override;
  void close() override;
  bool isConnected() const override { return m_listening; }
  bool hasInput() const override { return false; } // слушающий сокет не читает

  Result read(uint8_t* /*buf*/, size_t /*maxLen*/, uint32_t /*timeoutMs*/,
              size_t& /*bytesRead*/) override {
    return Result::err(Status::Unsupported); // слушающий сокет не читает
  }
  Result write(const uint8_t* /*buf*/, size_t /*len*/, uint32_t /*timeoutMs*/) override {
    return Result::err(Status::Unsupported); // слушающий сокет не пишет
  }

  /// @brief Попытаться принять соединение (неблокирующий опрос за timeoutMs).
  /// @return true и объект в @p out, если клиент подключился.
  bool accept(uint32_t timeoutMs, std::unique_ptr<ITransport>& out);

  TransportRole role() const override { return TransportRole::Server; }
  TransportKind kind() const override { return TransportKind::Tcp; }

 private:
  WiFiServer m_server;
  uint16_t m_port;
  bool m_listening = false;
};

} // namespace esp_common

#endif // ARDUINO
