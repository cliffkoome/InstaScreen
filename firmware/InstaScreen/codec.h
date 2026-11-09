// Pure, platform-independent payload codecs.
//
// No Arduino / ESP-IDF dependencies: this file is compiled both into the
// firmware and into the host-side unit tests (`pio test -e native`).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace codec {

struct DecodeResult {
  size_t written;  // bytes written to `out`
  bool overflow;   // input held more data than `outCap` allowed
};

// Decodes hex digits (wire format v1). Any non-hex character (JSON quotes,
// whitespace) is skipped. A trailing unpaired nibble is ignored.
DecodeResult hexDecode(const char* in, size_t inLen, uint8_t* out, size_t outCap);

// Decodes standard base64 (wire format v2). Characters outside the base64
// alphabet (JSON quotes, an escaped "\/") are skipped; decoding stops at '='.
DecodeResult base64Decode(const char* in, size_t inLen, uint8_t* out, size_t outCap);

// CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320). Matches Python's
// zlib.crc32(). Pass the previous return value as `crc` to checksum
// incrementally; start with 0.
uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0);

// True if the buffer starts with a JPEG SOI marker (FFD8) and ends with an EOI
// marker (FFD9). A cheap structural check that catches truncation in v1 data,
// which carries no checksum.
bool looksLikeJpeg(const uint8_t* data, size_t len);

}  // namespace codec
