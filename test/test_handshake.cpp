// File: test/test_handshake.cpp
/// @file test_handshake.cpp
/// @brief Тесты handshake и крипто: HKDF (RFC 5869), AES-CTR (NIST SP 800-38A),
///        ChaCha20 (RFC 7539), симметрия ECDH X25519, согласование режимов.

#include <unity.h>

#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/chacha20.h>

#include "esp_common/proto/CryptoFactory.h"
#include "esp_common/proto/Handshake.h"

using namespace esp_common;

namespace {

/// Тихий логгер для тестов.
class TestLogger final : public ILogger {
 public:
  void log(LogLevel, const char*, const char*, ...) override {}
};

TestLogger s_logger;

HandshakeCredentials serverCreds(const char* user = "admin", const char* pass = "secret",
                                 uint8_t mask = kCryptoSupportAll) {
  HandshakeCredentials c;
  c.userName = user;
  c.password = pass;
  c.allowedCryptoMask = mask;
  c.deviceType = 0x0102;
  return c;
}

HandshakeCredentials clientCreds(const char* user = "admin", const char* pass = "secret",
                                 uint8_t mask = kCryptoSupportAll) {
  return serverCreds(user, pass, mask);
}

/// IHandshakeIo-заглушка: клиентский обмен с «сервером-функцией» без I/O и потоков.
class ScriptedServerIo final : public IHandshakeIo {
 public:
  explicit ScriptedServerIo(Handshake& server) : m_server(server) {}
  HandshakeResult serverOut;

  Result writeAll(const uint8_t* buf, size_t len, uint32_t) override {
    if (len != sizeof(HandshakeRequestWire)) {
      return Result::err(Status::InvalidArgument);
    }
    memcpy(&m_req, buf, len);
    m_haveReq = true;
    return Result::ok();
  }

  Result readAll(uint8_t* buf, size_t len, uint32_t) override {
    if (!m_haveReq || len != sizeof(HandshakeResponseWire)) {
      return Result::err(Status::InvalidArgument);
    }
    HandshakeResponseWire resp;
    const Result r = m_server.processServerRequest(m_req, resp, serverOut);
    if (!r.isOk()) {
      return r;
    }
    memcpy(buf, &resp, len);
    return Result::ok();
  }

 private:
  Handshake& m_server;
  HandshakeRequestWire m_req = {};
  bool m_haveReq = false;
};

} // namespace

// ---------------------------------------------------------------------------
// HKDF-SHA256: RFC 5869 Test Case 1
// ---------------------------------------------------------------------------

void test_hkdf_rfc5869_case1() {
  const uint8_t ikm[22] = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                           0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                           0x0b, 0x0b};
  const uint8_t salt[13] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0a, 0x0b, 0x0c};
  const uint8_t info[10] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9};
  const uint8_t expected[42] = {
      0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36,
      0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56,
      0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65};

  uint8_t okm[42] = {0};
  TEST_ASSERT_TRUE(Handshake::hkdfSha256(ikm, sizeof(ikm), salt, sizeof(salt), info,
                                         sizeof(info), okm, sizeof(okm)));
  TEST_ASSERT_EQUAL_MEMORY(expected, okm, sizeof(okm));
}

void test_hkdf_deterministic_and_salt_sensitive() {
  const uint8_t ikm[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                           17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
  const uint8_t saltA[13] = {0};
  uint8_t saltB[13];
  memcpy(saltB, saltA, sizeof(saltA));
  saltB[12] = 0x01; // другой sessionId в salt

  uint8_t out1[32] = {0};
  uint8_t out2[32] = {0};
  uint8_t out3[32] = {0};
  const uint8_t info[] = "EspCommon.v1";

  TEST_ASSERT_TRUE(Handshake::hkdfSha256(ikm, sizeof(ikm), saltA, sizeof(saltA), info,
                                         sizeof(info) - 1u, out1, sizeof(out1)));
  TEST_ASSERT_TRUE(Handshake::hkdfSha256(ikm, sizeof(ikm), saltA, sizeof(saltA), info,
                                         sizeof(info) - 1u, out2, sizeof(out2)));
  TEST_ASSERT_TRUE(Handshake::hkdfSha256(ikm, sizeof(ikm), saltB, sizeof(saltB), info,
                                         sizeof(info) - 1u, out3, sizeof(out3)));

  TEST_ASSERT_EQUAL_MEMORY(out1, out2, sizeof(out1)); // детерминизм
  TEST_ASSERT_FALSE(0 == memcmp(out1, out3, sizeof(out1))); // чувствительность к salt
}

// ---------------------------------------------------------------------------
// Симметрия ECDH и полный обмен
// ---------------------------------------------------------------------------

void test_handshake_full_exchange_symmetric() {
  Handshake server(serverCreds(), s_logger);
  Handshake client(serverCreds(), s_logger);
  client.setClientCredentials(clientCreds());

  HandshakeRequestWire req;
  TEST_ASSERT_TRUE(client.createClientRequest(req).isOk());

  HandshakeResponseWire resp;
  HandshakeResult sOut;
  TEST_ASSERT_TRUE(server.processServerRequest(req, resp, sOut).isOk());

  HandshakeResult cOut;
  TEST_ASSERT_TRUE(client.finishClient(resp, cOut).isOk());

  TEST_ASSERT_EQUAL_UINT32(sOut.keyLength, cOut.keyLength);
  TEST_ASSERT_EQUAL_MEMORY(sOut.sessionKey, cOut.sessionKey, cOut.keyLength);
  TEST_ASSERT_EQUAL_MEMORY(sOut.baseIv, cOut.baseIv, kBaseIvLen);
  TEST_ASSERT_EQUAL_MEMORY(sOut.token, cOut.token, kTokenLen);
  TEST_ASSERT_EQUAL_UINT8(sOut.sessionId, cOut.sessionId);
  TEST_ASSERT_EQUAL_UINT8(CryptoChaCha20, cOut.cryptoMode); // обе стороны поддерживают всё
  TEST_ASSERT_EQUAL_UINT32(kMaxFrameSize, cOut.maxFrameSize);
  TEST_ASSERT_EQUAL_UINT8(HsOk, resp.getStatus());
  TEST_ASSERT_TRUE(cOut.keyLength > 0);
}

void test_handshake_full_path_via_io() {
  Handshake server(serverCreds(), s_logger);
  Handshake client(serverCreds(), s_logger);

  ScriptedServerIo io(server);
  HandshakeResult cOut;
  TEST_ASSERT_TRUE(client.startAsClient(io, clientCreds(), cOut).isOk());
  TEST_ASSERT_EQUAL_MEMORY(io.serverOut.sessionKey, cOut.sessionKey, cOut.keyLength);
  TEST_ASSERT_EQUAL_MEMORY(io.serverOut.baseIv, cOut.baseIv, kBaseIvLen);
}

void test_handshake_bad_auth() {
  Handshake server(serverCreds(), s_logger);
  Handshake client(serverCreds(), s_logger);
  client.setClientCredentials(clientCreds("admin", "WRONG"));

  HandshakeRequestWire req;
  TEST_ASSERT_TRUE(client.createClientRequest(req).isOk());

  HandshakeResponseWire resp;
  HandshakeResult sOut;
  TEST_ASSERT_EQUAL_INT(Status::AuthFailed,
                        static_cast<int>(server.processServerRequest(req, resp, sOut).status));
  TEST_ASSERT_EQUAL_UINT8(HsBadAuth, resp.getStatus());
  // CRC ответа об ошибке тоже корректен (клиент его примет и прочитает статус).
  TEST_ASSERT_EQUAL_HEX16(resp.getCrc(), resp.crcOfPacket());
}

void test_handshake_bad_crc_rejected() {
  Handshake server(serverCreds(), s_logger);

  HandshakeRequestWire req;
  req.init();
  req.setUser("admin");
  req.setPassword("secret");
  req.setCrc(req.crcOfPacket() ^ 0x01); // ломаем CRC

  HandshakeResponseWire resp;
  HandshakeResult sOut;
  TEST_ASSERT_EQUAL_INT(Status::CrcError,
                        static_cast<int>(server.processServerRequest(req, resp, sOut).status));
}

// ---------------------------------------------------------------------------
// Согласование режима шифрования
// ---------------------------------------------------------------------------

void test_handshake_negotiation_picks_strongest_common() {
  Handshake server(serverCreds("admin", "secret", 0x03), s_logger); // plain + AES-128
  Handshake client(serverCreds(), s_logger);
  client.setClientCredentials(clientCreds("admin", "secret", kCryptoSupportAll));

  HandshakeRequestWire req;
  TEST_ASSERT_TRUE(client.createClientRequest(req).isOk());
  HandshakeResponseWire resp;
  HandshakeResult sOut;
  TEST_ASSERT_TRUE(server.processServerRequest(req, resp, sOut).isOk());
  TEST_ASSERT_EQUAL_UINT8(CryptoAes128Ctr, sOut.cryptoMode);
  TEST_ASSERT_EQUAL_UINT32(16, sOut.keyLength);
}

void test_handshake_negotiation_plain_only() {
  Handshake server(serverCreds(), s_logger);
  Handshake client(serverCreds(), s_logger);
  client.setClientCredentials(clientCreds("admin", "secret", 0x01)); // только plain

  HandshakeRequestWire req;
  TEST_ASSERT_TRUE(client.createClientRequest(req).isOk());
  HandshakeResponseWire resp;
  HandshakeResult sOut;
  TEST_ASSERT_TRUE(server.processServerRequest(req, resp, sOut).isOk());
  TEST_ASSERT_EQUAL_UINT8(CryptoPlain, sOut.cryptoMode);
  TEST_ASSERT_EQUAL_UINT32(0, sOut.keyLength);
}

void test_handshake_no_common_mode() {
  Handshake server(serverCreds("admin", "secret", 0x01), s_logger); // сервер: только plain
  Handshake client(serverCreds(), s_logger);
  client.setClientCredentials(clientCreds("admin", "secret", 0x02)); // клиент: только AES-128

  HandshakeRequestWire req;
  TEST_ASSERT_TRUE(client.createClientRequest(req).isOk());
  HandshakeResponseWire resp;
  HandshakeResult sOut;
  TEST_ASSERT_EQUAL_INT(Status::Unsupported,
                        static_cast<int>(server.processServerRequest(req, resp, sOut).status));
  TEST_ASSERT_EQUAL_UINT8(HsCryptoUnsupported, resp.getStatus());
}

// ---------------------------------------------------------------------------
// ICrypto: фабрика, roundtrip, привязка к seq (IV)
// ---------------------------------------------------------------------------

void test_crypto_factory_modes() {
  TEST_ASSERT_NOT_NULL(CryptoFactory::create(CryptoPlain).get());
  TEST_ASSERT_NOT_NULL(CryptoFactory::create(CryptoAes128Ctr).get());
  TEST_ASSERT_NOT_NULL(CryptoFactory::create(CryptoAes256Ctr).get());
  TEST_ASSERT_NOT_NULL(CryptoFactory::create(CryptoChaCha20).get());
  TEST_ASSERT_NULL(CryptoFactory::create(static_cast<CryptoMode>(kCryptoModeCount)).get());
  TEST_ASSERT_EQUAL_UINT32(0, CryptoFactory::keyLengthFor(CryptoPlain));
  TEST_ASSERT_EQUAL_UINT32(16, CryptoFactory::keyLengthFor(CryptoAes128Ctr));
  TEST_ASSERT_EQUAL_UINT32(32, CryptoFactory::keyLengthFor(CryptoAes256Ctr));
  TEST_ASSERT_EQUAL_UINT32(32, CryptoFactory::keyLengthFor(CryptoChaCha20));
}

void test_crypto_iv_layout() {
  const uint8_t baseIv[kBaseIvLen] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
  uint8_t iv[16];
  buildAesIv(baseIv, 0x2233, iv);
  static const uint8_t kExpectedAes[16] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                           0x22, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_MEMORY(kExpectedAes, iv, 16);

  uint8_t nonce[12];
  buildChaChaIv(baseIv, 0x2233, nonce);
  static const uint8_t kExpectedChaCha[12] = {0x10, 0x11, 0x12, 0x13, 0x22, 0x33,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_MEMORY(kExpectedChaCha, nonce, 12);
}

static void cryptoRoundtrip(CryptoMode mode, size_t keyLen) {
  std::unique_ptr<ICrypto> crypto = CryptoFactory::create(mode);
  TEST_ASSERT_NOT_NULL(crypto.get());
  TEST_ASSERT_EQUAL_UINT32(keyLen, crypto->keyLength());

  uint8_t key[32] = {0};
  for (size_t i = 0; i < keyLen; ++i) {
    key[i] = static_cast<uint8_t>(i + 1);
  }
  const uint8_t baseIv[kBaseIvLen] = {1, 2, 3, 4, 5, 6, 7, 8};
  TEST_ASSERT_TRUE(crypto->setKey(key, keyLen));
  crypto->setBaseIv(baseIv);

  uint8_t data[37]; // длина не кратна блоку
  for (size_t i = 0; i < sizeof(data); ++i) {
    data[i] = static_cast<uint8_t>(i * 7 + 1);
  }
  uint8_t enc[sizeof(data)];
  uint8_t dec[sizeof(data)];

  TEST_ASSERT_TRUE(crypto->encrypt(0x1234, data, sizeof(data), enc).isOk());
  TEST_ASSERT_TRUE(crypto->decrypt(0x1234, enc, sizeof(data), dec).isOk());
  TEST_ASSERT_EQUAL_MEMORY(data, dec, sizeof(data));

  // IV привязан к seq: расшифровка другим seq не совпадает с исходником.
  TEST_ASSERT_TRUE(crypto->decrypt(0x1235, enc, sizeof(data), dec).isOk());
  TEST_ASSERT_FALSE(0 == memcmp(data, dec, sizeof(data)));
}

void test_aes128_roundtrip_and_seq_binding() { cryptoRoundtrip(CryptoAes128Ctr, 16); }
void test_aes256_roundtrip_and_seq_binding() { cryptoRoundtrip(CryptoAes256Ctr, 32); }
void test_chacha20_roundtrip_and_seq_binding() { cryptoRoundtrip(CryptoChaCha20, 32); }

void test_crypto_rejects_wrong_key_length() {
  std::unique_ptr<ICrypto> aes = CryptoFactory::create(CryptoAes128Ctr);
  uint8_t key[32] = {0};
  TEST_ASSERT_FALSE(aes->setKey(key, 32)); // AES-128 требует 16
  TEST_ASSERT_FALSE(aes->setKey(key, 0));

  std::unique_ptr<ICrypto> chacha = CryptoFactory::create(CryptoChaCha20);
  TEST_ASSERT_FALSE(chacha->setKey(key, 16)); // ChaCha20 требует 32
}

// ---------------------------------------------------------------------------
// Известные векторы на API mbedTLS (паттерн вызовов как в реализациях ICrypto)
// ---------------------------------------------------------------------------

void test_mbedtls_aes128_ctr_nist_vector() {
  // NIST SP 800-38A, F.5.1: AES-128-CTR, первый блок.
  static const uint8_t kKey[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                                   0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
  static const uint8_t kIv[16] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                                  0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
  static const uint8_t kPt[16] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                                  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};
  static const uint8_t kExpected[16] = {0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
                                        0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce};

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  TEST_ASSERT_EQUAL_INT(0, mbedtls_aes_setkey_enc(&ctx, kKey, 128));

  unsigned char nonceCounter[16];
  unsigned char streamBlock[16] = {0};
  size_t ncOff = 0;
  memcpy(nonceCounter, kIv, sizeof(kIv));

  unsigned char ct[16];
  TEST_ASSERT_EQUAL_INT(0, mbedtls_aes_crypt_ctr(&ctx, sizeof(kPt), &ncOff, nonceCounter,
                                                 streamBlock, kPt, ct));
  mbedtls_aes_free(&ctx);
  TEST_ASSERT_EQUAL_MEMORY(kExpected, ct, sizeof(ct));
}

void test_mbedtls_chacha20_rfc7539_vector() {
  // RFC 7539 §2.4.2: key 0x00..0x1f, nonce 00 00 00 00 00 00 00 4a 00 00 00 00, counter 1.
  static const uint8_t kKey[32] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
  static const uint8_t kNonce[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x4a, 0x00, 0x00, 0x00, 0x00};
  static const char kPt[] =
      "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the "
      "future, sunscreen would be it.";
  static const uint8_t kExpectedFirst16[16] = {0x6e, 0x2e, 0x35, 0x9a, 0x25, 0x68, 0xf9, 0x80,
                                               0x41, 0xba, 0x07, 0x28, 0xdd, 0x0d, 0x69, 0x81};
  static const uint8_t kExpectedLast4[4] = {0x5e, 0x42, 0x87, 0x4d};

  mbedtls_chacha20_context ctx;
  mbedtls_chacha20_init(&ctx);
  TEST_ASSERT_EQUAL_INT(0, mbedtls_chacha20_setkey(&ctx, kKey));
  TEST_ASSERT_EQUAL_INT(0, mbedtls_chacha20_starts(&ctx, kNonce, 1u));

  const size_t len = sizeof(kPt) - 1u; // 114 байт
  unsigned char ct[128];
  TEST_ASSERT_TRUE(len <= sizeof(ct));
  TEST_ASSERT_EQUAL_INT(0, mbedtls_chacha20_update(&ctx, len, reinterpret_cast<const unsigned char*>(kPt), ct));
  mbedtls_chacha20_free(&ctx);

  TEST_ASSERT_EQUAL_MEMORY(kExpectedFirst16, ct, sizeof(kExpectedFirst16));
  TEST_ASSERT_EQUAL_MEMORY(kExpectedLast4, ct + len - sizeof(kExpectedLast4), sizeof(kExpectedLast4));
}
