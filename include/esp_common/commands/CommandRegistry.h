// File: include/esp_common/commands/CommandRegistry.h
#pragma once
/// @file CommandRegistry.h
/// @brief Реестр команд: сборка набора команд отдельно от диспетчера.

#include <memory>
#include <vector>

#include "esp_common/commands/ICommand.h"

namespace esp_common {

/// @brief Контейнер команд с последующей установкой в диспетчер.
///
/// Разделяет состав команд (что регистрируем) и механизм маршрутизации
/// (CommandDispatcher): App собирает набор здесь, не трогая сессию/диспетчер.
class CommandRegistry {
 public:
  /// Добавить команду во временный набор (владение остаётся у реестра).
  void add(std::unique_ptr<ICommand> command);

  /// Перенести все команды в @p dispatcher (владение переходит ему).
  void installTo(ICommandDispatcher& dispatcher);

  /// Найти команду в наборе (для диагностики), не меняя владения.
  ICommand* find(uint16_t cmdId) const;

  /// Текущий размер набора.
  size_t size() const { return m_commands.size(); }

 private:
  std::vector<std::unique_ptr<ICommand>> m_commands;
};

} // namespace esp_common
