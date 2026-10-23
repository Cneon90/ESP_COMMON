// File: include/esp_common/proto/Handshake.h
#pragma once
/// @file Handshake.h
/// @brief Реализация handshake: X25519 (mbedTLS ECP) + HKDF-SHA256 (mbedTLS HMAC).

// Доступ к полям mbedtls_ecp_point/mbedtls_mpi (X25519: X-координата) в mbedTLS 3.x.
#if !defined(MBEDTLS_ALLOW_PRIVATE_ACCESS)
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md.h>

#include "esp_common/core/ILogger.h"
#include "esp_common/proto/IHandshake.h"

namespace esp_common {

/// Размер сырого секрета ECDH X25519.
constexpr size_t kEcdhSecretLen = 32;

/// @brief Обмен handshake и вывод ключа сессии.
///
/// Схема:
///  1. обе стороны генерируют пару X25519 (mbedtls_ecp_gen_privkey + mbedtls_ecp_mul);
///  2. pubKey обменивается в handshake-пакетах (32 байта, LE-кодирование X);
///  3. общий секрет Z = priv * peerPub (mbedtls_ecp_mul), проверка на вырожденность;
///  4. sessionKey = HKDF-SHA256(ikm = Z,
///                              salt = baseIV[8] || sessionToken[4] || sessionId[1],
///                              info = "EspCommon.v1", L = keyLength(mode));
///  5. baseIV/token/sessionId выбирает сервер и передаёт в ответе.
///
/// Сервер выбирает режим: старший общий бит маски clientSupport & serverPolicy
/// в порядке ChaCha20 → AES-256 → AES-128 → plain.
class Handshake final : public IHandshake {
 public:
  /// @param serverCreds ожидаемые учётные данные/политика (роль сервера).
  Handshake(const HandshakeCredentials& serverCreds, ILogger& logger);
  ~Handshake() override;

  Handshake(const Handshake&) = delete;
  Handshake& operator=(const Handshake&) = delete;

  // --- IHandshake ---
  Result startAsServer(IHandshakeIo& io, HandshakeResult& out) override;
  Result startAsClient(IHandshakeIo& io, const HandshakeCredentials& creds,
                       HandshakeResult& out) override;
  Result processServerRequest(const HandshakeRequestWire& req, HandshakeResponseWire& resp,
                              HandshakeResult& out) override;
  Result createClientRequest(HandshakeRequestWire& req) override;
  Result finishClient(const HandshakeResponseWire& resp, HandshakeResult& out) override;

  /// Учётные данные, которыми пользуется createClientRequest (роль клиента).
  void setClientCredentials(const HandshakeCredentials& creds) { m_clientCreds = creds; }

  /// Обновить серверные учётные данные (например, после правки конфига из NVS).
  void setServerCredentials(const HandshakeCredentials& creds) { m_serverCreds = creds; }

  /// HKDF-SHA256 (RFC 5869): extract + expand. Открыт для KAT-тестов.
  static bool hkdfSha256(const uint8_t* ikm, size_t ikmLen, const uint8_t* salt, size_t saltLen,
                         const uint8_t* info, size_t infoLen, uint8_t* okm, size_t okmLen);

 private:
  Result generateKeyPair();
  Result writePublicKey(uint8_t out[kPubKeyLen]);
  Result computeSharedSecret(const uint8_t peerPub[kPubKeyLen], uint8_t outSecret[kEcdhSecretLen]);

  /// Заполнить sessionKey/baseIV/token и т.д. из секрета ECDH.
  static Result deriveSessionKeys(const uint8_t secret[kEcdhSecretLen], const uint8_t baseIv[kBaseIvLen],
                                  const uint8_t token[kTokenLen], uint8_t sessionId,
                                  CryptoMode mode, HandshakeResult& out);

  /// Заполнить ответ ошибочным статусом (с CRC) — чтобы отправить клиенту.
  void fillErrorResponse(HandshakeResponseWire& resp, HandshakeStatus st) const;

  mbedtls_ecp_group m_group;         ///< Кривая Curve25519 (загружена в конструкторе).
  mbedtls_mpi m_priv;                ///< Приватный ключ текущей стороны.
  mbedtls_ecp_point m_pub;           ///< Публичный ключ текущей стороны.
  mbedtls_entropy_context m_entropy;
  mbedtls_ctr_drbg_context m_drbg;
  bool m_rngOk = false;
  bool m_hasKeypair = false;

  ILogger& m_logger;
  HandshakeCredentials m_serverCreds; ///< Для роли сервера.
  HandshakeCredentials m_clientCreds; ///< Для роли клиента.
  uint8_t m_nextSessionId = 1;        ///< Счётчик sessionId сервера (1..255).
};

} // namespace esp_common
