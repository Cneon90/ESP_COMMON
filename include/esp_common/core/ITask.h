// File: include/esp_common/core/ITask.h
#pragma once
/// @file ITask.h
/// @brief Интерфейс кооперативной задачи (setup/loop).

namespace esp_common {

/// @brief Кооперативная задача с явными фазами инициализации и цикла.
///
/// Используется для верхнего уровня (IApp) и будущих фоновых задач.
/// Никаких потоков: все задачи вызываются из одного loop() (Arduino-модель).
class ITask {
 public:
  virtual ~ITask() = default;

  /// Имя задачи (для логов и диагностики).
  virtual const char* name() const = 0;

  /// Разовая инициализация. После setup() задача готова к loop().
  virtual void setup() = 0;

  /// Один оборот кооперативного цикла; не должен блокироваться надолго.
  virtual void loop() = 0;
};

} // namespace esp_common
