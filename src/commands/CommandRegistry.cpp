// File: src/commands/CommandRegistry.cpp
#include "esp_common/commands/CommandRegistry.h"

namespace esp_common {

void CommandRegistry::add(std::unique_ptr<ICommand> command) {
  if (command) {
    m_commands.push_back(std::move(command));
  }
}

void CommandRegistry::installTo(ICommandDispatcher& dispatcher) {
  for (auto& command : m_commands) {
    dispatcher.registerCommand(std::move(command));
  }
  m_commands.clear();
}

ICommand* CommandRegistry::find(uint16_t cmdId) const {
  for (const auto& command : m_commands) {
    if (command->cmdId() == cmdId) {
      return command.get();
    }
  }
  return nullptr;
}

} // namespace esp_common
