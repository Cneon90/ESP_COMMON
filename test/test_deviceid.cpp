// File: test/test_deviceid.cpp
/// @file test_deviceid.cpp
/// @brief Тесты хелпера DeviceId: hex-круг, сравнение, генерация.

#include <unity.h>

#include <string.h>

#include <string>

#include "esp_common/app/DeviceId.h"
#include "esp_common/proto/ProtocolDefs.h"

using namespace esp_common;

void test_deviceid_hex_roundtrip() {
  DeviceId id;
  memcpy(id.bytes, "\x01\x23\x45\x67\x89\xAB\xCD\xEF", kDeviceIdLen);
  TEST_ASSERT_EQUAL_STRING("0123456789abcdef", id.toString().c_str());

  DeviceId parsed;
  TEST_ASSERT_TRUE(DeviceId::fromString(id.toString().c_str(), parsed));
  TEST_ASSERT_TRUE(parsed == id);
  TEST_ASSERT_FALSE(parsed != id);

  // Регистр hex-символов не важен.
  DeviceId upper;
  TEST_ASSERT_TRUE(DeviceId::fromString("0123456789ABCDEF", upper));
  TEST_ASSERT_TRUE(upper == id);
}

void test_deviceid_from_string_rejects_garbage() {
  DeviceId out;
  TEST_ASSERT_FALSE(DeviceId::fromString("0123456789abcde", out));   // 15 символов
  TEST_ASSERT_FALSE(DeviceId::fromString("0123456789abcdef0", out)); // 17 символов
  TEST_ASSERT_FALSE(DeviceId::fromString("0123456789abcdeg", out));  // не hex
  TEST_ASSERT_FALSE(DeviceId::fromString("", out));                  // пусто
  TEST_ASSERT_FALSE(DeviceId::fromString(nullptr, out));             // null
  TEST_ASSERT_TRUE(DeviceId::fromString("ffffffffffffffff", out));   // валидный
  TEST_ASSERT_EQUAL_UINT8(0xFF, out.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, out.bytes[7]);
}

void test_deviceid_is_zero_and_compare() {
  DeviceId zero;
  TEST_ASSERT_TRUE(zero.isZero());
  TEST_ASSERT_TRUE(zero == DeviceId());

  DeviceId other;
  other.bytes[7] = 1;
  TEST_ASSERT_FALSE(other.isZero());
  TEST_ASSERT_TRUE(zero != other);

  DeviceId same;
  same.bytes[7] = 1;
  TEST_ASSERT_TRUE(other == same);

  // Утилиты proto-слоя согласованы с обёрткой.
  char hex[2 * kDeviceIdLen + 1];
  deviceIdToHex(other.bytes, hex);
  TEST_ASSERT_EQUAL_STRING("0000000000000001", hex);
}

void test_deviceid_ensure_generates_nonzero_once() {
  uint8_t id[kDeviceIdLen] = {0};
  ensureDeviceId(id);
  TEST_ASSERT_FALSE(deviceIdIsZero(id));

  // Повторный вызов не перегенерирует непустое значение.
  uint8_t before[kDeviceIdLen];
  memcpy(before, id, sizeof(before));
  ensureDeviceId(id);
  TEST_ASSERT_EQUAL_MEMORY(before, id, sizeof(before));

  // Загруженное из NVS значение тоже не трогается.
  uint8_t stored[kDeviceIdLen];
  memcpy(stored, "\xDE\xAD\xBE\xEF\x00\x11\x22\x33", kDeviceIdLen);
  ensureDeviceId(stored);
  TEST_ASSERT_EQUAL_MEMORY("\xDE\xAD\xBE\xEF\x00\x11\x22\x33", stored, kDeviceIdLen);
}

void test_handshake_wire_sizes_with_device_id() {
  TEST_ASSERT_EQUAL_UINT32(78, sizeof(HandshakeRequestWire));
  TEST_ASSERT_EQUAL_UINT32(65, sizeof(HandshakeResponseWire));
  TEST_ASSERT_EQUAL_UINT32(68, offsetof(HandshakeRequestWire, targetDeviceId));
  TEST_ASSERT_EQUAL_UINT32(55, offsetof(HandshakeResponseWire, deviceId));
}
