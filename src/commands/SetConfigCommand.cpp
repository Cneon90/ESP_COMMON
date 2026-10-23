// File: src/commands/SetConfigCommand.cpp
#include "esp_common/commands/SetConfigCommand.h"

#include <string.h>

namespace esp_common {

SetConfigCommand::SetConfigCommand(AppConfig& config, IConfigStorage& storage, ILogger& logger)
    : m_config(config), m_storage(storage), m_logger(logger) {}

Result SetConfigCommand::execute(const CommandRequest& req, CommandResponse& resp) {
  resp.param = req.param;
  if (req.data.empty()) {
    return Result::err(Status::InvalidArgument);
  }
  const auto section = static_cast<ConfigSection>(req.data[0]);
  const size_t expected = configSectionSize(section);
  void* dst = configSectionPtr(m_config, section);
  if (expected == 0 || dst == nullptr) {
    m_logger.log(LogLevel::Warn, "SetConfig", "unknown section %u",
                 static_cast<unsigned>(req.data[0]));
    return Result::err(Status::InvalidArgument);
  }
  if (req.data.size() - 1u != expected) {
    m_logger.log(LogLevel::Warn, "SetConfig", "section %u: size %u != %u",
                 static_cast<unsigned>(req.data[0]),
                 static_cast<unsigned>(req.data.size() - 1u), static_cast<unsigned>(expected));
    return Result::err(Status::InvalidArgument);
  }

  memcpy(dst, req.data.data() + 1, expected);
  const Result sr = m_storage.save(configSectionKey(section), dst, expected);
  if (!sr.isOk()) {
    m_logger.log(LogLevel::Error, "SetConfig", "section %u save failed: %s",
                 static_cast<unsigned>(req.data[0]), statusName(sr.status));
    return Result::err(Status::IoError, sr.code);
  }
  resp.status = Status::Ok;
  resp.data.assign(1, req.data[0]); // эхо секции
  return Result::ok();
}

} // namespace esp_common
