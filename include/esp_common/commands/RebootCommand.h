// File: include/esp_common/commands/RebootCommand.h
#pragma once
/// @file RebootCommand.h
/// @brief Команда REBOOT (cmd=0x0003): отложенная перезагрузка устройства.

#include <stdint.h>

#include "esp_common/commands/ICommand.h"

namespace esp_common {

/// @brief Запланировать перезагрузку (для применения настроек WiFi и т.п.).
///
/// Команда НЕ перезагружает устройство немедленно — сначала Session успевает
/// отправить ответ, затем App::checkReboot() выполняет ESP.restart().
/// Задержка задаётся телом запроса: [0..1] = BE16, мс (по умолчанию 500,
/// минимум 100). Владелец ссылки m_restartAtMs — App.
class RebootCommand final : public ICommand {
 public:
  explicit RebootCommand(uint32_t& restartAtMsRef) : m_restartAtMs(restartAtMsRef) {}

  uint16_t cmdId() const override { return kCmdReboot; }

  Result execute(const CommandRequest& req, CommandResponse& resp) override;

 private:
  uint32_t& m_restartAtMs; ///< Референс на таймер перезагрузки в App.
};

} // namespace esp_common
