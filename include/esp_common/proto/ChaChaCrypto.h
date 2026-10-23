// File: include/esp_common/proto/ChaChaCrypto.h
#pragma once
/// @file ChaChaCrypto.h
/// @brief ChaCha20 (mbedTLS, RFC 8439): CryptoChaCha20.

// Доступ к полям контекстов mbedTLS 3.x (mbedtls_chacha20_context).
#if !defined(MBEDTLS_ALLOW_PRIVATE_ACCESS)
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#endif

// Include до проверки MBEDTLS_CHACHA20_C: конфиг mbedTLS подтягивается отсюда.
// В сборках без ChaCha20 заголовок безопасен (только декларации).
#include <mbedtls/chacha20.h>

// ChaCha20 есть не во всех предсобранных mbedTLS (например, arduino-esp32 2.x
// собирает ядро без MBEDTLS_CHACHA20_C) — в таких сборках класс не существует,
// а согласование режимов его исключает (см. CryptoFactory::supportedModesMask).
#if defined(MBEDTLS_CHACHA20_C)

#include "esp_common/proto/ICrypto.h"

namespace esp_common {

/// ChaCha20. Nonce фрейма = baseIV[0..3] || seq[2]BE || 0x00[6] (12 байт).
/// Счётчик блоков стартует с 0 на каждом encrypt/decrypt (nonce уникален по seq).
class ChaChaCrypto final : public ICrypto {
 public:
  ChaChaCrypto() = default;
  ~ChaChaCrypto() override;

  ChaChaCrypto(const ChaChaCrypto&) = delete;
  ChaChaCrypto& operator=(const ChaChaCrypto&) = delete;

  CryptoMode mode() const override { return CryptoChaCha20; }
  size_t keyLength() const override { return 32; }

  bool setKey(const uint8_t* key, size_t len) override;
  void setBaseIv(const uint8_t* baseIv8) override;

  Result encrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) override;
  Result decrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) override;

 private:
  Result crypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out);

  mbedtls_chacha20_context m_ctx;
  uint8_t m_baseIv[kBaseIvLen] = {0};
  bool m_keySet = false;
  bool m_ctxInit = false;
};

} // namespace esp_common

#endif // MBEDTLS_CHACHA20_C
