// File: include/esp_common/proto/AesCtrCrypto.h
#pragma once
/// @file AesCtrCrypto.h
/// @brief AES-CTR (mbedTLS): CryptoAes128Ctr / CryptoAes256Ctr.

// Доступ к полям контекстов mbedTLS 3.x (mbedtls_aes_context и т.п.).
#if !defined(MBEDTLS_ALLOW_PRIVATE_ACCESS)
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#endif

#include <mbedtls/aes.h>

#include "esp_common/proto/ICrypto.h"

namespace esp_common {

/// AES-счётчиковый режим. IV фрейма = baseIV[8] || seq[2]BE || 0x00[6].
/// Один экземпляр обслуживает одну сессию (ключ/IV фиксированы, seq — параметр вызова).
class AesCtrCrypto final : public ICrypto {
 public:
  /// @param keyBits 128 (CryptoAes128Ctr) или 256 (CryptoAes256Ctr).
  explicit AesCtrCrypto(uint16_t keyBits);
  ~AesCtrCrypto() override;

  AesCtrCrypto(const AesCtrCrypto&) = delete;
  AesCtrCrypto& operator=(const AesCtrCrypto&) = delete;

  CryptoMode mode() const override { return m_mode; }
  size_t keyLength() const override { return m_keyBits / 8u; }

  bool setKey(const uint8_t* key, size_t len) override;
  void setBaseIv(const uint8_t* baseIv8) override;

  Result encrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) override;
  Result decrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) override;

 private:
  /// Общая реализация encrypt/decrypt (в CTR они идентичны).
  Result crypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out);

  uint16_t m_keyBits;
  CryptoMode m_mode;
  mbedtls_aes_context m_ctx;
  uint8_t m_baseIv[kBaseIvLen] = {0};
  bool m_keySet = false;
};

} // namespace esp_common
