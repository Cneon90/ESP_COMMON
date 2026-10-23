// File: include/esp_common/commands/CommandDispatcher.h
#pragma once
/// @file CommandDispatcher.h
/// @brief Реализация ICommandDispatcher: map cmd → ICommand + заглушка.

#include <map>
#include <memory>

#include "esp_common/commands/ICommand.h"
#include "esp_common/commands/NotImplementedCommand.h"
#include "esp_common/core/ILogger.h"

namespace esp_common {

/// Маршрутизатор запросов. Новый код команды добавляется ТОЛЬКО регистрацией
/// нового ICommand — сам диспетчер не правится (OCP).
class CommandDispatcher final : public ICommandDispatcher {
 public:
  explicit CommandDispatcher(ILogger& logger);

  void registerCommand(std::unique_ptr<ICommand> command) override;

  /// Неизвестный cmd обслуживает встроенный NotImplementedCommand.
  Result dispatch(const CommandRequest& req, CommandResponse& resp) override;

  /// Число зарегистрированных команд.
  size_t count() const { return m_commands.size(); }

 private:
  ILogger& m_logger;
  std::map<uint16_t, std::unique_ptr<ICommand>> m_commands;
  NotImplementedCommand m_notImplemented;
};

} // namespace esp_common
