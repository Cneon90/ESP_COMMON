// File: src/session/Session.cpp
#include "esp_common/session/Session.h"

#include <string.h>

#include <algorithm>

namespace esp_common {
namespace {

/// Строгое «a больше b» для 16-битных счётчиков с учётом переполнения.
inline bool seqGreater(uint16_t a, uint16_t b) {
  return static_cast<int16_t>(static_cast<uint16_t>(a - b)) > 0;
}

/// Валидный ли код статуса пришёл по проводу.
inline Status mapStatusByte(uint8_t v) {
  return (v <= static_cast<uint8_t>(Status::IoError)) ? static_cast<Status>(v)
                                                      : Status::ProtocolError;
}

} // namespace

Session::Session(const SessionConfig& cfg, ITransport& transport, IFrameCodec& codec,
                 std::unique_ptr<ICrypto> crypto, ICommandDispatcher& dispatcher, ILogger& logger)
    : m_cfg(cfg),
      m_transport(transport),
      m_codec(codec),
      m_crypto(std::move(crypto)),
      m_dispatcher(dispatcher),
      m_logger(logger) {
  if (m_crypto) {
    m_crypto->setBaseIv(m_cfg.baseIv);
  }
}

void Session::close() { m_active = false; }

// ---------------------------------------------------------------------------
// Крипто тела фрейма (на месте, без лишних аллокаций)
// ---------------------------------------------------------------------------

Result Session::encryptPayload(Frame& f) {
  if (!m_crypto) {
    return Result::err(Status::CryptoError);
  }
  if (f.payload.empty() || m_cfg.cryptoMode == CryptoPlain) {
    return Result::ok();
  }
  return m_crypto->encrypt(f.seq, f.payload.data(), f.payload.size(), f.payload.data());
}

Result Session::decryptPayload(Frame& f) {
  if (!m_crypto) {
    return Result::err(Status::CryptoError);
  }
  if (f.payload.empty() || m_cfg.cryptoMode == CryptoPlain) {
    return Result::ok();
  }
  return m_crypto->decrypt(f.seq, f.payload.data(), f.payload.size(), f.payload.data());
}

// ---------------------------------------------------------------------------
// Приём/передача
// ---------------------------------------------------------------------------

Result Session::sendFrame(Frame& f) {
  if (!m_transport.isConnected()) {
    return Result::err(Status::Disconnected);
  }
  Result r = encryptPayload(f);
  if (!r.isOk()) {
    return r;
  }
  std::vector<uint8_t> wire; // единственный vector на фрейм
  r = m_codec.encode(f, wire);
  if (!r.isOk()) {
    return r;
  }
  return m_transport.write(wire.data(), wire.size(), kIoTimeoutMs);
}

Result Session::recvFrame(Frame& out, uint32_t timeoutMs) {
  const uint32_t deadline = nowMs() + timeoutMs;
  while (true) {
    size_t consumed = 0;
    const Result dr = m_codec.decode(m_rxBuffer, m_rxLen, out, consumed);
    if (dr.isOk()) {
      m_rxLen -= consumed;
      if (m_rxLen > 0) {
        memmove(m_rxBuffer, m_rxBuffer + consumed, m_rxLen);
      }
      return Result::ok();
    }
    if (dr.status != Status::NeedMoreData) {
      // ProtocolError/CrcError — рассинхронизация потока: сбрасываем буфер.
      m_rxLen = 0;
      return dr;
    }

    if (m_rxLen >= sizeof(m_rxBuffer)) {
      m_rxLen = 0; // фрейм не помещается — сброс и ждём магию заново
      return Result::err(Status::ProtocolError);
    }
    const uint32_t now = nowMs();
    const bool expired = static_cast<int32_t>(now - deadline) >= 0;
    const uint32_t remain = expired ? 0u : static_cast<uint32_t>(deadline - now);

    size_t n = 0;
    const Result rr = m_transport.read(m_rxBuffer + m_rxLen, sizeof(m_rxBuffer) - m_rxLen, remain, n);
    if (!rr.isOk() && rr.status != Status::Timeout) {
      return rr; // Disconnected и т.п.
    }
    m_rxLen += n;
    if (n == 0) {
      const uint32_t now2 = nowMs();
      if (static_cast<int32_t>(now2 - deadline) >= 0) {
        return Result::err(Status::Timeout);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Роль «к нам подключились»: обработка входящего запроса
// ---------------------------------------------------------------------------

Result Session::handleIncomingFrame(Frame& req) {
  if (req.isResponse()) {
    // Ответы принимает только sendRequest(); здесь они не ожидаются.
    m_logger.log(LogLevel::Debug, "Session", "drop unexpected response seq=%u cmd=0x%04X", req.seq,
                 req.cmd);
    return Result::ok();
  }
  if (req.cryptoMode != m_cfg.cryptoMode) {
    m_logger.log(LogLevel::Warn, "Session", "crypto mode mismatch: %u vs session %u",
                 static_cast<unsigned>(req.cryptoMode), static_cast<unsigned>(m_cfg.cryptoMode));
    return Result::err(Status::ProtocolError);
  }
  if (req.payload.size() > m_cfg.maxFrameSize) {
    return Result::err(Status::ProtocolError);
  }

  Result r = decryptPayload(req);
  if (!r.isOk()) {
    return r;
  }

  // Replay-защита: seq запросов строго монотонно растёт в сессии.
  if (m_hasRx && !seqGreater(req.seq, m_lastRxSeq)) {
    m_logger.log(LogLevel::Warn, "Session", "replay detected: seq=%u last=%u", req.seq, m_lastRxSeq);
    const Result er = sendErrorResponse(req, Status::ReplayDetected);
    return er.isOk() ? Result::err(Status::ReplayDetected) : er;
  }
  m_lastRxSeq = req.seq;
  m_hasRx = true;

  // Командная логика: роль и транспорт сюда не доходят.
  CommandRequest creq;
  creq.cmd = req.cmd;
  creq.param = req.param;
  creq.data = req.payload;
  CommandResponse cresp;
  cresp.param = req.param;
  cresp.status = Status::Ok;
  m_dispatcher.dispatch(creq, cresp);

  // Ответ: flags.response, тот же seq, error-флаг по статусу.
  Frame out;
  out.markResponse();
  out.cryptoMode = m_cfg.cryptoMode;
  out.seq = req.seq;
  out.cmd = req.cmd;
  out.param = cresp.param;
  if (cresp.status != Status::Ok) {
    out.markError();
    out.payload.push_back(static_cast<uint8_t>(cresp.status)); // [0] = Status
    out.payload.insert(out.payload.end(), cresp.data.begin(), cresp.data.end());
  } else {
    out.payload = cresp.data;
  }
  return sendFrame(out);
}

Result Session::sendErrorResponse(const Frame& badReq, Status code) {
  Frame out;
  out.markResponse();
  out.markError();
  out.cryptoMode = m_cfg.cryptoMode;
  out.seq = badReq.seq;
  out.cmd = badReq.cmd;
  out.param = badReq.param;
  out.payload.push_back(static_cast<uint8_t>(code));
  return sendFrame(out);
}

Result Session::pollOnce(uint32_t timeoutMs) {
  if (!isActive()) {
    return Result::err(Status::Disconnected);
  }
  Frame in;
  const Result r = recvFrame(in, timeoutMs);
  if (!r.isOk()) {
    return r;
  }
  return handleIncomingFrame(in);
}

// ---------------------------------------------------------------------------
// Роль «мы подключились»: запрос-ответ
// ---------------------------------------------------------------------------

Result Session::sendRequest(uint16_t cmd, uint8_t param, const std::vector<uint8_t>& data,
                            CommandResponse& resp, uint32_t timeoutMs) {
  if (!isActive()) {
    return Result::err(Status::Disconnected);
  }
  if (data.size() > m_cfg.maxFrameSize) {
    return Result::err(Status::InvalidArgument);
  }

  Frame out;
  out.cryptoMode = m_cfg.cryptoMode;
  out.seq = m_txSeq;
  m_txSeq = static_cast<uint16_t>(m_txSeq + 1u); // 0 не используем
  out.cmd = cmd;
  out.param = param;
  out.payload = data;

  Result r = sendFrame(out);
  if (!r.isOk()) {
    return r;
  }

  const uint32_t deadline = nowMs() + timeoutMs;
  while (true) {
    const uint32_t now = nowMs();
    const uint32_t remain =
        static_cast<int32_t>(now - deadline) >= 0
            ? 0u
            : static_cast<uint32_t>(deadline - now);
    Frame in;
    r = recvFrame(in, remain);
    if (!r.isOk()) {
      return r; // Timeout/Disconnect/протокол
    }
    if (in.isResponse()) {
      if (in.seq != out.seq) {
        m_logger.log(LogLevel::Debug, "Session", "skip response seq=%u (waiting %u)", in.seq,
                     out.seq);
        continue;
      }
      if (in.cryptoMode != m_cfg.cryptoMode) {
        return Result::err(Status::ProtocolError);
      }
      r = decryptPayload(in);
      if (!r.isOk()) {
        return r;
      }
      resp.param = in.param;
      if (in.isError()) {
        // Правило: payload[0] = Status, дальше — данные команды.
        resp.status = in.payload.empty() ? Status::ProtocolError : mapStatusByte(in.payload[0]);
        if (!in.payload.empty()) {
          resp.data.assign(in.payload.begin() + 1, in.payload.end());
        }
      } else {
        resp.status = Status::Ok;
        resp.data = in.payload;
      }
      return Result::ok();
    }
    // Встречный запрос от пира: обслуживаем тем же набором команд и ждём ответ.
    r = handleIncomingFrame(in);
    if (!r.isOk() && r.status != Status::ReplayDetected) {
      return r;
    }
  }
}

} // namespace esp_common
