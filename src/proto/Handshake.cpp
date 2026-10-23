// File: src/proto/Handshake.cpp
#include "esp_common/proto/Handshake.h"

#include <string.h>

#include "esp_common/proto/CryptoFactory.h"

namespace esp_common {
namespace {

/// info для HKDF (фиксировано протоколом).
const uint8_t kKdfInfo[] = "EspCommon.v1";
/// Персонализация CTR-DRBG.
const unsigned char kDrbgPers[] = "esp-common-handshake";

/// f_rng для mbedTLS на базе CTR-DRBG.
int rngCallback(void* ctx, unsigned char* out, size_t len) {
  return mbedtls_ctr_drbg_random(ctx, out, len);
}

/// Резервный источник энтропии (если системный недоступен — например, на нативных
/// тестах): xorshift от времени и адресов стека. Только для отладки/тестов.
int fallbackEntropy(void*, unsigned char* out, size_t len) {
  static uint32_t s = 0;
  if (s == 0) {
    s = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(out)) ^ nowMs() ^ 0x9E3779B9u;
    if (s == 0) {
      s = 0x1234567u;
    }
  }
  for (size_t i = 0; i < len; ++i) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    out[i] = static_cast<uint8_t>(s);
  }
  return 0;
}

/// Сравнение NUL-паддингового поля запроса со строкой.
bool requestFieldEquals(const HandshakeRequestWire& req, bool isUserName,
                        const std::string& expected) {
  char buf[kPasswordLen + 1];
  if (isUserName) {
    req.getUser(buf);
  } else {
    req.getPassword(buf);
  }
  return expected.compare(buf) == 0;
}

/// Согласовать режим: старший общий бит масок (ChaCha20 → AES-256 → AES-128 → plain).
bool negotiateCryptoMode(uint8_t serverMask, uint8_t clientMask, CryptoMode& out) {
  static const CryptoMode kOrder[kCryptoModeCount] = {CryptoChaCha20, CryptoAes256Ctr,
                                                      CryptoAes128Ctr, CryptoPlain};
  for (uint8_t i = 0; i < kCryptoModeCount; ++i) {
    if (cryptoAllowed(serverMask, kOrder[i]) && cryptoAllowed(clientMask, kOrder[i])) {
      out = kOrder[i];
      return true;
    }
  }
  return false;
}

} // namespace

Handshake::Handshake(const HandshakeCredentials& serverCreds, ILogger& logger)
    : m_logger(logger), m_serverCreds(serverCreds) {
  mbedtls_ecp_group_init(&m_group);
  mbedtls_mpi_init(&m_priv);
  mbedtls_ecp_point_init(&m_pub);
  mbedtls_entropy_init(&m_entropy);
  mbedtls_ctr_drbg_init(&m_drbg);

  int rc = mbedtls_ecp_group_load(&m_group, MBEDTLS_ECP_DP_CURVE25519);
  if (rc == 0) {
    rc = mbedtls_ctr_drbg_seed(&m_drbg, mbedtls_entropy_func, &m_entropy, kDrbgPers,
                               sizeof(kDrbgPers) - 1u);
    if (rc != 0) {
      m_logger.log(LogLevel::Warn, "Handshake", "entropy seed failed rc=%d, using fallback", rc);
      rc = mbedtls_ctr_drbg_seed(&m_drbg, fallbackEntropy, nullptr, kDrbgPers,
                                 sizeof(kDrbgPers) - 1u);
    }
  }
  m_rngOk = (rc == 0);
  if (!m_rngOk) {
    m_logger.log(LogLevel::Error, "Handshake", "init failed rc=%d", rc);
  }
}

Handshake::~Handshake() {
  mbedtls_mpi_free(&m_priv);
  mbedtls_ecp_point_free(&m_pub);
  mbedtls_ctr_drbg_free(&m_drbg);
  mbedtls_entropy_free(&m_entropy);
  mbedtls_ecp_group_free(&m_group);
}

// ---------------------------------------------------------------------------
// Криптографическое ядро (X25519)
// ---------------------------------------------------------------------------

Result Handshake::generateKeyPair() {
  if (!m_rngOk) {
    return Result::err(Status::CryptoError);
  }
  mbedtls_mpi_free(&m_priv);
  mbedtls_ecp_point_free(&m_pub);
  mbedtls_mpi_init(&m_priv);
  mbedtls_ecp_point_init(&m_pub);

  // Приватный ключ с клампом по RFC 7748; публичный = priv * G.
  int rc = mbedtls_ecp_gen_privkey(&m_group, &m_priv, &rngCallback, &m_drbg);
  if (rc == 0) {
    rc = mbedtls_ecp_mul(&m_group, &m_pub, &m_priv, &m_group.G, &rngCallback, &m_drbg);
  }
  if (rc != 0) {
    m_hasKeypair = false;
    return Result::err(Status::CryptoError, rc);
  }
  m_hasKeypair = true;
  return Result::ok();
}

Result Handshake::writePublicKey(uint8_t out[kPubKeyLen]) {
  if (!m_hasKeypair) {
    return Result::err(Status::CryptoError);
  }
  // Для Montgomery-кривых mbedTLS кодирует координату X как little-endian 32 байта.
  const int rc = mbedtls_mpi_write_binary_le(&m_pub.X, out, kPubKeyLen);
  return (rc == 0) ? Result::ok() : Result::err(Status::CryptoError, rc);
}

Result Handshake::computeSharedSecret(const uint8_t peerPub[kPubKeyLen],
                                      uint8_t outSecret[kEcdhSecretLen]) {
  if (!m_hasKeypair || !m_rngOk) {
    return Result::err(Status::CryptoError);
  }

  mbedtls_ecp_point peer;
  mbedtls_ecp_point_init(&peer);

  Result result = Result::ok();
  int rc = mbedtls_ecp_point_read_binary(&m_group, &peer, peerPub, kPubKeyLen);
  if (rc == 0) {
    rc = mbedtls_ecp_check_pubkey(&m_group, &peer);
  }
  if (rc == 0) {
    // Z = priv * peerPub; для Curve25519 значима только координата X.
    mbedtls_ecp_point zPoint;
    mbedtls_ecp_point_init(&zPoint);
    rc = mbedtls_ecp_mul(&m_group, &zPoint, &m_priv, &peer, &rngCallback, &m_drbg);
    if (rc == 0) {
      rc = mbedtls_mpi_write_binary_le(&zPoint.X, outSecret, kEcdhSecretLen);
    }
    mbedtls_ecp_point_free(&zPoint);
  }
  if (rc != 0) {
    result = Result::err(Status::CryptoError, rc);
  }

  // Вырожденные ключи малого порядка дают нулевой секрет — отвергаем.
  if (result.isOk()) {
    uint8_t acc = 0;
    for (size_t i = 0; i < kEcdhSecretLen; ++i) {
      acc = static_cast<uint8_t>(acc | outSecret[i]);
    }
    if (acc == 0) {
      result = Result::err(Status::CryptoError);
    }
  }

  mbedtls_ecp_point_free(&peer);
  return result;
}

// ---------------------------------------------------------------------------
// HKDF-SHA256 (RFC 5869)
// ---------------------------------------------------------------------------

bool Handshake::hkdfSha256(const uint8_t* ikm, size_t ikmLen, const uint8_t* salt, size_t saltLen,
                           const uint8_t* info, size_t infoLen, uint8_t* okm, size_t okmLen) {
  if (infoLen > 255u || ikm == nullptr || okm == nullptr) {
    return false;
  }
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) {
    return false;
  }

  // --- Extract: PRK = HMAC-SHA256(salt, IKM); пустой salt = нули длины хеша.
  static const uint8_t kZeroSalt[32] = {0};
  if (salt == nullptr || saltLen == 0) {
    salt = kZeroSalt;
    saltLen = sizeof(kZeroSalt);
  }
  uint8_t prk[32];
  if (mbedtls_md_hmac(md, salt, saltLen, ikm, ikmLen, prk) != 0) {
    return false;
  }

  // --- Expand: T(i) = HMAC(PRK, T(i-1) || info || i).
  uint8_t t[32] = {0};
  size_t tLen = 0;
  size_t done = 0;
  uint8_t counter = 1;
  uint8_t block[32 + 255 + 1];
  while (done < okmLen) {
    memcpy(block, t, tLen);
    if (infoLen > 0) {
      memcpy(block + tLen, info, infoLen);
    }
    block[tLen + infoLen] = counter;
    if (mbedtls_md_hmac(md, prk, sizeof(prk), block, tLen + infoLen + 1u, t) != 0) {
      return false;
    }
    tLen = sizeof(t);
    const size_t chunk = (okmLen - done < tLen) ? (okmLen - done) : tLen;
    memcpy(okm + done, t, chunk);
    done += chunk;
    ++counter;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Согласованный вывод ключей сессии
// ---------------------------------------------------------------------------

Result Handshake::deriveSessionKeys(const uint8_t secret[kEcdhSecretLen],
                                    const uint8_t baseIv[kBaseIvLen],
                                    const uint8_t token[kTokenLen], uint8_t sessionId,
                                    CryptoMode mode, HandshakeResult& out) {
  out.cryptoMode = mode;
  out.keyLength = CryptoFactory::keyLengthFor(mode);
  out.maxFrameSize = static_cast<uint16_t>(kMaxFrameSize);
  out.sessionId = sessionId;
  memcpy(out.baseIv, baseIv, kBaseIvLen);
  memcpy(out.token, token, kTokenLen);

  if (out.keyLength == 0) {
    memset(out.sessionKey, 0, sizeof(out.sessionKey));
    return Result::ok(); // plain — ключ не нужен
  }

  // salt = baseIV[8] || sessionToken[4] || sessionId[1]
  uint8_t salt[kBaseIvLen + kTokenLen + 1];
  memcpy(salt, baseIv, kBaseIvLen);
  memcpy(salt + kBaseIvLen, token, kTokenLen);
  salt[kBaseIvLen + kTokenLen] = sessionId;

  if (!hkdfSha256(secret, kEcdhSecretLen, salt, sizeof(salt), kKdfInfo, sizeof(kKdfInfo) - 1u,
                  out.sessionKey, out.keyLength)) {
    return Result::err(Status::CryptoError);
  }
  return Result::ok();
}

void Handshake::fillErrorResponse(HandshakeResponseWire& resp, HandshakeStatus st) const {
  resp.init();
  resp.setStatus(st);
  resp.setMaxFrameSize(static_cast<uint16_t>(kMaxFrameSize));
  resp.setCrc(resp.crcOfPacket());
}

// ---------------------------------------------------------------------------
// Роль сервера
// ---------------------------------------------------------------------------

Result Handshake::processServerRequest(const HandshakeRequestWire& req,
                                       HandshakeResponseWire& resp, HandshakeResult& out) {
  // Структурный мусор: ответ вообще не отправляем.
  if (req.magic != kHandshakeReqMagic) {
    return Result::err(Status::ProtocolError);
  }
  if (req.getCrc() != req.crcOfPacket()) {
    return Result::err(Status::CrcError);
  }
  if (req.protoVer != kProtocolVersion) {
    fillErrorResponse(resp, HsBadVersion);
    return Result::err(Status::ProtocolError);
  }

  // Аутентификация.
  if (!requestFieldEquals(req, true, m_serverCreds.userName) ||
      !requestFieldEquals(req, false, m_serverCreds.password)) {
    fillErrorResponse(resp, HsBadAuth);
    return Result::err(Status::AuthFailed);
  }

  // Согласование режима шифрования: пересечение политики сервера и рекламы
  // клиента, ограниченное режимами, доступными в этой сборке mbedTLS.
  CryptoMode mode = CryptoPlain;
  const uint8_t serverMask =
      static_cast<uint8_t>(m_serverCreds.allowedCryptoMask & CryptoFactory::supportedModesMask());
  if (!negotiateCryptoMode(serverMask, req.cryptoSupport, mode)) {
    fillErrorResponse(resp, HsCryptoUnsupported);
    return Result::err(Status::Unsupported);
  }

  if (!m_rngOk) {
    fillErrorResponse(resp, HsInternal);
    return Result::err(Status::CryptoError);
  }

  // Ключи и случайные параметры сессии.
  Result r = generateKeyPair();
  if (!r.isOk()) {
    fillErrorResponse(resp, HsInternal);
    return r;
  }
  uint8_t secret[kEcdhSecretLen];
  r = computeSharedSecret(req.pubKey, secret);
  if (!r.isOk()) {
    fillErrorResponse(resp, HsInternal);
    return r;
  }

  uint8_t baseIv[kBaseIvLen];
  uint8_t token[kTokenLen];
  if (mbedtls_ctr_drbg_random(&m_drbg, baseIv, kBaseIvLen) != 0 ||
      mbedtls_ctr_drbg_random(&m_drbg, token, kTokenLen) != 0) {
    fillErrorResponse(resp, HsInternal);
    return Result::err(Status::CryptoError);
  }
  const uint8_t sessionId = m_nextSessionId;
  m_nextSessionId = static_cast<uint8_t>((m_nextSessionId % 255u) + 1u);

  r = deriveSessionKeys(secret, baseIv, token, sessionId, mode, out);
  if (!r.isOk()) {
    fillErrorResponse(resp, HsInternal);
    return r;
  }

  // Ответ сервера.
  resp.init();
  resp.setStatus(HsOk);
  resp.sessionId = sessionId;
  resp.setCryptoMode(mode);
  resp.setToken(loadBE32(token));
  r = writePublicKey(resp.pubKey);
  if (!r.isOk()) {
    fillErrorResponse(resp, HsInternal);
    return r;
  }
  memcpy(resp.baseIV, baseIv, kBaseIvLen);
  resp.setMaxFrameSize(out.maxFrameSize);
  resp.setDeviceTime(static_cast<uint32_t>(nowMs() / 1000u));
  resp.setCrc(resp.crcOfPacket());
  return Result::ok();
}

Result Handshake::startAsServer(IHandshakeIo& io, HandshakeResult& out) {
  HandshakeRequestWire req;
  uint8_t buf[sizeof(HandshakeRequestWire)];
  const Result r = io.readAll(buf, sizeof(buf), kHandshakeTimeoutMs);
  if (!r.isOk()) {
    return r;
  }
  memcpy(&req, buf, sizeof(req));

  HandshakeResponseWire resp;
  const Result pr = processServerRequest(req, resp, out);
  // Отправляем ответ, если он вообще сформирован (всё, кроме мусора без магии/CRC).
  if (pr.isOk() || pr.status == Status::AuthFailed || pr.status == Status::Unsupported ||
      pr.status == Status::ProtocolError || pr.status == Status::CryptoError) {
    const Result wr = io.writeAll(reinterpret_cast<const uint8_t*>(&resp), sizeof(resp),
                                  kHandshakeTimeoutMs);
    if (!wr.isOk()) {
      return wr;
    }
  }
  return pr;
}

// ---------------------------------------------------------------------------
// Роль клиента
// ---------------------------------------------------------------------------

Result Handshake::createClientRequest(HandshakeRequestWire& req) {
  if (!m_rngOk) {
    return Result::err(Status::CryptoError);
  }
  Result r = generateKeyPair();
  if (!r.isOk()) {
    return r;
  }

  req.init();
  req.setDeviceType(m_clientCreds.deviceType);
  req.netType = static_cast<uint8_t>(m_clientCreds.netType);
  // Рекламируем только режимы, реально доступные в этой сборке mbedTLS.
  req.cryptoSupport =
      static_cast<uint8_t>(m_clientCreds.allowedCryptoMask & CryptoFactory::supportedModesMask());
  req.setUser(m_clientCreds.userName.c_str());
  req.setPassword(m_clientCreds.password.c_str());
  r = writePublicKey(req.pubKey);
  if (!r.isOk()) {
    return r;
  }
  req.setCurTime(static_cast<uint32_t>(nowMs() / 1000u));
  req.setCrc(req.crcOfPacket());
  return Result::ok();
}

Result Handshake::finishClient(const HandshakeResponseWire& resp, HandshakeResult& out) {
  if (resp.magic != kHandshakeRespMagic) {
    return Result::err(Status::ProtocolError);
  }
  if (resp.getCrc() != resp.crcOfPacket()) {
    return Result::err(Status::CrcError);
  }
  if (resp.protoVer != kProtocolVersion) {
    return Result::err(Status::ProtocolError);
  }
  if (resp.getStatus() != HsOk) {
    switch (resp.getStatus()) {
      case HsBadAuth:
        return Result::err(Status::AuthFailed);
      case HsCryptoUnsupported:
        return Result::err(Status::Unsupported);
      case HsBusy:
        return Result::err(Status::Busy);
      default:
        return Result::err(Status::ProtocolError);
    }
  }

  const CryptoMode mode = resp.getCryptoMode();
  if (!CryptoFactory::isKnown(mode)) {
    return Result::err(Status::ProtocolError);
  }
  if (!m_hasKeypair) {
    return Result::err(Status::CryptoError); // сначала createClientRequest()
  }

  uint8_t secret[kEcdhSecretLen];
  const Result r = computeSharedSecret(resp.pubKey, secret);
  if (!r.isOk()) {
    return r;
  }
  return deriveSessionKeys(secret, resp.baseIV, resp.sessionToken, resp.sessionId, mode, out);
}

Result Handshake::startAsClient(IHandshakeIo& io, const HandshakeCredentials& creds,
                                HandshakeResult& out) {
  m_clientCreds = creds;

  HandshakeRequestWire req;
  const Result cr = createClientRequest(req);
  if (!cr.isOk()) {
    return cr;
  }
  const Result wr = io.writeAll(reinterpret_cast<const uint8_t*>(&req), sizeof(req),
                                kHandshakeTimeoutMs);
  if (!wr.isOk()) {
    return wr;
  }

  HandshakeResponseWire resp;
  uint8_t buf[sizeof(HandshakeResponseWire)];
  const Result rr = io.readAll(buf, sizeof(buf), kHandshakeTimeoutMs);
  if (!rr.isOk()) {
    return rr;
  }
  memcpy(&resp, buf, sizeof(resp));
  return finishClient(resp, out);
}

} // namespace esp_common
