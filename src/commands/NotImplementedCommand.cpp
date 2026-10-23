// File: src/commands/NotImplementedCommand.cpp
#include "esp_common/commands/NotImplementedCommand.h"

namespace esp_common {

Result NotImplementedCommand::execute(const CommandRequest& req, CommandResponse& resp) {
  resp.param = req.param;
  resp.status = Status::NotImplemented;
  resp.data.clear();
  resp.data.push_back(static_cast<uint8_t>(req.cmd >> 8));
  resp.data.push_back(static_cast<uint8_t>(req.cmd & 0xFFu));
  return Result::ok();
}

} // namespace esp_common
