// File: include/esp_common/proto/PlainCrypto.h
#pragma once
/// @file PlainCrypto.h
/// @brief Режим без шифрования (CryptoPlain): тождественное преобразование.

#include "esp_common/proto/ICrypto.h"

namespace esp_common {

/// Пустое шифрование: ключ не нужен, encrypt/decrypt копируют данные как есть.
class PlainCrypto final : public ICrypto {
 public:
  PlainCrypto() = default;

  CryptoMode mode() const override { return CryptoPlain; }
  size_t keyLength() const override { return 0; }

  bool setKey(const uint8_t* key, size_t len) override;
  void setBaseIv(const uint8_t* baseIv8) override;

  Result encrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) override;
  Result decrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) override;
};

} // namespace esp_common
