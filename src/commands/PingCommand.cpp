// File: src/commands/PingCommand.cpp
#include "esp_common/commands/PingCommand.h"

namespace esp_common {

Result PingCommand::execute(const CommandRequest& req, CommandResponse& resp) {
  resp.param = req.param;
  resp.status = Status::Ok;
  resp.data = req.data; // эхо
  return Result::ok();
}

} // namespace esp_common
