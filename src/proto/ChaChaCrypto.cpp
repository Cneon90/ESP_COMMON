// File: src/proto/ChaChaCrypto.cpp
// Заголовок подтягивает конфиг mbedTLS (mbedtls/chacha20.h) и объявляет класс
// только при включённой фиче; определения охраняем той же проверкой.
#include "esp_common/proto/ChaChaCrypto.h"

#if defined(MBEDTLS_CHACHA20_C)

#include <string.h>

namespace esp_common {

ChaChaCrypto::~ChaChaCrypto() {
  if (m_ctxInit) {
    mbedtls_chacha20_free(&m_ctx);
  }
}

bool ChaChaCrypto::setKey(const uint8_t* key, size_t len) {
  if (key == nullptr || len != keyLength()) {
    return false;
  }
  if (!m_ctxInit) {
    mbedtls_chacha20_init(&m_ctx);
    m_ctxInit = true;
  }
  const int rc = mbedtls_chacha20_setkey(&m_ctx, key);
  m_keySet = (rc == 0);
  return m_keySet;
}

void ChaChaCrypto::setBaseIv(const uint8_t* baseIv8) { memcpy(m_baseIv, baseIv8, kBaseIvLen); }

Result ChaChaCrypto::encrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) {
  return crypt(seq, in, len, out);
}

Result ChaChaCrypto::decrypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) {
  return crypt(seq, in, len, out);
}

Result ChaChaCrypto::crypt(uint16_t seq, const uint8_t* in, size_t len, uint8_t* out) {
  if (!m_keySet) {
    return Result::err(Status::CryptoError);
  }
  if (len == 0) {
    return Result::ok();
  }
  if (in == nullptr || out == nullptr) {
    return Result::err(Status::InvalidArgument);
  }

  // Nonce = baseIV[0..3] || seq[2]BE || 0x00[6]; счётчик блоков — с нуля на каждый вызов.
  unsigned char nonce[12];
  buildChaChaIv(m_baseIv, seq, nonce);

  int rc = mbedtls_chacha20_starts(&m_ctx, nonce, 0u);
  if (rc == 0) {
    rc = mbedtls_chacha20_update(&m_ctx, len, in, out);
  }
  if (rc != 0) {
    return Result::err(Status::CryptoError, rc);
  }
  return Result::ok();
}

} // namespace esp_common

#endif // MBEDTLS_CHACHA20_C
