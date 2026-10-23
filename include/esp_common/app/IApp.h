// File: include/esp_common/app/IApp.h
#pragma once
/// @file IApp.h
/// @brief Интерфейс приложения (верхний уровень, точка входа main.cpp).

#include "esp_common/app/AppConfig.h"
#include "esp_common/core/ITask.h"

namespace esp_common {

/// @brief Приложение: инициализирует WiFi/транспорт по AppConfig (роль узла),
///        регистрирует команды и крутит цикл сессий.
class IApp : public ITask {
 public:
  virtual ~IApp() = default;

  /// Разовая инициализация (WiFi, транспорт, команды). false — критическая ошибка.
  virtual bool init() = 0;

  /// Конфигурация, с которой работает приложение.
  virtual const AppConfig& config() const = 0;
};

} // namespace esp_common
