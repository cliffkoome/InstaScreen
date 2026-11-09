// Compile-time tunables. Credentials live in secrets.h, not here.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cfg {

// Slideshow
constexpr uint32_t kSlideIntervalMs = 15000;  // time each image stays on screen
constexpr uint32_t kEmptyLibraryPollMs = 30000;  // re-check cadence when no images exist

// Resilience
constexpr uint32_t kWifiConnectTimeoutMs = 20000;  // reboot if boot-time connect exceeds this
constexpr uint32_t kBackoffInitialMs = 2000;
constexpr uint32_t kBackoffMaxMs = 60000;

// HTTP
constexpr uint16_t kHttpConnectTimeoutMs = 8000;
constexpr uint16_t kHttpReadTimeoutMs = 8000;

// Memory budget. A 320x240 baseline JPEG at q85 is typically 15-35 KB; the
// uploader re-encodes at lower quality until it fits under this limit. The
// whole image must sit in one contiguous heap block because TJpgDec decodes
// from a memory buffer.
constexpr size_t kMaxImageBytes = 60 * 1024;
// Upper bound on one chunk's HTTP body (encoded). Guards against a malformed
// node pulling a huge String onto the heap.
constexpr size_t kMaxChunkBodyBytes = 8 * 1024;
// v1 (legacy hex) data has no size field; the original uploader used
// 1024-byte chunks, so this is the initial per-chunk allocation estimate.
constexpr size_t kV1ChunkBytesHint = 1024;

// Database layout (see README "Data model")
constexpr const char* kBasePath = "images";

}  // namespace cfg
