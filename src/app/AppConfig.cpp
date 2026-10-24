// File: src/app/AppConfig.cpp
#include "esp_common/app/AppConfig.h"

#include <string.h>

#include "esp_common/core/ILogger.h"

namespace esp_common {

namespace {

/// Скопировать строку в NUL-паддинговое поле.
void setField(uint8_t* dst, size_t cap, const char* src) {
  copyToFixed(dst, cap, src);
}

} // namespace

AppConfig makeDefaultAppConfig() {
  AppConfig cfg; // NSDMI задаёт безопасные значения по умолчанию

  setField(cfg.system.deviceName, sizeof(cfg.system.deviceName), "esp-common");
  cfg.system.deviceType = 0x0001;
  cfg.system.uartBaud = 115200;
  cfg.system.tcpPort = 7777;
  cfg.system.logLevel = static_cast<uint8_t>(LogLevel::Info);
  cfg.system.netRole = static_cast<uint8_t>(NetRole::Server);
  cfg.system.netType = static_cast<uint8_t>(NetTcp);
  setField(cfg.system.serverHost, sizeof(cfg.system.serverHost), "192.168.4.1");
  cfg.system.uartRxPin = -1;   // пины UART по умолчанию (RX=3, TX=1)
  cfg.system.uartTxPin = -1;
  cfg.system.rs485DePin = -1;  // RS485 без DE-пина (полный дуплекс)

  // WiFi-клиент: подключаемся к сети по умолчанию сразу при старте.
  cfg.wifiClient.enabled = 1;
  setField(cfg.wifiClient.ssid, sizeof(cfg.wifiClient.ssid), "kalita");
  setField(cfg.wifiClient.password, sizeof(cfg.wifiClient.password), "87477036302");
  cfg.wifiClient.connectTimeoutMs = 15000;

  // AP для первого включения: сервер протокола доступен сразу.
  cfg.wifiAp.enabled = 1;
  setField(cfg.wifiAp.ssid, sizeof(cfg.wifiAp.ssid), "EspCommon-Setup");
  setField(cfg.wifiAp.password, sizeof(cfg.wifiAp.password), "12345678");
  cfg.wifiAp.channel = 6;
  cfg.wifiAp.maxClients = 4;

  cfg.http.enabled = 0;
  cfg.http.port = 80;

  cfg.mqtt.enabled = 0;
  cfg.mqtt.port = 1883;

  cfg.tcpConsole.enabled = 0;
  cfg.tcpConsole.port = 23;

  // Крипто-политика: все режимы разрешены, учетка по умолчанию (сменить!).
  cfg.cryptoPolicy.allowedMask = kCryptoSupportAll;
  cfg.cryptoPolicy.minMode = static_cast<uint8_t>(CryptoPlain);
  setField(cfg.cryptoPolicy.userName, sizeof(cfg.cryptoPolicy.userName), "admin");
  setField(cfg.cryptoPolicy.password, sizeof(cfg.cryptoPolicy.password), "admin");

  return cfg;
}

void loadAppConfig(IConfigStorage& storage, AppConfig& cfg) {
  static const ConfigSection kSections[] = {CfgSystem,     CfgWifiClient, CfgWifiAp,
                                            CfgHttp,       CfgMqtt,       CfgTcpConsole,
                                            CfgCryptoPolicy};
  for (const ConfigSection section : kSections) {
    const size_t size = configSectionSize(section);
    if (size == 0 || !storage.exists(configSectionKey(section))) {
      continue;
    }
    uint8_t buf[sizeof(MqttConfig)]; // самая большая секция
    size_t stored = 0;
    const Result r = storage.load(configSectionKey(section), buf, sizeof(buf), stored);
    if (!r.isOk() || stored != size) {
      continue; // секция чужого layout'а — оставляем дефолт
    }
    memcpy(configSectionPtr(cfg, section), buf, size);
  }
}

} // namespace esp_common

// TODO: следующий этап — CRC блока настроек и миграция версий layout'а
// (версия хранится отдельным ключом NVS).
