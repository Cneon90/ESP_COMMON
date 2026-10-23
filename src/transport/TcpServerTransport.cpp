// File: src/transport/TcpServerTransport.cpp
#if defined(ARDUINO) && !defined(ESP_COMMON_NO_TCP)

#include "esp_common/transport/TcpServerTransport.h"

#include "esp_common/core/Result.h"

namespace esp_common {

// --- TcpServerConnection -----------------------------------------------------

TcpServerConnection::TcpServerConnection(WiFiClient client) : m_client(client) {}

void TcpServerConnection::close() { m_client.stop(); }

bool TcpServerConnection::hasInput() const { return m_client.available() > 0; }

Result TcpServerConnection::read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs,
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

Result TcpServerConnection::write(const uint8_t* buf, size_t len, uint32_t timeoutMs) {
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

// --- TcpServerTransport ------------------------------------------------------

TcpServerTransport::TcpServerTransport(uint16_t port) : m_server(port), m_port(port) {}

bool TcpServerTransport::open(uint32_t /*timeoutMs*/) {
  // WiFiServer::begin() в core 2.x не возвращает статус — считаем начавшим слушать.
  m_server.begin(m_port);
  m_listening = true;
  return m_listening;
}

void TcpServerTransport::close() {
  m_server.stop();
  m_listening = false;
}

bool TcpServerTransport::accept(uint32_t timeoutMs, std::unique_ptr<ITransport>& out) {
  const uint32_t deadline = nowMs() + timeoutMs;
  do {
    WiFiClient candidate = m_server.available();
    if (candidate && candidate.connected()) {
      out.reset(new TcpServerConnection(candidate));
      return true;
    }
    yield();
  } while (!isExpired(deadline));
  return false;
}

} // namespace esp_common

#endif // ARDUINO
