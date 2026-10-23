// File: src/proto/FrameCodec.cpp
#include "esp_common/proto/FrameCodec.h"

#include <string.h>

#include "esp_common/proto/ProtocolDefs.h"
#include "esp_common/util/crc16.h"

namespace esp_common {

Result FrameCodec::encode(const Frame& in, std::vector<uint8_t>& out) {
  if (in.payload.size() > kMaxFrameSize) {
    return Result::err(Status::InvalidArgument);
  }
  if (static_cast<uint8_t>(in.cryptoMode) >= kCryptoModeCount) {
    return Result::err(Status::InvalidArgument);
  }

  out.assign(kFrameHeaderSize + in.payload.size(), 0);
  FrameWireHeader& h = *reinterpret_cast<FrameWireHeader*>(out.data());
  h.init();
  h.setFlags(in.flags);
  h.setCryptoMode(in.cryptoMode);
  h.setSeq(in.seq);
  h.setCmd(in.cmd);
  h.param = in.param;
  h.setLength(static_cast<uint16_t>(in.payload.size()));

  if (!in.payload.empty()) {
    memcpy(out.data() + kFrameHeaderSize, in.payload.data(), in.payload.size());
  }

  // CRC по заголовку без crc16 (11 байт) + телу.
  uint16_t crc = h.crcOfHeader();
  if (!in.payload.empty()) {
    crc = crc16_ccitt_update(crc, out.data() + kFrameHeaderSize, in.payload.size());
  }
  h.setCrc(crc);
  return Result::ok();
}

Result FrameCodec::decode(const uint8_t* buf, size_t bufLen, Frame& out, size_t& consumed) {
  consumed = 0;
  if (bufLen < kFrameHeaderSize) {
    return Result::err(Status::NeedMoreData);
  }
  const FrameWireHeader& h = *reinterpret_cast<const FrameWireHeader*>(buf);
  if (h.magic != kFrameMagic) {
    return Result::err(Status::ProtocolError);
  }

  const uint16_t length = h.getLength();
  if (length > kMaxFrameSize) {
    return Result::err(Status::ProtocolError);
  }
  const size_t total = kFrameHeaderSize + static_cast<size_t>(length);
  if (bufLen < total) {
    return Result::err(Status::NeedMoreData);
  }

  const uint16_t crcGot = h.getCrc();
  uint16_t crcCalc = h.crcOfHeader();
  if (length > 0) {
    crcCalc = crc16_ccitt_update(crcCalc, buf + kFrameHeaderSize, length);
  }
  if (crcGot != crcCalc) {
    return Result::err(Status::CrcError);
  }

  const uint8_t cryptoMode = buf[2];
  if (cryptoMode >= kCryptoModeCount) {
    return Result::err(Status::ProtocolError);
  }

  out.clear();
  out.flags = h.getFlags();
  out.cryptoMode = static_cast<CryptoMode>(cryptoMode);
  out.seq = h.getSeq();
  out.cmd = h.getCmd();
  out.param = h.param;
  if (length > 0) {
    out.payload.assign(buf + kFrameHeaderSize, buf + kFrameHeaderSize + length);
  }
  consumed = total;
  return Result::ok();
}

} // namespace esp_common
