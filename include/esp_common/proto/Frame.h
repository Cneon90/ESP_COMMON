// File: include/esp_common/proto/Frame.h
#pragma once
/// @file Frame.h
/// @brief Хост-структура фрейма (после/до кодека).

#include <stdint.h>
#include <vector>

#include "esp_common/proto/ProtocolDefs.h"

namespace esp_common {

/// @brief Фрейм в хост-представлении.
///
/// payload — тело «как в проводе»: на отправке в шифрованных режимах сюда
/// кладут уже шифротекст, на приёме кодек возвращает шифротекст, а Session
/// расшифровывает на месте. Кодек не знает о криптографии (см. IFrameCodec).
struct Frame {
  uint8_t flags = 0;               ///< kFrameFlagResponse / kFrameFlagError
  CryptoMode cryptoMode = CryptoPlain; ///< Режим шифрования тела.
  uint16_t seq = 0;                ///< Номер фрейма (replay-защита).
  uint16_t cmd = 0;                ///< Код команды.
  uint8_t param = 0;               ///< Параметр команды.
  std::vector<uint8_t> payload;    ///< Тело (единственная динамика на фрейм).

  /// Сбросить всё (ёмкость payload сохраняется).
  void clear() {
    flags = 0;
    cryptoMode = CryptoPlain;
    seq = 0;
    cmd = 0;
    param = 0;
    payload.clear();
  }

  bool isResponse() const { return (flags & kFrameFlagResponse) != 0; }
  bool isError() const { return (flags & kFrameFlagError) != 0; }

  void markResponse(bool on = true) { setFlag(kFrameFlagResponse, on); }
  void markError(bool on = true) { setFlag(kFrameFlagError, on); }

 private:
  void setFlag(uint8_t mask, bool on) {
    if (on) {
      flags = static_cast<uint8_t>(flags | mask);
    } else {
      flags = static_cast<uint8_t>(flags & static_cast<uint8_t>(~mask));
    }
  }
};

} // namespace esp_common
