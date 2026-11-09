#include "codec.h"

namespace codec {
namespace {

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int base64Value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

}  // namespace

DecodeResult hexDecode(const char* in, size_t inLen, uint8_t* out, size_t outCap) {
  DecodeResult r{0, false};
  int high = -1;
  for (size_t i = 0; i < inLen; ++i) {
    const int v = hexValue(in[i]);
    if (v < 0) continue;
    if (high < 0) {
      high = v;
      continue;
    }
    if (r.written >= outCap) {
      r.overflow = true;
      return r;
    }
    out[r.written++] = static_cast<uint8_t>((high << 4) | v);
    high = -1;
  }
  return r;
}

DecodeResult base64Decode(const char* in, size_t inLen, uint8_t* out, size_t outCap) {
  DecodeResult r{0, false};
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = 0; i < inLen; ++i) {
    const char c = in[i];
    if (c == '=') break;
    const int v = base64Value(c);
    if (v < 0) continue;
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (r.written >= outCap) {
        r.overflow = true;
        return r;
      }
      out[r.written++] = static_cast<uint8_t>((acc >> bits) & 0xFF);
    }
  }
  return r;
}

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

bool looksLikeJpeg(const uint8_t* data, size_t len) {
  return len >= 4 && data[0] == 0xFF && data[1] == 0xD8 && data[len - 2] == 0xFF &&
         data[len - 1] == 0xD9;
}

}  // namespace codec
