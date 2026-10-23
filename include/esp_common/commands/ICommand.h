// File: include/esp_common/commands/ICommand.h
#pragma once
/// @file ICommand.h
/// @brief Интерфейсы команд и диспетчера. О транспорте и роли ничего не знают.

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "esp_common/core/Result.h"

namespace esp_common {

/// Код команды PING (эхо данных).
constexpr uint16_t kCmdPing = 0x0000;
/// Записать секцию настроек: тело = [0]=ConfigSection, [1..]=pack(1) байты секции.
constexpr uint16_t kCmdSetConfig = 0x0001;
/// Прочитать секцию настроек: тело = [0]=ConfigSection; ответ = байты секции.
constexpr uint16_t kCmdGetConfig = 0x0002;
/// Перезагрузить устройство (применение настроек): тело = [0..1]=задержка BE, мс.
constexpr uint16_t kCmdReboot = 0x0003;
/// Псевдо-код для ответа «команда не реализована».
constexpr uint16_t kCmdNotImplemented = 0xFFFF;

/// @brief Запрос команды (payload фрейма после расшифровки).
struct CommandRequest {
  uint16_t cmd = 0;              ///< Код команды (из заголовка фрейма).
  uint8_t param = 0;             ///< Параметр (из заголовка фрейма).
  std::vector<uint8_t> data;     ///< Тело запроса.
};

/// @brief Ответ команды (до упаковки в фрейм).
///
/// Правило протокола для ошибок (resp.status != Ok): Session добавит к телу
/// первый байт = код Status, дальше — данные команды (у NotImplementedCommand
/// это BE-код запрошенной команды). Клиент при flags.error трактует payload[0]
/// как Status, остальное — как данные.
struct CommandResponse {
  uint8_t param = 0;             ///< Эхо param запроса.
  Status status = Status::Ok;    ///< Итог выполнения.
  std::vector<uint8_t> data;     ///< Тело ответа.
};

/// @brief Одна команда протокола.
///
/// Новая команда = новый класс с этим интерфейсом, зарегистрированный в
/// диспетчере. Правки Session/Dispatcher не требуются (OCP).
class ICommand {
 public:
  virtual ~ICommand() = default;

  /// Код команды на проводе (cmd в заголовке фрейма).
  virtual uint16_t cmdId() const = 0;

  /// @brief Выполнить запрос и заполнить ответ.
  /// @note Реализация не должна блокироваться надолго и не знает про транспорт.
  virtual Result execute(const CommandRequest& req, CommandResponse& resp) = 0;
};

/// @brief Маршрутизатор запросов к командам.
///
/// Неизвестный cmd обслуживается NotImplementedCommand. dispatch всегда
/// возвращает Ok — итог выполнения передаётся через resp.status.
class ICommandDispatcher {
 public:
  virtual ~ICommandDispatcher() = default;

  /// Зарегистрировать команду (владение переходит диспетчеру).
  virtual void registerCommand(std::unique_ptr<ICommand> command) = 0;

  /// @brief Исполнить запрос: найти команду или ответить NotImplemented.
  virtual Result dispatch(const CommandRequest& req, CommandResponse& resp) = 0;
};

} // namespace esp_common
