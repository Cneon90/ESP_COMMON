// File: include/esp_common/app/AppConfig.h
#pragma once
/// @file AppConfig.h
/// @brief Бинарные структуры настроек (pack(1)) + значения по умолчанию.
///
/// Настройки читаются из NVS при старте (см. loadAppConfig) и изменяются
/// командами SET_CONFIG/GET_CONFIG (по любому каналу: UART/RS485 или TCP).
/// Все многобайтовые поля — хост-порядка: сериализация — сырые pack(1) байты.

#include <stddef.h>
#include <stdint.h>

#include "esp_common/app/IConfigStorage.h"
#include "esp_common/proto/IHandshake.h"

namespace esp_common {

/// Роль узла в сети (AppConfig.SystemConfig.netRole).
enum class NetRole : uint8_t {
  Server = 0, ///< К нам подключаются.
  Client = 1, ///< Мы подключаемся к чужому серверу.
};

#pragma pack(push, 1)

/// Параметры WiFi-клиента (STA): 1 + 33 + 64 + 4 = 102 байта.
struct WifiClientConfig {
  uint8_t enabled = 0;
  uint8_t ssid[33] = {0};
  uint8_t password[64] = {0};
  uint32_t connectTimeoutMs = 15000;
};
static_assert(sizeof(WifiClientConfig) == 102, "WifiClientConfig must be 102 bytes");

/// Параметры WiFi-точки доступа (AP): 1 + 33 + 64 + 1 + 1 = 100 байт.
struct WifiApConfig {
  uint8_t enabled = 0;
  uint8_t ssid[33] = {0};
  uint8_t password[64] = {0};
  uint8_t channel = 6;
  uint8_t maxClients = 4;
};
static_assert(sizeof(WifiApConfig) == 100, "WifiApConfig must be 100 bytes");

/// Параметры HTTP-сервера (зарезервировано): 1 + 2 = 3 байта.
struct HttpConfig {
  uint8_t enabled = 0;
  uint16_t port = 80;
};
static_assert(sizeof(HttpConfig) == 3, "HttpConfig must be 3 bytes");

/// Параметры MQTT (зарезервировано): 1 + 64 + 2 + 32 + 32 + 64 = 195 байт.
struct MqttConfig {
  uint8_t enabled = 0;
  uint8_t host[64] = {0};
  uint16_t port = 1883;
  uint8_t user[32] = {0};
  uint8_t password[32] = {0};
  uint8_t topicPrefix[64] = {0};
};
static_assert(sizeof(MqttConfig) == 195, "MqttConfig must be 195 bytes");

/// Параметры TCP-консоли (зарезервировано): 1 + 2 = 3 байта.
struct TcpConsoleConfig {
  uint8_t enabled = 0;
  uint16_t port = 23;
};
static_assert(sizeof(TcpConsoleConfig) == 3, "TcpConsoleConfig must be 3 bytes");

/// Системные параметры: 16 + 64 + 2 + 4 + 2 + 1 + 1 + 1 + 1 + 1 + 1 = 94 байта.
struct SystemConfig {
  uint8_t deviceName[16] = {0};  ///< Имя устройства (NUL-паддинг).
  uint8_t serverHost[64] = {0};  ///< Адрес сервера для роли Client (host[:ip]).
  uint16_t deviceType = 0x0001;  ///< Тип устройства (дублируется в handshake).
  uint32_t uartBaud = 115200;    ///< Скорость UART для NetSerial/NetDual.
  uint16_t tcpPort = 7777;       ///< Порт бинарного протокола (TCP).
  uint8_t logLevel = 3;          ///< LogLevel (None..Debug).
  uint8_t netRole = 0;           ///< NetRole.
  uint8_t netType = 1;           ///< NetType (0=Serial, 1=TCP, 2=Dual).
  int8_t uartRxPin = -1;         ///< GPIO RX UART (-1 = пины по умолчанию).
  int8_t uartTxPin = -1;         ///< GPIO TX UART (-1 = пины по умолчанию).
  int8_t rs485DePin = -1;        ///< GPIO DE/RE RS485 (-1 = не используется).
};
static_assert(sizeof(SystemConfig) == 94, "SystemConfig must be 94 bytes");

/// Крипто-политика и учётные данные handshake: 1 + 1 + 32 + 10 + 16 = 60 байт.
struct CryptoPolicyConfig {
  uint8_t allowedMask = 0x0F;    ///< Разрешённые режимы (бит i = CryptoMode i).
  uint8_t minMode = 0;           ///< Минимально допустимый режим (зарезервировано).
  uint8_t psk[32] = {0};         ///< PSK для будущих режимов (не используется).
  uint8_t userName[10] = {0};    ///< Учетка handshake.
  uint8_t password[16] = {0};    ///< Пароль handshake.
};
static_assert(sizeof(CryptoPolicyConfig) == 60, "CryptoPolicyConfig must be 60 bytes");

/// Корневой блок настроек: 94 + 102 + 100 + 3 + 195 + 3 + 60 = 557 байт.
struct AppConfig {
  SystemConfig system;
  WifiClientConfig wifiClient;
  WifiApConfig wifiAp;
  HttpConfig http;
  MqttConfig mqtt;
  TcpConsoleConfig tcpConsole;
  CryptoPolicyConfig cryptoPolicy;
};
static_assert(sizeof(AppConfig) == 557, "AppConfig must be 557 bytes");

#pragma pack(pop)

/// Секции настроек для команд GET_CONFIG/SET_CONFIG (байт 0 тела запроса).
enum ConfigSection : uint8_t {
  CfgNone = 0,
  CfgSystem = 1,        ///< SystemConfig.
  CfgWifiClient = 2,    ///< WifiClientConfig.
  CfgWifiAp = 3,        ///< WifiApConfig.
  CfgHttp = 4,          ///< HttpConfig.
  CfgMqtt = 5,          ///< MqttConfig.
  CfgTcpConsole = 6,    ///< TcpConsoleConfig.
  CfgCryptoPolicy = 7,  ///< CryptoPolicyConfig.
};

/// Размер секции в байтах (0 — неизвестная секция).
inline size_t configSectionSize(ConfigSection s) {
  switch (s) {
    case CfgSystem:       return sizeof(SystemConfig);
    case CfgWifiClient:   return sizeof(WifiClientConfig);
    case CfgWifiAp:       return sizeof(WifiApConfig);
    case CfgHttp:         return sizeof(HttpConfig);
    case CfgMqtt:         return sizeof(MqttConfig);
    case CfgTcpConsole:   return sizeof(TcpConsoleConfig);
    case CfgCryptoPolicy: return sizeof(CryptoPolicyConfig);
    default:              return 0;
  }
}

/// Указатель на секцию внутри конфига (nullptr — неизвестная секция).
inline void* configSectionPtr(AppConfig& c, ConfigSection s) {
  switch (s) {
    case CfgSystem:       return &c.system;
    case CfgWifiClient:   return &c.wifiClient;
    case CfgWifiAp:       return &c.wifiAp;
    case CfgHttp:         return &c.http;
    case CfgMqtt:         return &c.mqtt;
    case CfgTcpConsole:   return &c.tcpConsole;
    case CfgCryptoPolicy: return &c.cryptoPolicy;
    default:              return nullptr;
  }
}
inline const void* configSectionPtr(const AppConfig& c, ConfigSection s) {
  return configSectionPtr(const_cast<AppConfig&>(c), s);
}

/// Ключ секции в хранилище (NVS).
inline const char* configSectionKey(ConfigSection s) {
  switch (s) {
    case CfgSystem:       return "sys";
    case CfgWifiClient:   return "wcli";
    case CfgWifiAp:       return "wap";
    case CfgHttp:         return "http";
    case CfgMqtt:         return "mqtt";
    case CfgTcpConsole:   return "tcpc";
    case CfgCryptoPolicy: return "crypto";
    default:              return "";
  }
}

/// Конфигурация по умолчанию (роль: TCP-сервер, режимы шифрования — все).
AppConfig makeDefaultAppConfig();

/// @brief Загрузить сохранённые секции из хранилища поверх дефолтов.
/// @note Секции с несовпадающим размером (смена layout) молча пропускаются.
void loadAppConfig(IConfigStorage& storage, AppConfig& cfg);

/// Собрать учётные данные handshake из политики крипто + типа устройства.
inline HandshakeCredentials makeHandshakeCredentials(const CryptoPolicyConfig& policy,
                                                     uint16_t deviceType) {
  HandshakeCredentials creds;
  char buf[17];
  copyFromFixed(buf, sizeof(buf), policy.userName, sizeof(policy.userName));
  creds.userName = buf;
  copyFromFixed(buf, sizeof(buf), policy.password, sizeof(policy.password));
  creds.password = buf;
  creds.allowedCryptoMask = policy.allowedMask;
  creds.deviceType = deviceType;
  creds.netType = NetTcp;
  return creds;
}

} // namespace esp_common
