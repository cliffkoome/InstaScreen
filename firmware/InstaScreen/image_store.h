// Reads chunked JPEGs from the Realtime Database and reassembles them into a
// single, integrity-checked buffer.
//
// Supported wire formats (see README "Data model"):
//   v2  images/image_N/meta   {v, encoding:"base64", size, crc32, chunk_count}
//       images/image_N/chunks [base64, ...]
//   v1  images/image_N/chunk_count  N
//       images/image_N/chunks       [hex, ...]          (legacy uploader)
#pragma once

#include <Arduino.h>

#include "rtdb_client.h"

// Owning, move-only heap buffer for a downloaded JPEG.
class ImageBuffer {
 public:
  ImageBuffer() = default;
  ~ImageBuffer() { release(); }
  ImageBuffer(const ImageBuffer&) = delete;
  ImageBuffer& operator=(const ImageBuffer&) = delete;

  bool allocate(size_t capacity);
  bool grow(size_t capacity);
  void release();

  uint8_t* data() { return data_; }
  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }
  void setSize(size_t n) { size_ = n; }

 private:
  uint8_t* data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
};

enum class FetchError {
  None,
  NotFound,     // slot is empty or has no metadata
  Network,      // transport failure; worth retrying the same slot
  Denied,       // HTTP error (rules, bad host); retrying won't help soon
  TooLarge,     // exceeds cfg::kMaxImageBytes
  OutOfMemory,  // no contiguous heap block large enough
  Corrupt,      // size/CRC/JPEG-marker mismatch or malformed metadata
};

const char* toString(FetchError e);

using ProgressFn = void (*)(uint16_t done, uint16_t total);

class ImageStore {
 public:
  explicit ImageStore(RtdbClient& client) : client_(client) {}

  // Reads images/image_count. A missing node is reported as count = 0.
  FetchError fetchImageCount(int& count);

  // Downloads slot `slot` (1-based) into `out`. `out` holds a verified JPEG
  // on success and is released on failure: partial images are never returned.
  FetchError fetchImage(int slot, ImageBuffer& out, ProgressFn progress = nullptr);

 private:
  struct Meta {
    uint8_t version = 0;
    uint16_t chunkCount = 0;
    uint32_t size = 0;    // v2 only
    uint32_t crc32 = 0;   // v2 only
  };

  FetchError fetchMeta(const String& slotPath, Meta& meta);
  FetchError fetchChunks(const String& slotPath, const Meta& meta, ImageBuffer& out,
                         ProgressFn progress);

  RtdbClient& client_;
};
