// File: src/proto/PlainCrypto.cpp
#include "esp_common/proto/PlainCrypto.h"

#include <string.h>

namespace esp_common {

bool PlainCrypto::setKey(const uint8_t* /*key*/, size_t /*len*/) {
  return true; // ключ не требуется
}

void PlainCrypto::setBaseIv(const uint8_t* /*baseIv8*/) {}

Result PlainCrypto::encrypt(uint16_t /*seq*/, const uint8_t* in, size_t len, uint8_t* out) {
  if (len > 0 && in != out) {
    memcpy(out, in, len);
  }
  return Result::ok();
}

Result PlainCrypto::decrypt(uint16_t /*seq*/, const uint8_t* in, size_t len, uint8_t* out) {
  if (len > 0 && in != out) {
    memcpy(out, in, len);
  }
  return Result::ok();
}

} // namespace esp_common
