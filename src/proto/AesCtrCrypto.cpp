// File: src/proto/AesCtrCrypto.cpp
#include "esp_common/proto/AesCtrCrypto.h"

#include <string.h>

#include "esp_common/core/ILogger.h"

namespace esp_common {

AesCtrCrypto::AesCtrCrypto(uint16_t keyBits) : m_keyBits(keyBits) {
  m_mode = (keyBits == 128) ? CryptoAes128Ctr : CryptoAes256Ctr;
  mbedtls_aes_init(&m_ctx);
}

AesCtrCrypto::~AesCtrCrypto() { mbedtls_aes_free(&m_ctx); }

bool AesCtrCrypto::setKey(const uint8_t* key, size_t len) {
  if (key == nullptr || len != keyLength()) {
    return false;
  }
  // Для CTR расписание шифрования используется в обе стороны.
  const int rc = mbedtls_aes_setkey_enc(&m_ctx, key, m_keyBits);
  m_keySet = (rc == 0);
  return m_keySet;
}

void AesCtrCrypto::setBaseIv(const uint8_t* baseIv8) { memcpy(m_baseIv, baseIv8, kBaseIvLen); }

Result AesCtrCrypto::encrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) {
  return crypt(seq, in, len, out);
}

Result AesCtrCrypto::decrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) {
  return crypt(seq, in, len, out);
}

Result AesCtrCrypto::crypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) {
  if (!m_keySet) {
    return Result::err(Status::CryptoError);
  }
  if (len == 0) {
    return Result::ok();
  }
  if (in == nullptr || out == nullptr) {
    return Result::err(Status::InvalidArgument);
  }

  // IV = baseIV[8] || seq[2]BE || 0x00[6]; состояние счётчика — на каждый вызов.
  unsigned char nonceCounter[16];
  unsigned char streamBlock[16];
  size_t ncOff = 0;
  buildAesIv(m_baseIv, seq, nonceCounter);
  memset(streamBlock, 0, sizeof(streamBlock));

  const int rc = mbedtls_aes_crypt_ctr(&m_ctx, len, &ncOff, nonceCounter, streamBlock, in, out);
  if (rc != 0) {
    return Result::err(Status::CryptoError, rc);
  }
  return Result::ok();
}

} // namespace esp_common
