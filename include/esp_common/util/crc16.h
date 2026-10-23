// File: include/esp_common/util/crc16.h
#pragma once
/// @file crc16.h
/// @brief CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, без рефлексии, без финального XOR.
///
/// Контрольное значение: crc16_ccitt("123456789") == 0x29B1.
/// Именно этот вариант зафиксирован протоколом Esp_common для handshake и фреймов.

#include <stddef.h>
#include <stdint.h>

namespace esp_common {

/// Продолжить вычисление CRC от промежуточного значения @p crc (конвейерная передача).
uint16_t crc16_ccitt_update(uint16_t crc, const uint8_t* data, size_t len);

/// CRC-16/CCITT-FALSE от буфера (init 0xFFFF).
uint16_t crc16_ccitt(const uint8_t* data, size_t len);

} // namespace esp_common
