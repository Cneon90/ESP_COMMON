// File: test/test_frame.cpp
/// @file test_frame.cpp
/// @brief Тесты FrameCodec: encode/decode/CRC/фрагментация (env:native, Unity).
///
/// Runner с общими setUp/tearDown определён в test_main.cpp (PIO не генерирует
/// main() для Unity автоматически).

#include <unity.h>

#include <string.h>

#include "esp_common/proto/FrameCodec.h"
#include "esp_common/proto/ProtocolDefs.h"
#include "esp_common/util/crc16.h"

using namespace esp_common;

namespace {

static FrameCodec s_codec;

Frame makeTestFrame(uint16_t seq, const char* body, uint8_t extraFlags = 0) {
  Frame f;
  f.flags = extraFlags;
  f.cryptoMode = CryptoPlain;
  f.seq = seq;
  f.cmd = 0x0000; // PING
  f.param = 0x42;
  const size_t len = strlen(body);
  f.payload.assign(reinterpret_cast<const uint8_t*>(body),
                   reinterpret_cast<const uint8_t*>(body) + len);
  return f;
}

} // namespace

// ---------------------------------------------------------------------------
// CRC-16/CCITT-FALSE
// ---------------------------------------------------------------------------

void test_crc16_check_value_123456789() {
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16_ccitt(data, sizeof(data)));
}

void test_crc16_streaming_equals_oneshot() {
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  const uint16_t whole = crc16_ccitt(data, sizeof(data));
  const uint16_t split = crc16_ccitt_update(crc16_ccitt(data, 4), data + 4, sizeof(data) - 4);
  TEST_ASSERT_EQUAL_HEX16(whole, split);
}

// ---------------------------------------------------------------------------
// encode/decode roundtrip
// ---------------------------------------------------------------------------

void test_frame_roundtrip() {
  const Frame in = makeTestFrame(7, "hello");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());
  TEST_ASSERT_EQUAL_UINT32(kFrameHeaderSize + 5, bytes.size());

  Frame out;
  size_t consumed = 0;
  const Result r = s_codec.decode(bytes.data(), bytes.size(), out, consumed);
  TEST_ASSERT_TRUE(r.isOk());
  TEST_ASSERT_EQUAL_UINT32(bytes.size(), consumed);
  TEST_ASSERT_EQUAL_UINT16(in.seq, out.seq);
  TEST_ASSERT_EQUAL_UINT16(in.cmd, out.cmd);
  TEST_ASSERT_EQUAL_UINT8(in.param, out.param);
  TEST_ASSERT_EQUAL_UINT8(in.flags, out.flags);
  TEST_ASSERT_EQUAL_UINT8(in.cryptoMode, out.cryptoMode);
  TEST_ASSERT_EQUAL_UINT32(in.payload.size(), out.payload.size());
  TEST_ASSERT_EQUAL_MEMORY(in.payload.data(), out.payload.data(), in.payload.size());
}

void test_frame_zero_payload_roundtrip() {
  const Frame in = makeTestFrame(0xFFFF, "");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());
  TEST_ASSERT_EQUAL_UINT32(kFrameHeaderSize, bytes.size());

  Frame out;
  size_t consumed = 0;
  TEST_ASSERT_TRUE(s_codec.decode(bytes.data(), bytes.size(), out, consumed).isOk());
  TEST_ASSERT_EQUAL_UINT32(kFrameHeaderSize, consumed);
  TEST_ASSERT_TRUE(out.payload.empty());
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, out.seq);
}

void test_frame_header_layout() {
  const Frame in = makeTestFrame(0x0102, "abc");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());

  TEST_ASSERT_EQUAL_UINT8(kFrameMagic, bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0x00, bytes[1]); // flags
  TEST_ASSERT_EQUAL_UINT8(CryptoPlain, bytes[2]);
  TEST_ASSERT_EQUAL_UINT8(0x00, bytes[3]); // reserved
  TEST_ASSERT_EQUAL_UINT8(0x01, bytes[4]); // seq BE
  TEST_ASSERT_EQUAL_UINT8(0x02, bytes[5]);
  TEST_ASSERT_EQUAL_UINT8(0x00, bytes[6]); // cmd BE (0x0000 = PING)
  TEST_ASSERT_EQUAL_UINT8(0x00, bytes[7]);
  TEST_ASSERT_EQUAL_UINT8(0x42, bytes[8]); // param
  TEST_ASSERT_EQUAL_UINT8(0x00, bytes[9]); // length BE = 3
  TEST_ASSERT_EQUAL_UINT8(0x03, bytes[10]);
  // CRC в конце согласован с данными.
  const uint16_t crcCalc =
      crc16_ccitt_update(crc16_ccitt(bytes.data(), kFrameHeaderSize - 2u), bytes.data() + kFrameHeaderSize, 3u);
  TEST_ASSERT_EQUAL_HEX16(crcCalc, (static_cast<uint16_t>(bytes[11]) << 8) | bytes[12]);
}

// ---------------------------------------------------------------------------
// Ошибки декодирования
// ---------------------------------------------------------------------------

void test_frame_decode_truncated_header() {
  const std::vector<uint8_t> bytes(5, 0);
  Frame out;
  size_t consumed = 0;
  const Result r = s_codec.decode(bytes.data(), bytes.size(), out, consumed);
  TEST_ASSERT_EQUAL_INT(Status::NeedMoreData, static_cast<int>(r.status));
  TEST_ASSERT_EQUAL_UINT32(0, consumed);
}

void test_frame_decode_truncated_body() {
  const Frame in = makeTestFrame(1, "0123456789");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());

  Frame out;
  size_t consumed = 0;
  const Result r = s_codec.decode(bytes.data(), bytes.size() - 1, out, consumed);
  TEST_ASSERT_EQUAL_INT(Status::NeedMoreData, static_cast<int>(r.status));
}

void test_frame_decode_bad_magic() {
  const Frame in = makeTestFrame(1, "x");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());
  bytes[0] = 0x00;

  Frame out;
  size_t consumed = 0;
  TEST_ASSERT_EQUAL_INT(Status::ProtocolError,
                        static_cast<int>(s_codec.decode(bytes.data(), bytes.size(), out, consumed).status));
}

void test_frame_decode_crc_corruption_in_body() {
  const Frame in = makeTestFrame(1, "abcdefgh");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());
  bytes[kFrameHeaderSize] ^= 0x01; // портим первый байт тела

  Frame out;
  size_t consumed = 0;
  TEST_ASSERT_EQUAL_INT(Status::CrcError,
                        static_cast<int>(s_codec.decode(bytes.data(), bytes.size(), out, consumed).status));
}

void test_frame_decode_crc_corruption_in_crc_field() {
  const Frame in = makeTestFrame(1, "abcdefgh");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());
  bytes[11] ^= 0x80; // портим CRC

  Frame out;
  size_t consumed = 0;
  TEST_ASSERT_EQUAL_INT(Status::CrcError,
                        static_cast<int>(s_codec.decode(bytes.data(), bytes.size(), out, consumed).status));
}

void test_frame_decode_oversize_length() {
  FrameWireHeader h;
  h.init();
  h.setSeq(1);
  h.setLength(0xFFFF); // больше kMaxFrameSize
  h.setCrc(h.crcOfHeader());

  Frame out;
  size_t consumed = 0;
  TEST_ASSERT_EQUAL_INT(
      Status::ProtocolError,
      static_cast<int>(s_codec.decode(reinterpret_cast<const uint8_t*>(&h), sizeof(h), out, consumed).status));
}

void test_frame_decode_rejects_unknown_crypto_mode() {
  const Frame in = makeTestFrame(1, "x");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(in, bytes).isOk());
  bytes[2] = kCryptoModeCount; // несуществующий режим
  // Пересчитываем CRC, чтобы изолировать именно проверку режима
  // (иначе сначала отработала бы проверка целостности).
  const uint16_t crc = crc16_ccitt_update(crc16_ccitt(bytes.data(), kFrameHeaderSize - 2u),
                                          bytes.data() + kFrameHeaderSize, 1u);
  bytes[11] = static_cast<uint8_t>(crc >> 8);
  bytes[12] = static_cast<uint8_t>(crc & 0xFFu);

  Frame out;
  size_t consumed = 0;
  TEST_ASSERT_EQUAL_INT(Status::ProtocolError,
                        static_cast<int>(s_codec.decode(bytes.data(), bytes.size(), out, consumed).status));
}

// ---------------------------------------------------------------------------
// Поток: несколько фреймов подряд
// ---------------------------------------------------------------------------

void test_frame_stream_two_frames() {
  const Frame a = makeTestFrame(1, "AAA");
  const Frame b = makeTestFrame(2, "BB");
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(s_codec.encode(a, bytes).isOk());
  const size_t firstLen = bytes.size();
  std::vector<uint8_t> tmp;
  TEST_ASSERT_TRUE(s_codec.encode(b, tmp).isOk());
  bytes.insert(bytes.end(), tmp.begin(), tmp.end());

  Frame out;
  size_t consumed = 0;
  TEST_ASSERT_TRUE(s_codec.decode(bytes.data(), bytes.size(), out, consumed).isOk());
  TEST_ASSERT_EQUAL_UINT32(firstLen, consumed);
  TEST_ASSERT_EQUAL_UINT8('A', out.payload[0]);

  Frame out2;
  size_t consumed2 = 0;
  TEST_ASSERT_TRUE(s_codec.decode(bytes.data() + consumed, bytes.size() - consumed, out2, consumed2).isOk());
  TEST_ASSERT_EQUAL_UINT16(2, out2.seq);
  TEST_ASSERT_EQUAL_UINT8('B', out2.payload[0]);
}

void test_frame_encode_rejects_oversize_payload() {
  Frame in = makeTestFrame(1, "");
  in.payload.assign(kMaxFrameSize + 1, 0xAA);
  std::vector<uint8_t> bytes;
  TEST_ASSERT_EQUAL_INT(Status::InvalidArgument,
                        static_cast<int>(s_codec.encode(in, bytes).status));
}
