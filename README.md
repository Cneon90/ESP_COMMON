# Esp_common

Каркас прошивки ESP32 (Arduino-ESP32 + PlatformIO, C++17, только mbedTLS):
бинарный протокол (handshake + фреймы + крипто), сменный транспорт и
обработка команд. Реализована команда `PING` (cmd=0x0000, эхо данных);
все прочие cmd получают ответ `NOT_IMPLEMENTED`.

Ключевое свойство: **один набор команд работает и когда узел — сервер, и когда
он — клиент**. Логика команд не знает ни роль, ни транспорт; роль задаётся
фабрикой транспорта в `AppConfig`.

## Архитектура

```
+--------------------------------------------------------------------------+
|                                 main.cpp                                 |
|                          setup()/loop() + App                            |
+--------------------------------------------------------------------------+
|                                  app/                                    |
|   IApp / App            AppConfig (pack(1), дефолты; загрузка — TODO)     |
|   роль узла (Server/Client) выбирает фабрику транспорта                   |
+--------------------------------------------------------------------------+
|                                 session/                                 |
|   ISession / Session    SessionManager (+ адаптер ITransport→IHandshakeIo)|
|   шифрование тела, replay (seq), маршрутизация команд — БЕЗ знания роли   |
+------------------------------------+-------------------------------------+
|                        commands/   |               proto/                 |
|   ICommand / ICommandDispatcher    |  ProtocolDefs (wire, pack(1))        |
|   CommandDispatcher / Registry     |  Frame + IFrameCodec / FrameCodec    |
|   PingCommand (0x0000)             |  IHandshake / Handshake (X25519+HKDF)|
|   NotImplementedCommand (unknown)  |  ICrypto: Plain/AES-CTR/ChaCha20     |
+------------------------------------+-------------------------------------+
|                                transport/                                 |
|   ITransport  →  SerialTransport | TcpServerTransport | TcpClientTransport|
+--------------------------------------------------------------------------+
|                      core/ (Result, ILogger, ITask)  +  util/ (crc16)     |
+--------------------------------------------------------------------------+
```

Зависимости направлены вниз; вверх — только через интерфейсы (`ICommand`,
`ICommandDispatcher`, `IFrameCodec`, `ICrypto`, `IHandshake`, `ITransport`,
`ISession`, `ILogger`). Новая команда добавляется без правок диспетчера и
сессии (OCP). Глобальное состояние — только точка входа `main.cpp`.

## Дерево файлов

```
include/esp_common/
  core/      ITask.h ILogger.h Result.h
  proto/     ProtocolDefs.h Frame.h IFrameCodec.h FrameCodec.h
             IHandshake.h Handshake.h ICrypto.h PlainCrypto.h
             AesCtrCrypto.h ChaChaCrypto.h CryptoFactory.h
  transport/ ITransport.h SerialTransport.h TcpServerTransport.h TcpClientTransport.h
  commands/  ICommand.h CommandDispatcher.h CommandRegistry.h PingCommand.h NotImplementedCommand.h
  session/   ISession.h Session.h SessionManager.h
  app/       IApp.h App.h AppConfig.h
  util/      crc16.h
src/         (.cpp зеркально include/)
test/        test_frame.cpp test_handshake.cpp test_ping.cpp
main.cpp  platformio.ini  README.md
```

## Протокол

Все многобайтовые поля — **big-endian**, все wire-структуры — `#pragma pack(1)`
с `static_assert` размера. CRC-16 — **CRC-16/CCITT-FALSE**
(poly 0x1021, init 0xFFFF, без рефлексии, без финального XOR;
`"123456789" → 0x29B1`).

### Handshake запрос — 76 байт (клиент → сервер)

| Смещение | Размер | Поле          | Описание                                   |
|---------:|-------:|---------------|--------------------------------------------|
| 0        | 1      | magic         | `0xA5`                                     |
| 1        | 1      | protoVer      | `0x01`                                     |
| 2        | 2      | deviceType    | BE                                         |
| 4        | 1      | netType       | 0=Serial, 1=TCP                            |
| 5        | 1      | cryptoSupport | битовая маска `CryptoMode`                 |
| 6        | 10     | userName      | NUL-паддинг                                |
| 16       | 16     | password      | NUL-паддинг                                |
| 32       | 32     | pubKey        | X25519 (LE-кодирование X, как в mbedTLS)   |
| 64       | 4      | curTime       | BE, unix time                              |
| 68       | 6      | reserved      | `0x00`                                     |
| 74       | 2      | crc16         | CRC по байтам 0..73                        |

### Handshake ответ — 58 байт (сервер → клиент)

| Смещение | Размер | Поле         | Описание                                |
|---------:|-------:|--------------|-----------------------------------------|
| 0        | 1      | magic        | `0x5A`                                  |
| 1        | 1      | protoVer     | `0x01`                                  |
| 2        | 1      | status       | 0=ok, 1=badAuth, 2=badVersion, 3=crypto, 4=busy, 5=internal |
| 3        | 1      | sessionId    | 1..255                                  |
| 4        | 1      | cryptoMode   | выбранный `CryptoMode`                  |
| 5        | 4      | sessionToken | случайный                               |
| 9        | 32     | pubKey       | X25519 сервера                          |
| 41       | 8      | baseIV       | базовый IV сессии                       |
| 49       | 2      | maxFrameSize | BE                                      |
| 51       | 4      | deviceTime   | BE, unix time сервера                   |
| 55       | 1      | reserved     | `0x00`                                  |
| 56       | 2      | crc16        | CRC по байтам 0..55                     |

### Фрейм — заголовок 13 байт + тело (0..1024)

| Смещение | Размер | Поле    | Описание                                        |
|---------:|-------:|---------|-------------------------------------------------|
| 0        | 1      | magic   | `0xC5`                                          |
| 1        | 1      | flags   | бит0 = response, бит1 = error                   |
| 2        | 1      | crypto  | `CryptoMode` тела                               |
| 3        | 1      | reserved| `0x00`                                          |
| 4        | 2      | seq     | BE, монотонный в сессии (replay-защита)         |
| 6        | 2      | cmd     | BE, `0x0000` = PING                             |
| 8        | 1      | param   | параметр команды                                |
| 9        | 2      | length  | BE, длина тела                                  |
| 11       | 2      | crc16   | CRC по заголовку без crc16 (11 байт) + **телу** |

Ответ на команду: `flags.response=1`, `seq` совпадает с запросом.
Ответ с ошибкой: дополнительно `flags.error=1`, тело = `[0]` = код `Status`,
дальше — данные команды (у NOT_IMPLEMENTED — BE-код запрошенной cmd).

### Крипто

| Режим | Код | Ключ | IV / nonce фрейма                        |
|-------|----:|------|------------------------------------------|
| plain       | 0 | —      | —                                        |
| AES-128-CTR | 1 | 16 байт| `baseIV[8] \|\| seq[2]BE \|\| 0x00[6]`   |
| AES-256-CTR | 2 | 32 байта| `baseIV[8] \|\| seq[2]BE \|\| 0x00[6]`  |
| ChaCha20    | 3 | 32 байта| `baseIV[0..3] \|\| seq[2]BE \|\| 0x00[6]` |

Обмен ключами (режимы 1..3): ECDH **X25519** (mbedTLS ECP), затем
**HKDF-SHA256** (RFC 5869): `ikm = ECDH-секрет`,
`salt = baseIV[8] || sessionToken[4] || sessionId[1]`, `info = "EspCommon.v1"`,
`L = keyLength(режим)`. Ключ по сети **не передаётся**.

Наличие режима в конкретной сборке учитывается автоматически
(`CryptoFactory::supportedModesMask()`): если предсобранный mbedTLS платформы
собран без `MBEDTLS_CHACHA20_C` (как в arduino-esp32 2.x), ChaCha20 исключается
из согласования, и стороны выбирают следующий по силе режим (AES-256-CTR);
на нативной среде (полный mbedTLS 3.6) ChaCha20 доступен и покрыт тестами.

Replay-защита: `seq` принятых запросов строго монотонно растёт в сессии
(учёт переполнения uint16); нарушение или плохой CRC → ошибка
(повтор seq — ответ `flags.error` с кодом `ReplayDetected`; мусор — сброс потока).

CRC фрейма считается по передаваемому телу (шифротекст) — целостность канала
проверяется до расшифровки. Для криптостойкой аутентификации кадров
(AEAD/Poly1305) — TODO следующего этапа.

## Как добавить команду

1. Создать класс с интерфейсом `ICommand` (см. `PingCommand`):

```cpp
// include/esp_common/commands/GetInfoCommand.h
#include "esp_common/commands/ICommand.h"

class GetInfoCommand final : public esp_common::ICommand {
 public:
  uint16_t cmdId() const override { return 0x0001; }            // код на проводе
  esp_common::Result execute(const esp_common::CommandRequest& req,
                             esp_common::CommandResponse& resp) override {
    resp.status = esp_common::Status::Ok;
    resp.data  = {'E', 'C', '1'};                               // своё тело
    return esp_common::Result::ok();
  }
};
```

2. Зарегистрировать в `src/app/App.cpp` (`App::init()`):

```cpp
m_registry.add(std::unique_ptr<ICommand>(new GetInfoCommand()));
```

Больше ничего: `Session`, `CommandDispatcher`, транспорт и роль не меняются.
Неизвестные команды автоматически получают ответ `NOT_IMPLEMENTED`.

## Роли

Роль задаётся в `AppConfig.SystemConfig`:

| netType | netRole | Каналы                             | Сценарий                                          |
|--------:|--------:|------------------------------------|---------------------------------------------------|
| 0       | 0       | UART/RS485 (`SerialTransport`)     | только кабель                                     |
| 1       | 0       | TCP `TcpServerTransport`           | слушаем `tcpPort`, accept → handshake → сессия    |
| 1       | 1       | TCP `TcpClientTransport`           | подключаемся к `serverHost:tcpPort` → handshake   |
| 2       | *       | **TCP (по роли) + UART одновременно** | сервисный доступ/настройка через RS485 в любой момент |

Поле `SystemConfig.netType` (`NetType`: 0=Serial, 1=TCP, 2=**NetDual**).
В NetDual оба канала обслуживаются одним `SessionManager` и одним набором
команд; UART-handshake выполняется неблокирующе — только когда пир начал
передачу (`ITransport::hasInput()`).

Для RS485 в `SystemConfig` задаются пины: `uartRxPin`, `uartTxPin`
(-1 — пины UART0 по умолчанию) и `rs485DePin` — GPIO драйвера DE/RE
(-1 — полудуплексное управление выключено). DE поднимается на время передачи
и опускается после `flush()` (все байты физически ушли).

Клиент обрабатывает **те же команды**: `Session::pollOnce()` обслуживает
входящие запросы, `Session::sendRequest()` отправляет свои — на одном объекте.

## Изменение настроек по каналу (UART/RS485 или TCP)

Команды настроек работают на ЛЮБОЙ сессии (один набор команд для всех
каналов). Секция = одна pack(1)-структура `AppConfig` целиком.

| cmd      | Имя        | Тело запроса                                | Тело ответа              |
|---------:|------------|---------------------------------------------|--------------------------|
| `0x0001` | SET_CONFIG | `[0]` = ConfigSection, `[1..]` = байты секции | `[0]` = ConfigSection    |
| `0x0002` | GET_CONFIG | `[0]` = ConfigSection                       | байты секции             |
| `0x0003` | REBOOT     | `[0..1]` = задержка BE16, мс (необязательно, по умолчанию 500, минимум 100) | эхо задержки BE16 |

Секции (`ConfigSection`): 1=System, 2=WifiClient, 3=WifiAp, 4=Http, 5=Mqtt,
6=TcpConsole, 7=CryptoPolicy. Размер секции строго равен `sizeof` структуры —
иначе `Status::InvalidArgument`.

Механика: SET_CONFIG применяет значение в памяти **сразу** и сохраняет секцию
в NVS (`Preferences`, пространство `esp-common`); при старте
`loadAppConfig()` читает сохранённые секции поверх дефолтов. Настройки,
требующие перезапуска (WiFi, роль, порты, скорость UART), применяются после
REBOOT: сначала уходит ответ на команду, затем устройство перезагружается.

Типовой сценарий «подключился по RS485 → поменял настройки»:

```
handshake → SET_CONFIG(section=CfgWifiClient, wifi-ssid/wifi-pass)
          → SET_CONFIG(section=CfgSystem, netType=1)   // отключить UART-режим, если нужно
          → REBOOT(500)
```

## Сборка и тесты

```bash
# прошивка ESP32
pio run -e esp32dev
pio run -e esp32dev -t upload && pio device monitor

# юнит-тесты без железа (Unity, env:native)
pio test -e native
```

`env:native` собирает `src/` + `test/` с mbedTLS из реестра PlatformIO
(`kochcodes/mbedtls@3.6.2`), без Arduino. Особенности:

- точка входа тестов — `test/test_main.cpp` (PIO 6.x не генерирует runner для
  Unity автоматически; новые тесты добавляются в него через `RUN_TEST`);
- на Windows нужен gcc (MinGW-w64) в PATH и `-lbcrypt` (источник энтропии
  mbedTLS) — уже прописан в `platformio.ini`;
- `UNITY_END()` возвращает число проваленных тестов как код выхода.

Тесты покрывают: FrameCodec (roundtrip/CRC/фрагментация), CRC16 (вектор
`0x29B1`), HKDF (RFC 5869), AES-CTR (NIST SP 800-38A), ChaCha20 (RFC 7539),
симметрию ECDH X25519, согласование режимов шифрования, PING и
SET/GET_CONFIG/REBOOT через диспетчер, сквозной круг Session (включая replay).
Итог: **47 тестов — PASS**; прошивка `esp32dev` собирается (Flash ~60%).

## Статус и следующий этап

Реализовано: протокол, кодек, handshake с ключом сессии, три режима шифрования,
Serial (RS485)/TCP-транспорты, одновременная работа каналов (NetDual),
диспетчер команд, PING, чтение/запись настроек (SET/GET_CONFIG + NVS),
отложенный REBOOT, Session/SessionManager, App с ролью из конфига, Unity-тесты.

`// TODO: следующий этап`:
- CRC блока настроек и миграция версий layout'а AppConfig;
- валидация содержимого секций (диапазоны netRole/netType, пины, скорость);
- шифрование MQTT/HTTP/TCP-Console — каркасные конфиги уже объявлены;
- AEAD (ChaCha20-Poly1305) для аутентифицированных кадров;
- доверенные clientCert/PSK-режимы handshake (PSK уже в `CryptoPolicyConfig`).
