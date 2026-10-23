// File: main.cpp
/// @file main.cpp
/// @brief Точка входа Esp_common: запуск приложения в роли из AppConfig.

#include <Arduino.h>

#include <memory>

#include "esp_common/app/App.h"
#include "esp_common/core/ILogger.h"

namespace {

// Единственная «глобальная» точка входа (все остальные объекты — в setup()).
esp_common::IApp* g_app = nullptr;

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  static esp_common::SerialLogger logger(esp_common::LogLevel::Info);
  static esp_common::AppConfig config = esp_common::makeDefaultAppConfig();

  static esp_common::App app(config, logger); // живёт весь цикл устройства
  if (app.init()) {
    g_app = &app;
  } else {
    Serial.println("[FATAL] App init failed");
  }
}

void loop() {
  if (g_app != nullptr) {
    g_app->loop();
  }
}
