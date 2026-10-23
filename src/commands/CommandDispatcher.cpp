// File: src/commands/CommandDispatcher.cpp
#include "esp_common/commands/CommandDispatcher.h"

namespace esp_common {

CommandDispatcher::CommandDispatcher(ILogger& logger) : m_logger(logger) {}

void CommandDispatcher::registerCommand(std::unique_ptr<ICommand> command) {
  if (!command) {
    return;
  }
  const uint16_t id = command->cmdId();
  const auto it = m_commands.find(id);
  if (it != m_commands.end()) {
    m_logger.log(LogLevel::Warn, "Dispatcher", "cmd 0x%04X re-registered (replaced)", id);
    it->second = std::move(command);
    return;
  }
  m_commands.emplace(id, std::move(command));
}

Result CommandDispatcher::dispatch(const CommandRequest& req, CommandResponse& resp) {
  resp.param = req.param;
  const auto it = m_commands.find(req.cmd);
  if (it == m_commands.end()) {
    m_logger.log(LogLevel::Debug, "Dispatcher", "unknown cmd 0x%04X -> NotImplemented", req.cmd);
    return m_notImplemented.execute(req, resp);
  }
  const Result r = it->second->execute(req, resp);
  if (!r.isOk() && resp.status == Status::Ok) {
    resp.status = r.status; // итог выполнения — в статус ответа
  }
  return Result::ok();
}

} // namespace esp_common
