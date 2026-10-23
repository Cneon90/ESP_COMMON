// File: include/esp_common/commands/GetConfigCommand.h
#pragma once
/// @file GetConfigCommand.h
/// @brief Команда GET_CONFIG (cmd=0x0002): чтение секции AppConfig.

#include "esp_common/app/AppConfig.h"
#include "esp_common/commands/ICommand.h"

namespace esp_common {

/// Тело запроса = [0]=ConfigSection; тело ответа = текущие байты секции.
class GetConfigCommand final : public ICommand {
 public:
  explicit GetConfigCommand(const AppConfig& config) : m_config(config) {}

  uint16_t cmdId() const override { return kCmdGetConfig; }

  Result execute(const CommandRequest& req, CommandResponse& resp) override;

 private:
  const AppConfig& m_config;
};

} // namespace esp_common
