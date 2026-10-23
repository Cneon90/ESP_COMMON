// File: include/esp_common/proto/ProtocolDefs.h
#pragma once
/// @file ProtocolDefs.h
/// @brief Бинарный протокол "Esp_common": константы, wire-структуры и таблицы полей.
///
/// Правила протокола:
///  - все многобайтовые поля — big-endian (BE); доступ через get/set-аксессоры;
///  - все wire-структуры — #pragma pack(1), размер зафиксирован static_assert;
///  - CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, без рефлексии, без финального XOR);
///  - CRC считается по всем байтам пакета, КРОМЕ самого поля crc16;
///  - поля "строка" (userName/password) — NUL-паддинг до фиксированной длины.
///
/// =============================== Handshake запрос (76 байт) ===============================
/// | Смещение | Размер | Поле          | Описание                                      |
/// |----------|--------|---------------|-----------------------------------------------|
/// | 0        | 1      | magic         | 0xA5                                          |
/// | 1        | 1      | protoVer      | kProtocolVersion                              |
/// | 2        | 2      | deviceType    | BE, идентификатор типа устройства             |
/// | 4        | 1      | netType       | 0=Serial, 1=TCP (информационно)               |
/// | 5        | 1      | cryptoSupport | битовая маска CryptoMode (бит i = режим i)    |
/// | 6        | 10     | userName      | NUL-паддинг                                   |
/// | 16       | 16     | password      | NUL-паддинг                                   |
/// | 32       | 32     | pubKey        | X25519, 32 байта (LE-кодирование X, mbedtls)  |
/// | 64       | 4      | curTime       | BE, unix time отправителя                     |
/// | 68       | 6      | reserved      | 0x00                                          |
/// | 74       | 2      | crc16         | BE, CRC по байтам 0..73                       |
///
/// =============================== Handshake ответ (58 байт) ================================
/// | Смещение | Размер | Поле         | Описание                                       |
/// |----------|--------|--------------|------------------------------------------------|
/// | 0        | 1      | magic        | 0x5A                                           |
/// | 1        | 1      | protoVer     | kProtocolVersion                               |
/// | 2        | 1      | status       | HandshakeStatus                                |
/// | 3        | 1      | sessionId    | идентификатор сессии (1..255, 0 = нет)         |
/// | 4        | 1      | cryptoMode   | выбранный CryptoMode                           |
/// | 5        | 4      | sessionToken | токен сессии (случайный)                       |
/// | 9        | 32     | pubKey       | X25519 публичный ключ сервера                  |
/// | 41       | 8      | baseIV       | базовый IV сессии (случайный)                  |
/// | 49       | 2      | maxFrameSize | BE, максимум длины тела фрейма                 |
/// | 51       | 4      | deviceTime   | BE, unix time сервера                          |
/// | 55       | 1      | reserved     | 0x00                                           |
/// | 56       | 2      | crc16        | BE, CRC по байтам 0..55                        |
///
/// =============================== Заголовок фрейма (13 байт) ===============================
/// | Смещение | Размер | Поле    | Описание                                            |
/// |----------|--------|---------|-----------------------------------------------------|
/// | 0        | 1      | magic   | 0xC5                                                |
/// | 1        | 1      | flags   | бит0 kFrameFlagResponse, бит1 kFrameFlagError       |
/// | 2        | 1      | crypto  | CryptoMode (каким режимом зашифровано тело)         |
/// | 3        | 1      | reserved| 0x00                                                |
/// | 4        | 2      | seq     | BE, номер фрейма (replay-защита, монотонный)        |
/// | 6        | 2      | cmd     | BE, код команды (0x0000 = PING)                     |
/// | 8        | 1      | param   | параметр команды (на усмотрение команды)            |
/// | 9        | 2      | length  | BE, длина тела (0..kMaxFrameSize)                   |
/// | 11       | 2      | crc16   | BE, CRC по заголовку без crc16 (11 байт) + ТЕЛУ     |
///
/// CRC фрейма считается по передаваемому телу: для шифрованных режимов —
/// по шифротексту (приёмник проверяет целостность канала до расшифровки).

#include <string.h>

#include "esp_common/util/crc16.h"

namespace esp_common {

/// Версия протокола.
constexpr uint8_t kProtocolVersion = 0x01;
/// Магия запроса handshake (клиент → сервер).
constexpr uint8_t kHandshakeReqMagic = 0xA5;
/// Магия ответа handshake (сервер → клиент).
constexpr uint8_t kHandshakeRespMagic = 0x5A;
/// Магия фрейма.
constexpr uint8_t kFrameMagic = 0xC5;

// ---- Фиксированные размеры полей (байт) ----
constexpr size_t kPubKeyLen = 32;    ///< Публичный ключ X25519.
constexpr size_t kUserNameLen = 10;  ///< userName (NUL-паддинг).
constexpr size_t kPasswordLen = 16;  ///< password (NUL-паддинг).
constexpr size_t kBaseIvLen = 8;     ///< Базовый IV сессии.
constexpr size_t kTokenLen = 4;      ///< Токен сессии.
/// Максимум длины ТЕЛА фрейма: предел кодека и значение по умолчанию для сессии.
constexpr size_t kMaxFrameSize = 1024;
/// Размер заголовка фрейма (без тела).
constexpr size_t kFrameHeaderSize = 13;

/// Режимы шифрования тела фрейма (Frame.crypto, HandshakeResp.cryptoMode).
enum CryptoMode : uint8_t {
  CryptoPlain = 0,     ///< Без шифрования.
  CryptoAes128Ctr = 1, ///< AES-128-CTR.
  CryptoAes256Ctr = 2, ///< AES-256-CTR.
  CryptoChaCha20 = 3,  ///< ChaCha20 (RFC 8439).
};
constexpr uint8_t kCryptoModeCount = 4;
/// Битовая маска поддержки режимов: бит i = режим i (см. CryptoMode).
constexpr uint8_t kCryptoSupportAll = 0x0F;

/// Разрешён ли режим @p mode битовой маской @p mask.
inline bool cryptoAllowed(uint8_t mask, CryptoMode mode) {
  return (mask & (1u << static_cast<uint8_t>(mode))) != 0;
}

/// Тип сети (информационное поле handshake).
enum NetType : uint8_t {
  NetSerial = 0, ///< UART (RS485).
  NetTcp = 1,    ///< TCP (WiFi).
  NetDual = 2,   ///< TCP (по роли) + UART-сервер одновременно.
};

/// Статус handshake, передаётся в ответе (HandshakeResp.status).
enum HandshakeStatus : uint8_t {
  HsOk = 0,                 ///< Успех.
  HsBadAuth = 1,            ///< Неверные userName/password.
  HsBadVersion = 2,         ///< Неподдерживаемая версия протокола.
  HsCryptoUnsupported = 3,  ///< Нет пересечения по режимам шифрования.
  HsBusy = 4,               ///< Нет свободных сессий.
  HsInternal = 5,           ///< Внутренняя ошибка сервера.
};

/// Флаги фрейма (Frame.flags).
constexpr uint8_t kFrameFlagResponse = 0x01; ///< Фрейм — ответ на запрос.
constexpr uint8_t kFrameFlagError = 0x02;    ///< Запрос обработан с ошибкой.

// ---------------------------------------------------------------------------
// BE load/store — единственная точка преобразования порядка байт.
// ---------------------------------------------------------------------------
inline uint16_t loadBE16(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}
inline void storeBE16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v >> 8);
  p[1] = static_cast<uint8_t>(v);
}
inline uint32_t loadBE32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}
inline void storeBE32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v >> 24);
  p[1] = static_cast<uint8_t>(v >> 16);
  p[2] = static_cast<uint8_t>(v >> 8);
  p[3] = static_cast<uint8_t>(v);
}

/// Скопировать строку в NUL-паддинговое поле фиксированной длины (с усечением).
inline void copyToFixed(uint8_t* dst, size_t cap, const char* src) {
  size_t i = 0;
  for (; src != nullptr && src[i] != '\0' && i < cap; ++i) {
    dst[i] = static_cast<uint8_t>(src[i]);
  }
  for (; i < cap; ++i) {
    dst[i] = 0;
  }
}

/// Скопировать NUL-паддинговое поле в C-строку (гарантирует терминатор).
inline void copyFromFixed(char* out, size_t outCap, const uint8_t* src, size_t cap) {
  size_t i = 0;
  for (; i + 1 < outCap && i < cap && src[i] != 0; ++i) {
    out[i] = static_cast<char>(src[i]);
  }
  out[i] = '\0';
}

#pragma pack(push, 1)

/// ---------------------------------------------------------------------------
/// @brief Handshake запрос (клиент → сервер), ровно 76 байт.
/// ---------------------------------------------------------------------------
struct HandshakeRequestWire {
  uint8_t magic;               ///< 0xA5
  uint8_t protoVer;            ///< kProtocolVersion
  uint8_t deviceType[2];       ///< BE, тип устройства
  uint8_t netType;             ///< NetType
  uint8_t cryptoSupport;       ///< битовая маска CryptoMode
  uint8_t userName[kUserNameLen];
  uint8_t password[kPasswordLen];
  uint8_t pubKey[kPubKeyLen];  ///< X25519 (LE-кодирование X, как в mbedTLS)
  uint8_t curTime[4];          ///< BE, unix time отправителя
  uint8_t reserved[6];         ///< 0x00
  uint8_t crc16[2];            ///< BE, CRC по байтам 0..73

  uint16_t getDeviceType() const { return loadBE16(deviceType); }
  void setDeviceType(uint16_t v) { storeBE16(deviceType, v); }
  uint32_t getCurTime() const { return loadBE32(curTime); }
  void setCurTime(uint32_t v) { storeBE32(curTime, v); }
  uint16_t getCrc() const { return loadBE16(crc16); }
  void setCrc(uint16_t v) { storeBE16(crc16, v); }

  void setUser(const char* s) { copyToFixed(userName, sizeof(userName), s); }
  void setPassword(const char* s) { copyToFixed(password, sizeof(password), s); }
  void getUser(char out[kUserNameLen + 1]) const {
    copyFromFixed(out, kUserNameLen + 1, userName, kUserNameLen);
  }
  void getPassword(char out[kPasswordLen + 1]) const {
    copyFromFixed(out, kPasswordLen + 1, password, kPasswordLen);
  }

  /// CRC по всем байтам структуры, кроме поля crc16.
  uint16_t crcOfPacket() const {
    return crc16_ccitt(reinterpret_cast<const uint8_t*>(this), sizeof(*this) - 2u);
  }
  /// Обнулить всё, выставить magic/version (для клиента перед заполнением).
  void init() {
    const uint8_t* end = reinterpret_cast<const uint8_t*>(this) + sizeof(*this);
    for (uint8_t* p = reinterpret_cast<uint8_t*>(this) + 2; p < end; ++p) {
      *p = 0;
    }
    magic = kHandshakeReqMagic;
    protoVer = kProtocolVersion;
  }
};
static_assert(sizeof(HandshakeRequestWire) == 76, "HandshakeRequestWire must be exactly 76 bytes");

/// ---------------------------------------------------------------------------
/// @brief Handshake ответ (сервер → клиент), ровно 58 байт.
/// ---------------------------------------------------------------------------
struct HandshakeResponseWire {
  uint8_t magic;               ///< 0x5A
  uint8_t protoVer;            ///< kProtocolVersion
  uint8_t status;              ///< HandshakeStatus
  uint8_t sessionId;           ///< 1..255
  uint8_t cryptoMode;          ///< выбранный CryptoMode
  uint8_t sessionToken[kTokenLen];
  uint8_t pubKey[kPubKeyLen];  ///< X25519 публичный ключ сервера
  uint8_t baseIV[kBaseIvLen];  ///< базовый IV сессии
  uint8_t maxFrameSize[2];     ///< BE
  uint8_t deviceTime[4];       ///< BE, unix time сервера
  uint8_t reserved;            ///< 0x00
  uint8_t crc16[2];            ///< BE, CRC по байтам 0..55

  HandshakeStatus getStatus() const { return static_cast<HandshakeStatus>(status); }
  void setStatus(HandshakeStatus s) { status = static_cast<uint8_t>(s); }
  CryptoMode getCryptoMode() const { return static_cast<CryptoMode>(cryptoMode); }
  void setCryptoMode(CryptoMode m) { cryptoMode = static_cast<uint8_t>(m); }
  uint16_t getMaxFrameSize() const { return loadBE16(maxFrameSize); }
  void setMaxFrameSize(uint16_t v) { storeBE16(maxFrameSize, v); }
  uint32_t getDeviceTime() const { return loadBE32(deviceTime); }
  void setDeviceTime(uint32_t v) { storeBE32(deviceTime, v); }
  uint32_t getToken() const { return loadBE32(sessionToken); }
  void setToken(uint32_t v) { storeBE32(sessionToken, v); }
  uint16_t getCrc() const { return loadBE16(crc16); }
  void setCrc(uint16_t v) { storeBE16(crc16, v); }

  /// CRC по всем байтам структуры, кроме поля crc16.
  uint16_t crcOfPacket() const {
    return crc16_ccitt(reinterpret_cast<const uint8_t*>(this), sizeof(*this) - 2u);
  }
  /// Обнулить всё, выставить magic/version (для сервера перед заполнением).
  void init() {
    const uint8_t* end = reinterpret_cast<const uint8_t*>(this) + sizeof(*this);
    for (uint8_t* p = reinterpret_cast<uint8_t*>(this) + 2; p < end; ++p) {
      *p = 0;
    }
    magic = kHandshakeRespMagic;
    protoVer = kProtocolVersion;
  }
};
static_assert(sizeof(HandshakeResponseWire) == 58, "HandshakeResponseWire must be exactly 58 bytes");

/// ---------------------------------------------------------------------------
/// @brief Заголовок фрейма, ровно 13 байт. Тело следует сразу за заголовком.
/// ---------------------------------------------------------------------------
struct FrameWireHeader {
  uint8_t magic;      ///< 0xC5
  uint8_t flags;      ///< kFrameFlagResponse / kFrameFlagError
  uint8_t crypto;     ///< CryptoMode
  uint8_t reserved;   ///< 0x00
  uint8_t seq[2];     ///< BE
  uint8_t cmd[2];     ///< BE
  uint8_t param;      ///< параметр команды
  uint8_t length[2];  ///< BE, длина тела
  uint8_t crc16[2];   ///< BE, CRC по заголовку (11 байт) + телу

  uint8_t getFlags() const { return flags; }
  void setFlags(uint8_t v) { flags = v; }
  CryptoMode getCryptoMode() const { return static_cast<CryptoMode>(crypto); }
  void setCryptoMode(CryptoMode m) { crypto = static_cast<uint8_t>(m); }
  uint16_t getSeq() const { return loadBE16(seq); }
  void setSeq(uint16_t v) { storeBE16(seq, v); }
  uint16_t getCmd() const { return loadBE16(cmd); }
  void setCmd(uint16_t v) { storeBE16(cmd, v); }
  uint16_t getLength() const { return loadBE16(length); }
  void setLength(uint16_t v) { storeBE16(length, v); }
  uint16_t getCrc() const { return loadBE16(crc16); }
  void setCrc(uint16_t v) { storeBE16(crc16, v); }

  /// CRC по заголовку без поля crc16 (первые 11 байт).
  uint16_t crcOfHeader() const {
    return crc16_ccitt(reinterpret_cast<const uint8_t*>(this), sizeof(*this) - 2u);
  }
  /// Обнулить всё, выставить magic (для отправителя перед заполнением).
  void init() {
    const uint8_t* end = reinterpret_cast<const uint8_t*>(this) + sizeof(*this);
    for (uint8_t* p = reinterpret_cast<uint8_t*>(this) + 1; p < end; ++p) {
      *p = 0;
    }
    magic = kFrameMagic;
  }
};
static_assert(sizeof(FrameWireHeader) == 13, "FrameWireHeader must be exactly 13 bytes");

#pragma pack(pop)

} // namespace esp_common
