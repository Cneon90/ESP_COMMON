// File: src/transport/TcpClientTransport.cpp
#if defined(ARDUINO) && !defined(ESP_COMMON_NO_TCP)

#include "esp_common/transport/TcpClientTransport.h"

#include "esp_common/core/Result.h"

namespace esp_common {

TcpClientTransport::TcpClientTransport(const std::string& host, uint16_t port)
    : m_host(host), m_port(port) {}

bool TcpClientTransport::open(uint32_t /*timeoutMs*/) {
  // WiFiClient::connect блокирует со своим внутренним таймаутом стека.
  return m_client.connect(m_host.c_str(), m_port) == 1;
}

void TcpClientTransport::close() { m_client.stop(); }

Result TcpClientTransport::read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs,
                                size_t& bytesRead) {
  bytesRead = 0;
  if (maxLen == 0) {
    return Result::err(Status::InvalidArgument);
  }
  const uint32_t deadline = nowMs() + timeoutMs;
  while (m_client.connected()) {
    const int available = m_client.available();
    if (available > 0) {
      const size_t want = (static_cast<size_t>(available) < maxLen) ? static_cast<size_t>(available)
                                                                    : maxLen;
      const int got = m_client.read(buf, static_cast<int>(want));
      if (got > 0) {
        bytesRead = static_cast<size_t>(got);
        return Result::ok();
      }
    }
    if (isExpired(deadline)) {
      return Result::err(Status::Timeout);
    }
    yield();
  }
  return Result::err(Status::Disconnected);
}

Result TcpClientTransport::write(const uint8_t* buf, size_t len, uint32_t timeoutMs) {
  const uint32_t deadline = nowMs() + timeoutMs;
  size_t written = 0;
  while (written < len && m_client.connected()) {
    written += m_client.write(buf + written, len - written);
    if (isExpired(deadline)) {
      return Result::err(Status::Timeout);
    }
    yield();
  }
  if (!m_client.connected()) {
    return Result::err(Status::Disconnected);
  }
  m_client.flush();
  return (written == len) ? Result::ok() : Result::err(Status::Timeout);
}

} // namespace esp_common

#endif // ARDUINO
