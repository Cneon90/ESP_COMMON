// File: include/esp_common/commands/PingCommand.h
#pragma once
/// @file PingCommand.h
/// @brief Команда PING (cmd=0x0000): ответ — эхо тела запроса.

#include "esp_common/commands/ICommand.h"

namespace esp_common {

/// PING: тело запроса произвольное; ответ — то же тело, flags.response=1,
/// seq совпадает с запросом (обеспечивает Session).
class PingCommand final : public ICommand {
 public:
  PingCommand() = default;

  uint16_t cmdId() const override { return kCmdPing; }

  Result execute(const CommandRequest& req, CommandResponse& resp) override;
};

} // namespace esp_common
