// File: include/esp_common/commands/SetConfigCommand.h
#pragma once
/// @file SetConfigCommand.h
/// @brief Команда SET_CONFIG (cmd=0x0001): запись секции AppConfig + NVS.

#include "esp_common/app/AppConfig.h"
#include "esp_common/app/IConfigStorage.h"
#include "esp_common/commands/ICommand.h"
#include "esp_common/core/ILogger.h"

namespace esp_common {

/// @brief Записать секцию настроек: тело запроса = [0]=ConfigSection,
///        [1..] = ровно sizeof(секции) байт pack(1)-структуры.
///
/// Значение применяется в памяти сразу и сохраняется в хранилище (NVS),
/// чтобы пережить перезагрузку. Часть настроек (WiFi, роль, порты)
/// вступает в силу после REBOOT.
class SetConfigCommand final : public ICommand {
 public:
  SetConfigCommand(AppConfig& config, IConfigStorage& storage, ILogger& logger);

  uint16_t cmdId() const override { return kCmdSetConfig; }

  Result execute(const CommandRequest& req, CommandResponse& resp) override;

 private:
  AppConfig& m_config;
  IConfigStorage& m_storage;
  ILogger& m_logger;
};

} // namespace esp_common
