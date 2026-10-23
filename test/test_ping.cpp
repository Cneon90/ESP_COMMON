// File: test/test_ping.cpp
/// @file test_ping.cpp
/// @brief Тесты команд: PING через CommandDispatcher + сквозной круг через Session
///        (две сессии в памяти, plain-режим, включая replay-защиту).

#include <unity.h>

#include <algorithm>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>

#include "esp_common/app/AppConfig.h"
#include "esp_common/app/IConfigStorage.h"
#include "esp_common/commands/CommandDispatcher.h"
#include "esp_common/commands/CommandRegistry.h"
#include "esp_common/commands/GetConfigCommand.h"
#include "esp_common/commands/PingCommand.h"
#include "esp_common/commands/RebootCommand.h"
#include "esp_common/commands/SetConfigCommand.h"
#include "esp_common/proto/CryptoFactory.h"
#include "esp_common/proto/FrameCodec.h"
#include "esp_common/proto/ProtocolDefs.h"
#include "esp_common/session/Session.h"

using namespace esp_common;

namespace {

class TestLogger final : public ILogger {
 public:
  void log(LogLevel, const char*, const char*, ...) override {}
};

TestLogger s_logger;

/// Пара соединённых in-memory транспортов: write() кладёт байты в буфер пира,
/// read() читает из своего буфера. Пустой буфер + установленный pump() —
/// вызовет обработку на противоположной стороне (однопоточный «event loop»).
class LoopbackTransport final : public ITransport {
 public:
  explicit LoopbackTransport(TransportRole role) : m_role(role) {}

  void connect(LoopbackTransport* peer) { m_peer = peer; }
  void setPump(std::function<void()> pump) { m_pump = std::move(pump); }

  bool open(uint32_t) override { return true; }
  void close() override { m_peer = nullptr; }
  bool isConnected() const override { return m_peer != nullptr; }
  bool hasInput() const override { return !m_buffer.empty(); }

  Result read(uint8_t* buf, size_t maxLen, uint32_t, size_t& bytesRead) override {
    bytesRead = 0;
    // Даём противоположной стороне шанс ответить, прежде чем вернуть Timeout.
    if (m_buffer.empty() && m_pump && !m_pumping) {
      m_pumping = true;
      m_pump();
      m_pumping = false;
    }
    if (m_buffer.empty()) {
      return Result::err(Status::Timeout);
    }
    bytesRead = (m_buffer.size() < maxLen) ? m_buffer.size() : maxLen;
    std::copy(m_buffer.begin(), m_buffer.begin() + static_cast<long>(bytesRead), buf);
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<long>(bytesRead));
    return Result::ok();
  }

  Result write(const uint8_t* buf, size_t len, uint32_t) override {
    if (m_peer == nullptr) {
      return Result::err(Status::Disconnected);
    }
    m_peer->m_buffer.insert(m_peer->m_buffer.end(), buf, buf + len);
    return Result::ok();
  }

  TransportRole role() const override { return m_role; }
  TransportKind kind() const override { return TransportKind::Tcp; }

 private:
  TransportRole m_role;
  LoopbackTransport* m_peer = nullptr;
  std::function<void()> m_pump;
  bool m_pumping = false;
  std::vector<uint8_t> m_buffer;
};

struct TwoSessions {
  FrameCodec codec;
  CommandDispatcher serverDispatcher{s_logger};
  CommandDispatcher clientDispatcher{s_logger};
  LoopbackTransport serverTransport{TransportRole::Server};
  LoopbackTransport clientTransport{TransportRole::Client};
  std::unique_ptr<ICrypto> serverCrypto;
  std::unique_ptr<ICrypto> clientCrypto;
  std::unique_ptr<Session> server;
  std::unique_ptr<Session> client;

  explicit TwoSessions(uint8_t sessionId = 1) {
    serverDispatcher.registerCommand(std::unique_ptr<ICommand>(new PingCommand()));

    serverTransport.connect(&clientTransport);
    clientTransport.connect(&serverTransport);

    SessionConfig cfg;
    cfg.sessionId = sessionId;
    cfg.cryptoMode = CryptoPlain;
    cfg.maxFrameSize = static_cast<uint16_t>(kMaxFrameSize);

    serverCrypto = CryptoFactory::create(CryptoPlain);
    clientCrypto = CryptoFactory::create(CryptoPlain);

    server.reset(new Session(cfg, serverTransport, codec, std::move(serverCrypto),
                             serverDispatcher, s_logger));
    client.reset(new Session(cfg, clientTransport, codec, std::move(clientCrypto),
                             clientDispatcher, s_logger));

    // Пока клиент ждёт ответ, его транспорт «подкачивает» сервер.
    clientTransport.setPump([this]() { server->pollOnce(0); });
  }
};

} // namespace

// ---------------------------------------------------------------------------
// PingCommand напрямую
// ---------------------------------------------------------------------------

void test_ping_command_echoes_data() {
  PingCommand ping;
  TEST_ASSERT_EQUAL_UINT16(kCmdPing, ping.cmdId());

  CommandRequest req;
  req.cmd = kCmdPing;
  req.param = 0x77;
  req.data = {'h', 'e', 'l', 'l', 'o'};

  CommandResponse resp;
  TEST_ASSERT_TRUE(ping.execute(req, resp).isOk());
  TEST_ASSERT_EQUAL_UINT8(0x77, resp.param);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_UINT32(5, resp.data.size());
  TEST_ASSERT_EQUAL_MEMORY(req.data.data(), resp.data.data(), 5);
}

void test_ping_command_empty_payload() {
  PingCommand ping;
  CommandRequest req;
  req.cmd = kCmdPing;
  CommandResponse resp;
  TEST_ASSERT_TRUE(ping.execute(req, resp).isOk());
  TEST_ASSERT_TRUE(resp.data.empty());
}

// ---------------------------------------------------------------------------
// CommandDispatcher
// ---------------------------------------------------------------------------

void test_dispatcher_dispatches_known_cmd() {
  CommandDispatcher dispatcher(s_logger);
  dispatcher.registerCommand(std::unique_ptr<ICommand>(new PingCommand()));
  TEST_ASSERT_EQUAL_UINT32(1, dispatcher.count());

  CommandRequest req;
  req.cmd = kCmdPing;
  req.param = 0x11;
  req.data = {1, 2, 3};
  CommandResponse resp;
  TEST_ASSERT_TRUE(dispatcher.dispatch(req, resp).isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_MEMORY(req.data.data(), resp.data.data(), 3);
}

void test_dispatcher_unknown_cmd_not_implemented() {
  CommandDispatcher dispatcher(s_logger);

  CommandRequest req;
  req.cmd = 0x1234;
  req.param = 0x55;
  req.data = {'x'};
  CommandResponse resp;
  TEST_ASSERT_TRUE(dispatcher.dispatch(req, resp).isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::NotImplemented), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_UINT8(0x55, resp.param);
  // Тело ответа заглушки: BE-код запрошенной команды.
  TEST_ASSERT_EQUAL_UINT32(2, resp.data.size());
  TEST_ASSERT_EQUAL_UINT8(0x12, resp.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x34, resp.data[1]);
}

void test_dispatcher_re_registration_replaces() {
  CommandDispatcher dispatcher(s_logger);
  dispatcher.registerCommand(std::unique_ptr<ICommand>(new PingCommand()));
  dispatcher.registerCommand(std::unique_ptr<ICommand>(new PingCommand()));
  TEST_ASSERT_EQUAL_UINT32(1, dispatcher.count());
}

// ---------------------------------------------------------------------------
// CommandRegistry
// ---------------------------------------------------------------------------

void test_registry_installs_into_dispatcher() {
  CommandRegistry registry;
  registry.add(std::unique_ptr<ICommand>(new PingCommand()));
  TEST_ASSERT_NOT_NULL(registry.find(kCmdPing));
  TEST_ASSERT_NULL(registry.find(0x1234));

  CommandDispatcher dispatcher(s_logger);
  registry.installTo(dispatcher);
  TEST_ASSERT_EQUAL_UINT32(0, registry.size()); // владение переехало
  TEST_ASSERT_EQUAL_UINT32(1, dispatcher.count());

  CommandRequest req;
  req.cmd = kCmdPing;
  CommandResponse resp;
  TEST_ASSERT_TRUE(dispatcher.dispatch(req, resp).isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
}

// ---------------------------------------------------------------------------
// Сквозной круг: клиент sendRequest → сервер pollOnce → PING → ответ
// ---------------------------------------------------------------------------

void test_session_roundtrip_ping() {
  TwoSessions ts;
  CommandResponse resp;
  const uint8_t payload[] = {'p', 'i', 'n', 'g'};

  const Result r = ts.client->sendRequest(kCmdPing, 0x09,
                                          std::vector<uint8_t>(payload, payload + 4), resp, 100);
  TEST_ASSERT_TRUE(r.isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_UINT8(0x09, resp.param);
  TEST_ASSERT_EQUAL_UINT32(4, resp.data.size());
  TEST_ASSERT_EQUAL_MEMORY(payload, resp.data.data(), 4);
  // seq ответа совпадает с запросом (проверяем буфером: клиент принял ответ на свой seq=1).
}

void test_session_roundtrip_unknown_cmd() {
  TwoSessions ts;
  CommandResponse resp;

  const Result r = ts.client->sendRequest(0xABCD, 0x00, {}, resp, 100);
  TEST_ASSERT_TRUE(r.isOk()); // обмен состоялся
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::NotImplemented), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_UINT32(2, resp.data.size()); // эхо cmd BE
  TEST_ASSERT_EQUAL_UINT8(0xAB, resp.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0xCD, resp.data[1]);
}

void test_session_replay_rejected() {
  TwoSessions ts;

  // Обычный PING: seq=1.
  CommandResponse resp;
  TEST_ASSERT_TRUE(ts.client->sendRequest(kCmdPing, 0, {'a'}, resp, 100).isOk());

  // Повторяем тот же seq: сервер обязан ответить flags.error + ReplayDetected.
  Frame replay;
  replay.cryptoMode = CryptoPlain;
  replay.seq = 1; // уже использован
  replay.cmd = kCmdPing;
  replay.param = 0;
  replay.payload = {'a'};
  std::vector<uint8_t> bytes;
  TEST_ASSERT_TRUE(ts.codec.encode(replay, bytes).isOk());
  TEST_ASSERT_TRUE(ts.clientTransport.write(bytes.data(), bytes.size(), 10).isOk());

  const Result r = ts.server->pollOnce(10);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::ReplayDetected), static_cast<int>(r.status));

  // Ошибка дошла до клиента в виде error-ответа: [0] = Status::ReplayDetected.
  Frame errFrame;
  size_t consumed = 0;
  uint8_t rx[64];
  size_t got = 0;
  TEST_ASSERT_TRUE(ts.clientTransport.read(rx, sizeof(rx), 10, got).isOk());
  TEST_ASSERT_TRUE(got >= kFrameHeaderSize);
  TEST_ASSERT_TRUE(ts.codec.decode(rx, got, errFrame, consumed).isOk());
  TEST_ASSERT_TRUE(errFrame.isResponse());
  TEST_ASSERT_TRUE(errFrame.isError());
  TEST_ASSERT_EQUAL_UINT16(1, errFrame.seq);
  TEST_ASSERT_FALSE(errFrame.payload.empty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::ReplayDetected), errFrame.payload[0]);
}

void test_session_server_serves_same_command_set_both_roles() {
  // Демонстрация «один набор команд в обе стороны»: та же сессия-«сервер»
  // отправляет запрос сессии-«клиенту», у которой зарегистрирован тот же PING.
  TwoSessions ts;
  ts.clientDispatcher.registerCommand(std::unique_ptr<ICommand>(new PingCommand()));
  ts.serverTransport.setPump([this_ts = &ts]() { this_ts->client->pollOnce(0); });

  CommandResponse resp;
  const Result r = ts.server->sendRequest(kCmdPing, 0x02, {'z'}, resp, 100);
  TEST_ASSERT_TRUE(r.isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_UINT8('z', resp.data[0]);
}

// ---------------------------------------------------------------------------
// Команды настройки: SET_CONFIG / GET_CONFIG / REBOOT
// ---------------------------------------------------------------------------

void test_config_section_helpers() {
  TEST_ASSERT_EQUAL_UINT32(sizeof(SystemConfig), configSectionSize(CfgSystem));
  TEST_ASSERT_EQUAL_UINT32(sizeof(MqttConfig), configSectionSize(CfgMqtt));
  TEST_ASSERT_EQUAL_UINT32(0, configSectionSize(CfgNone));
  TEST_ASSERT_EQUAL_UINT32(0, configSectionSize(static_cast<ConfigSection>(0x7F)));
  TEST_ASSERT_NOT_NULL(configSectionPtr(AppConfig{}, CfgSystem));
  TEST_ASSERT_NULL(configSectionPtr(AppConfig{}, CfgNone));
  TEST_ASSERT_EQUAL_STRING("sys", configSectionKey(CfgSystem));
}

void test_config_get_set_roundtrip() {
  AppConfig cfg = makeDefaultAppConfig();
  RamConfigStorage storage;
  SetConfigCommand setCmd(cfg, storage, s_logger);
  GetConfigCommand getCmd(cfg);

  // Меняем имя устройства через SET_CONFIG: [0]=секция, [1..]=SystemConfig.
  SystemConfig patched;
  memcpy(&patched, &cfg.system, sizeof(patched));
  copyToFixed(patched.deviceName, sizeof(patched.deviceName), "factory-2");

  CommandRequest req;
  req.cmd = kCmdSetConfig;
  req.data.push_back(CfgSystem);
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&patched);
  req.data.insert(req.data.end(), bytes, bytes + sizeof(patched));

  CommandResponse resp;
  TEST_ASSERT_TRUE(setCmd.execute(req, resp).isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_UINT8(CfgSystem, resp.data[0]);
  TEST_ASSERT_EQUAL_STRING("factory-2", reinterpret_cast<const char*>(cfg.system.deviceName));
  TEST_ASSERT_TRUE(storage.exists("sys"));

  // GET_CONFIG возвращает то же, что записали.
  CommandRequest getReq;
  getReq.cmd = kCmdGetConfig;
  getReq.data.assign(1, CfgSystem);
  CommandResponse getResp;
  TEST_ASSERT_TRUE(getCmd.execute(getReq, getResp).isOk());
  TEST_ASSERT_EQUAL_UINT32(sizeof(SystemConfig), getResp.data.size());
  TEST_ASSERT_EQUAL_MEMORY(&patched, getResp.data.data(), sizeof(patched));
}

void test_config_set_wrong_size_rejected() {
  AppConfig cfg = makeDefaultAppConfig();
  RamConfigStorage storage;
  SetConfigCommand setCmd(cfg, storage, s_logger);

  CommandRequest req;
  req.cmd = kCmdSetConfig;
  req.data.assign(1 + sizeof(SystemConfig) - 5, 0); // на 5 байт короче секции
  req.data[0] = CfgSystem;

  CommandResponse resp;
  TEST_ASSERT_EQUAL_INT(Status::InvalidArgument,
                        static_cast<int>(setCmd.execute(req, resp).status));
  TEST_ASSERT_FALSE(storage.exists("sys"));
}

void test_config_set_unknown_section_rejected() {
  AppConfig cfg = makeDefaultAppConfig();
  RamConfigStorage storage;
  SetConfigCommand setCmd(cfg, storage, s_logger);

  CommandRequest req;
  req.cmd = kCmdSetConfig;
  req.data.assign(1 + sizeof(SystemConfig), 0);
  req.data[0] = 0x55; // нет такой секции

  CommandResponse resp;
  TEST_ASSERT_EQUAL_INT(Status::InvalidArgument,
                        static_cast<int>(setCmd.execute(req, resp).status));
}

void test_reboot_command_schedules_delay() {
  uint32_t restartAtMs = 0;
  RebootCommand rebootCmd(restartAtMs);

  CommandRequest req;
  req.cmd = kCmdReboot;
  req.data = {0x00, 0xC8}; // 200 мс BE16
  CommandResponse resp;
  TEST_ASSERT_TRUE(rebootCmd.execute(req, resp).isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
  // Таймер установлен в будущее (задержка ~200 мс, с запасом на планировщик).
  TEST_ASSERT_TRUE(restartAtMs > nowMs() && restartAtMs <= nowMs() + 200);

  // Пустое тело — дефолтная задержка 500 мс.
  CommandRequest empty;
  TEST_ASSERT_TRUE(rebootCmd.execute(empty, resp).isOk());
  TEST_ASSERT_TRUE(restartAtMs > nowMs() + 300);
}

void test_config_commands_via_session_roundtrip() {
  // Сквозной круг через Session: клиент по каналу читает и пишет секцию.
  AppConfig cfg = makeDefaultAppConfig();
  RamConfigStorage storage;
  TwoSessions ts;

  // Сервер обслуживает PING/GET/SET на одной и той же сессии.
  ts.serverDispatcher.registerCommand(
      std::unique_ptr<ICommand>(new SetConfigCommand(cfg, storage, s_logger)));
  ts.serverDispatcher.registerCommand(std::unique_ptr<ICommand>(new GetConfigCommand(cfg)));

  // SET: меняем deviceType (первые 2 байта SystemConfig, хост-порядок).
  SystemConfig patched;
  memcpy(&patched, &cfg.system, sizeof(patched));
  patched.deviceType = 0x0B0B;
  CommandResponse resp;
  std::vector<uint8_t> setBody;
  setBody.push_back(CfgSystem);
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&patched);
  setBody.insert(setBody.end(), bytes, bytes + sizeof(patched));
  TEST_ASSERT_TRUE(ts.client->sendRequest(kCmdSetConfig, 0, setBody, resp, 100).isOk());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Status::Ok), static_cast<int>(resp.status));
  TEST_ASSERT_EQUAL_UINT16(0x0B0B, cfg.system.deviceType);

  // GET: возвращаем секцию и сверяем.
  const uint8_t getBody[] = {CfgSystem};
  TEST_ASSERT_TRUE(ts.client->sendRequest(kCmdGetConfig, 0,
                                          std::vector<uint8_t>(getBody, getBody + 1), resp,
                                          100).isOk());
  TEST_ASSERT_EQUAL_UINT32(sizeof(SystemConfig), resp.data.size());
  TEST_ASSERT_EQUAL_MEMORY(&patched, resp.data.data(), sizeof(patched));
}
