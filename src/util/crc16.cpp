// File: src/util/crc16.cpp
#include "esp_common/util/crc16.h"

namespace esp_common {

uint16_t crc16_ccitt_update(uint16_t crc, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(static_cast<uint16_t>(data[i]) << 8);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if ((crc & 0x8000u) != 0) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021u);
      } else {
        crc = static_cast<uint16_t>(crc << 1);
      }
    }
  }
  return crc;
}

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
  return crc16_ccitt_update(0xFFFFu, data, len);
}

} // namespace esp_common
