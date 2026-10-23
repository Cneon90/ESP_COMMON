// File: include/esp_common/proto/FrameCodec.h
#pragma once
/// @file FrameCodec.h
/// @brief Реализация IFrameCodec по спецификации ProtocolDefs.h.

#include "esp_common/proto/IFrameCodec.h"

namespace esp_common {

/// Кодек фреймов: 13-байтный заголовок + тело, CRC-16/CCITT-FALSE.
class FrameCodec final : public IFrameCodec {
 public:
  FrameCodec() = default;

  Result encode(const Frame& in, std::vector<uint8_t>& out) override;
  Result decode(const uint8_t* buf, size_t bufLen, Frame& out, size_t& consumed) override;
};

} // namespace esp_common
