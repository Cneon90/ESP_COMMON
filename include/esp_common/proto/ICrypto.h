// File: include/esp_common/proto/ICrypto.h
#pragma once
/// @file ICrypto.h
/// @brief Интерфейс поточного шифрования фрейма + построение IV.

#include <stddef.h>
#include <stdint.h>

#include "esp_common/core/Result.h"
#include "esp_common/proto/ProtocolDefs.h"

namespace esp_common {

/// @brief Построить 16-байтный IV для AES-CTR: baseIV[8] || seq[2]BE || 0x00[6].
inline void buildAesIv(const uint8_t* baseIv8, uint16_t seq, uint8_t iv[16]) {
  for (size_t i = 0; i < kBaseIvLen; ++i) {
    iv[i] = baseIv8[i];
  }
  iv[8] = static_cast<uint8_t>(seq >> 8);
  iv[9] = static_cast<uint8_t>(seq & 0xFFu);
  for (size_t i = 10; i < 16; ++i) {
    iv[i] = 0;
  }
}

/// @brief Построить 12-байтный nonce для ChaCha20: baseIV[0..3] || seq[2]BE || 0x00[6].
inline void buildChaChaIv(const uint8_t* baseIv8, uint16_t seq, uint8_t iv[12]) {
  for (size_t i = 0; i < 4; ++i) {
    iv[i] = baseIv8[i];
  }
  iv[4] = static_cast<uint8_t>(seq >> 8);
  iv[5] = static_cast<uint8_t>(seq & 0xFFu);
  for (size_t i = 6; i < 12; ++i) {
    iv[i] = 0;
  }
}

/// @brief Поточное шифрование тел фреймов в рамках одной сессии.
///
/// Ключ и базовый IV задаются один раз после handshake (SessionManager);
/// IV конкретного фрейма вычисляется из baseIV и seq «внутри» реализации.
/// Шифрование и расшифрование при одном seq дают одинаковый keystream,
/// поэтому encrypt/decrypt симметричны (CTR/ChaCha20).
class ICrypto {
 public:
  virtual ~ICrypto() = default;

  /// Режим, реализуемый данным объектом.
  virtual CryptoMode mode() const = 0;
  /// Требуемая длина ключа в байтах (0 — ключ не нужен).
  virtual size_t keyLength() const = 0;

  /// Установить ключ сессии (длина должна равняться keyLength()).
  virtual bool setKey(const uint8_t* key, size_t len) = 0;
  /// Установить базовый IV сессии (8 байт, из handshake-ответа).
  virtual void setBaseIv(const uint8_t* baseIv8) = 0;

  /// @brief Зашифровать @p len байт: out = E(baseIV, seq, in).
  /// @note Допускается in == out (шифрование на месте).
  virtual Result encrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) = 0;

  /// @brief Расшифровать @p len байт: out = D(baseIV, seq, in).
  /// @note Допускается in == out (расшифровка на месте).
  virtual Result decrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) = 0;
};

} // namespace esp_common
