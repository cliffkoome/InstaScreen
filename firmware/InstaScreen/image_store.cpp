#include "image_store.h"

#include <ArduinoJson.h>

#include <algorithm>

#include "codec.h"
#include "config.h"

// ---------------------------------------------------------------- ImageBuffer

bool ImageBuffer::allocate(size_t capacity) {
  release();
  data_ = static_cast<uint8_t*>(malloc(capacity));
  if (!data_) return false;
  capacity_ = capacity;
  return true;
}

bool ImageBuffer::grow(size_t capacity) {
  if (capacity <= capacity_) return true;
  auto* p = static_cast<uint8_t*>(realloc(data_, capacity));
  if (!p) return false;  // original block is still owned and freed by release()
  data_ = p;
  capacity_ = capacity;
  return true;
}

void ImageBuffer::release() {
  free(data_);
  data_ = nullptr;
  size_ = 0;
  capacity_ = 0;
}

// ----------------------------------------------------------------- helpers

namespace {

FetchError fromStatus(RtdbClient::Status s) {
  switch (s) {
    case RtdbClient::Status::Ok: return FetchError::None;
    case RtdbClient::Status::Null: return FetchError::NotFound;
    case RtdbClient::Status::TooLarge: return FetchError::TooLarge;
    case RtdbClient::Status::HttpError: return FetchError::Denied;
    case RtdbClient::Status::Transport: return FetchError::Network;
  }
  return FetchError::Network;
}

String slotPath(int slot) {
  String p(cfg::kBasePath);
  p += "/image_";
  p += slot;
  return p;
}

}  // namespace

const char* toString(FetchError e) {
  switch (e) {
    case FetchError::None: return "ok";
    case FetchError::NotFound: return "not-found";
    case FetchError::Network: return "network";
    case FetchError::Denied: return "denied";
    case FetchError::TooLarge: return "too-large";
    case FetchError::OutOfMemory: return "out-of-memory";
    case FetchError::Corrupt: return "corrupt";
  }
  return "?";
}

// ----------------------------------------------------------------- ImageStore

FetchError ImageStore::fetchImageCount(int& count) {
  String body;
  String path(cfg::kBasePath);
  path += "/image_count";
  const auto st = client_.get(path, body, 32);
  if (st == RtdbClient::Status::Null) {
    count = 0;
    return FetchError::None;
  }
  if (st != RtdbClient::Status::Ok) return fromStatus(st);
  count = body.toInt();
  return count >= 0 ? FetchError::None : FetchError::Corrupt;
}

FetchError ImageStore::fetchMeta(const String& base, Meta& meta) {
  String body;

  // v2: a single small JSON object.
  auto st = client_.get(base + "/meta", body, 512);
  if (st == RtdbClient::Status::Ok) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) return FetchError::Corrupt;
    const char* encoding = doc["encoding"] | "";
    // as<uint32_t>() rather than `| 0`: a CRC above INT32_MAX would not
    // convert to the int default and would silently read as 0.
    meta.version = doc["v"].as<uint8_t>();
    meta.chunkCount = doc["chunk_count"].as<uint16_t>();
    meta.size = doc["size"].as<uint32_t>();
    meta.crc32 = doc["crc32"].as<uint32_t>();
    if (meta.version != 2 || strcmp(encoding, "base64") != 0 || meta.chunkCount == 0 ||
        meta.size == 0) {
      return FetchError::Corrupt;
    }
    return FetchError::None;
  }
  if (st != RtdbClient::Status::Null) return fromStatus(st);

  // v1 fallback: legacy nodes only carry a chunk count.
  st = client_.get(base + "/chunk_count", body, 32);
  if (st != RtdbClient::Status::Ok) return fromStatus(st);
  const long n = body.toInt();
  if (n <= 0 || n > 0xFFFF) return FetchError::Corrupt;
  meta.version = 1;
  meta.chunkCount = static_cast<uint16_t>(n);
  return FetchError::None;
}

FetchError ImageStore::fetchChunks(const String& base, const Meta& meta, ImageBuffer& out,
                                   ProgressFn progress) {
  // v2 knows its exact size, so allocate once. v1 starts from an estimate and
  // grows only if a chunk turns out larger than the legacy 1 KB default.
  if (meta.version == 2 && meta.size > cfg::kMaxImageBytes) return FetchError::TooLarge;
  const size_t initial =
      meta.version == 2
          ? static_cast<size_t>(meta.size)
          : std::min(static_cast<size_t>(meta.chunkCount) * cfg::kV1ChunkBytesHint, cfg::kMaxImageBytes);
  if (!out.allocate(initial)) return FetchError::OutOfMemory;

  size_t written = 0;
  String body;
  for (uint16_t i = 0; i < meta.chunkCount; ++i) {
    String path = base;
    path += "/chunks/";
    path += i;

    const auto st = client_.get(path, body, cfg::kMaxChunkBodyBytes);
    if (st != RtdbClient::Status::Ok) {
      // A missing chunk means the node is incomplete, not that the slot is empty.
      return st == RtdbClient::Status::Null ? FetchError::Corrupt : fromStatus(st);
    }

    codec::DecodeResult r;
    for (;;) {
      uint8_t* dst = out.data() + written;
      const size_t room = out.capacity() - written;
      r = meta.version == 2 ? codec::base64Decode(body.c_str(), body.length(), dst, room)
                            : codec::hexDecode(body.c_str(), body.length(), dst, room);
      if (!r.overflow) break;
      // v2 overflowing its declared size is corruption; v1 may legitimately grow.
      if (meta.version == 2) return FetchError::Corrupt;
      const size_t next = std::min(out.capacity() * 2, cfg::kMaxImageBytes);
      if (next <= out.capacity()) return FetchError::TooLarge;
      if (!out.grow(next)) return FetchError::OutOfMemory;
    }
    written += r.written;
    if (progress) progress(i + 1, meta.chunkCount);
  }
  out.setSize(written);
  return FetchError::None;
}

FetchError ImageStore::fetchImage(int slot, ImageBuffer& out, ProgressFn progress) {
  const String base = slotPath(slot);

  Meta meta;
  FetchError err = fetchMeta(base, meta);
  if (err == FetchError::None) err = fetchChunks(base, meta, out, progress);

  if (err == FetchError::None) {
    if (meta.version == 2) {
      if (out.size() != meta.size ||
          codec::crc32(out.data(), out.size()) != meta.crc32) {
        err = FetchError::Corrupt;
      }
    }
    if (err == FetchError::None && !codec::looksLikeJpeg(out.data(), out.size())) {
      err = FetchError::Corrupt;
    }
  }

  if (err != FetchError::None) out.release();
  return err;
}
