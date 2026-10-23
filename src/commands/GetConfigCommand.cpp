// File: src/commands/GetConfigCommand.cpp
#include "esp_common/commands/GetConfigCommand.h"

#include <string.h>

namespace esp_common {

Result GetConfigCommand::execute(const CommandRequest& req, CommandResponse& resp) {
  resp.param = req.param;
  if (req.data.empty()) {
    return Result::err(Status::InvalidArgument);
  }
  const auto section = static_cast<ConfigSection>(req.data[0]);
  const size_t size = configSectionSize(section);
  const void* src = configSectionPtr(m_config, section);
  if (size == 0 || src == nullptr) {
    return Result::err(Status::InvalidArgument);
  }
  const uint8_t* bytes = static_cast<const uint8_t*>(src);
  resp.status = Status::Ok;
  resp.data.assign(bytes, bytes + size);
  return Result::ok();
}

} // namespace esp_common
