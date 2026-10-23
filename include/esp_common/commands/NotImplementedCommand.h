// File: include/esp_common/commands/NotImplementedCommand.h
#pragma once
/// @file NotImplementedCommand.h
/// @brief Ответ на неизвестную команду: Status::NotImplemented + эхо cmd.

#include "esp_common/commands/ICommand.h"

namespace esp_common {

/// Заглушка для незарегистрированных cmd. В теле ответа — BE-код запрошенной
/// команды (после добавления Session'ом байта Status).
class NotImplementedCommand final : public ICommand {
 public:
  NotImplementedCommand() = default;

  uint16_t cmdId() const override { return kCmdNotImplemented; }

  Result execute(const CommandRequest& req, CommandResponse& resp) override;
};

} // namespace esp_common
