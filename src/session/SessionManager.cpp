// File: src/session/SessionManager.cpp
#include "esp_common/session/SessionManager.h"

#include <string.h>

#include "esp_common/core/ILogger.h"

namespace esp_common {

Result TransportHandshakeIo::readAll(uint8_t* buf, size_t len, uint32_t timeoutMs) {
  const uint32_t deadline = nowMs() + timeoutMs;
  size_t got = 0;
  while (got < len) {
    if (!m_transport.isConnected()) {
      return Result::err(Status::Disconnected);
    }
    const uint32_t now = nowMs();
    if (static_cast<int32_t>(now - deadline) >= 0) {
      return Result::err(Status::Timeout);
    }
    size_t n = 0;
    const Result r = m_transport.read(buf + got, len - got, deadline - now, n);
    if (!r.isOk() && r.status != Status::Timeout) {
      return r;
    }
    got += n;
  }
  return Result::ok();
}

// ---------------------------------------------------------------------------

SessionManager::SessionManager(IFrameCodec& codec, IHandshake& handshake,
                               ICommandDispatcher& dispatcher, ILogger& logger)
    : m_codec(codec), m_handshake(handshake), m_dispatcher(dispatcher), m_logger(logger) {}

Result SessionManager::handshakeAsServer(ITransport& io, HandshakeResult& out) {
  TransportHandshakeIo hsIo(io);
  const Result r = m_handshake.startAsServer(hsIo, out);
  if (!r.isOk()) {
    m_logger.log(LogLevel::Warn, "SessionMgr", "server handshake failed: %s (rc=%d)",
                 statusName(r.status), r.code);
  }
  return r;
}

Result SessionManager::handshakeAsClient(ITransport& io, const HandshakeCredentials& creds,
                                         HandshakeResult& out) {
  TransportHandshakeIo hsIo(io);
  const Result r = m_handshake.startAsClient(hsIo, creds, out);
  if (!r.isOk()) {
    m_logger.log(LogLevel::Warn, "SessionMgr", "client handshake failed: %s (rc=%d)",
                 statusName(r.status), r.code);
  }
  return r;
}

Result SessionManager::registerSession(ITransport* transport, std::unique_ptr<ITransport> owned,
                                       const HandshakeResult& hs, ISession** out) {
  if (transport == nullptr) {
    return Result::err(Status::InvalidArgument);
  }
  std::unique_ptr<ICrypto> crypto = CryptoFactory::create(hs.cryptoMode);
  if (!crypto) {
    return Result::err(Status::Unsupported);
  }
  if (!crypto->setKey(hs.sessionKey, hs.keyLength)) {
    return Result::err(Status::CryptoError);
  }

  SessionConfig cfg;
  cfg.sessionId = hs.sessionId;
  cfg.cryptoMode = hs.cryptoMode;
  cfg.maxFrameSize = hs.maxFrameSize;
  memcpy(cfg.baseIv, hs.baseIv, kBaseIvLen);
  memcpy(cfg.token, hs.token, kTokenLen);

  auto session = std::unique_ptr<ISession>(
      new Session(cfg, *transport, m_codec, std::move(crypto), m_dispatcher, m_logger));

  SessionEntry entry;
  entry.transport = transport;
  entry.owned = std::move(owned);
  entry.session = std::move(session);
  if (out != nullptr) {
    *out = entry.session.get();
  }
  m_logger.log(LogLevel::Info, "SessionMgr", "session #%u established (mode=%u, frame<=%u)",
               static_cast<unsigned>(cfg.sessionId), static_cast<unsigned>(cfg.cryptoMode),
               static_cast<unsigned>(cfg.maxFrameSize));
  m_entries.push_back(std::move(entry));
  return Result::ok();
}

Result SessionManager::attachSession(std::unique_ptr<ITransport> io, const HandshakeResult& hs,
                                     ISession** out) {
  if (!io) {
    return Result::err(Status::InvalidArgument);
  }
  ITransport* raw = io.get();
  return registerSession(raw, std::move(io), hs, out);
}

Result SessionManager::attachSession(ITransport& io, const HandshakeResult& hs, ISession** out) {
  return registerSession(&io, nullptr, hs, out);
}

Result SessionManager::establishAsServer(std::unique_ptr<ITransport> io, ISession** out) {
  if (!io) {
    return Result::err(Status::InvalidArgument);
  }
  HandshakeResult hs;
  const Result r = handshakeAsServer(*io, hs);
  if (!r.isOk()) {
    return r;
  }
  return attachSession(std::move(io), hs, out);
}

Result SessionManager::establishAsClient(std::unique_ptr<ITransport> io,
                                         const HandshakeCredentials& creds, ISession** out) {
  if (!io) {
    return Result::err(Status::InvalidArgument);
  }
  HandshakeResult hs;
  const Result r = handshakeAsClient(*io, creds, hs);
  if (!r.isOk()) {
    return r;
  }
  return attachSession(std::move(io), hs, out);
}

ISession* SessionManager::findSessionByTransport(const ITransport* transport) const {
  if (transport == nullptr) {
    return nullptr;
  }
  for (const SessionEntry& entry : m_entries) {
    if (entry.transport == transport) {
      return entry.session.get();
    }
  }
  return nullptr;
}

void SessionManager::tick(uint32_t frameTimeoutMs) {
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (!it->transport->isConnected() || !it->session->isActive()) {
      m_logger.log(LogLevel::Info, "SessionMgr", "session #%u disconnected",
                   static_cast<unsigned>(it->session->id()));
      it = m_entries.erase(it);
      continue;
    }
    const Result r = it->session->pollOnce(frameTimeoutMs);
    if (r.isOk() || r.status == Status::Timeout) {
      ++it;
      continue;
    }
    if (r.status == Status::Disconnected) {
      it->transport->close();
      continue; // удалим на следующей итерации цикла
    }
    // Одиночные ошибки CRC/протокола не рвут сессию — канал может восстановиться.
    m_logger.log(LogLevel::Warn, "SessionMgr", "session #%u poll: %s (rc=%d)",
                 static_cast<unsigned>(it->session->id()), statusName(r.status), r.code);
    ++it;
  }
}

} // namespace esp_common
