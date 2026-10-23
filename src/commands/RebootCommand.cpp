// File: src/commands/RebootCommand.cpp
#include "esp_common/commands/RebootCommand.h"

#include "esp_common/core/Result.h"
#include "esp_common/proto/ProtocolDefs.h"

namespace esp_common {
namespace {
constexpr uint32_t kDefaultRebootDelayMs = 500;
constexpr uint32_t kMinRebootDelayMs = 100; // чтобы ответ успел уйти
} // namespace

Result RebootCommand::execute(const CommandRequest& req, CommandResponse& resp) {
  resp.param = req.param;
  uint32_t delay = kDefaultRebootDelayMs;
  if (req.data.size() >= 2) {
    delay = loadBE16(req.data.data());
  }
  if (delay < kMinRebootDelayMs) {
    delay = kMinRebootDelayMs;
  }
  m_restartAtMs = nowMs() + delay;

  resp.status = Status::Ok;
  resp.data = {static_cast<uint8_t>(delay >> 8), static_cast<uint8_t>(delay & 0xFFu)};
  return Result::ok();
}

} // namespace esp_common
