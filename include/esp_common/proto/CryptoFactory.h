// File: include/esp_common/proto/CryptoFactory.h
#pragma once
/// @file CryptoFactory.h
/// @brief Фабрика ICrypto по режиму из handshake.

#include <memory>
#include <stdint.h>

#include "esp_common/proto/ICrypto.h"

namespace esp_common {

/// Создаёт реализацию ICrypto по negotiated-режиму сессии.
class CryptoFactory {
 public:
  /// @return объект режима или nullptr для неизвестного/недоступного режима.
  static std::unique_ptr<ICrypto> create(CryptoMode mode);

  /// Длина ключа режима в байтах (0 для plain, 16/32 для AES, 32 для ChaCha20).
  static size_t keyLengthFor(CryptoMode mode);

  /// Известен ли режим вообще (0..kCryptoModeCount-1).
  static bool isKnown(CryptoMode mode);

  /// @brief Битовая маска режимов, доступных в ЭТОЙ сборке.
  ///
  /// ChaCha20 отсутствует в предсобранных mbedTLS части платформ
  /// (arduino-esp32 2.x), поэтому mask = kCryptoSupportAll без бита ChaCha20.
  /// Handshake обязан согласовать только режимы из этой маски.
  static uint8_t supportedModesMask();
};

} // namespace esp_common
