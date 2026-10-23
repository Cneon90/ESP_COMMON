// File: include/esp_common/proto/IFrameCodec.h
#pragma once
/// @file IFrameCodec.h
/// @brief Интерфейс кодека фреймов (только plain-заголовок + CRC).

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "esp_common/core/Result.h"
#include "esp_common/proto/Frame.h"

namespace esp_common {

/// @brief Кодек бинарных фреймов.
///
/// Знает ТОЛЬКО «прозрачный» заголовок (magic/flags/crypto/seq/cmd/param/length)
/// и CRC по заголовку + телу. Шифрование/расшифрование — не его забота:
/// тело передаётся «как в проводе».
///
/// Ожидается, что @p buf начинается ровно с магии фрейма (потоковую синхронизацию
/// ведёт вызывающий — Session).
class IFrameCodec {
 public:
  virtual ~IFrameCodec() = default;

  /// @brief Закодировать фрейм в байты (заголовок + тело + CRC).
  /// @param in   фрейм (payload — тело как в проводе);
  /// @param out  выходной буфер (переписывается).
  virtual Result encode(const Frame& in, std::vector<uint8_t>& out) = 0;

  /// @brief Декодировать фрейм из начала буфера.
  /// @param buf,len   входные данные (возможно, несколько фреймов подряд);
  /// @param out       декодированный фрейм;
  /// @param consumed  сколько байт занимает этот фрейм (при Status::Ok).
  /// @return Ok; NeedMoreData (фрейм ещё не полный); ProtocolError/CrcError.
  virtual Result decode(const uint8_t* buf, size_t bufLen, Frame& out, size_t& consumed) = 0;
};

} // namespace esp_common
