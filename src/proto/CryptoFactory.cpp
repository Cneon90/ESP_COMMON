// File: src/proto/CryptoFactory.cpp
#include "esp_common/proto/CryptoFactory.h"

#include "esp_common/proto/AesCtrCrypto.h"
#include "esp_common/proto/ChaChaCrypto.h"
#include "esp_common/proto/PlainCrypto.h"

namespace esp_common {

bool CryptoFactory::isKnown(CryptoMode mode) {
  return static_cast<uint8_t>(mode) < kCryptoModeCount;
}

size_t CryptoFactory::keyLengthFor(CryptoMode mode) {
  switch (mode) {
    case CryptoPlain:
      return 0;
    case CryptoAes128Ctr:
      return 16;
    case CryptoAes256Ctr:
    case CryptoChaCha20:
      return 32;
    default:
      return 0;
  }
}

uint8_t CryptoFactory::supportedModesMask() {
  // AesCtrCrypto.h тянет <mbedtls/aes.h> → конфиг mbedTLS виден здесь.
#if defined(MBEDTLS_CHACHA20_C)
  return kCryptoSupportAll;
#else
  return static_cast<uint8_t>(kCryptoSupportAll & ~(1u << static_cast<uint8_t>(CryptoChaCha20)));
#endif
}

std::unique_ptr<ICrypto> CryptoFactory::create(CryptoMode mode) {
  switch (mode) {
    case CryptoPlain:
      return std::unique_ptr<ICrypto>(new PlainCrypto());
    case CryptoAes128Ctr:
      return std::unique_ptr<ICrypto>(new AesCtrCrypto(128));
    case CryptoAes256Ctr:
      return std::unique_ptr<ICrypto>(new AesCtrCrypto(256));
    case CryptoChaCha20:
#if defined(MBEDTLS_CHACHA20_C)
      return std::unique_ptr<ICrypto>(new ChaChaCrypto());
#else
      return nullptr; // сборка mbedTLS без ChaCha20 — режим недоступен
#endif
    default:
      return nullptr;
  }
}

} // namespace esp_common
