# ADR-0002: Wire format v2 (base64 chunks + size + CRC32)

- **Status:** Accepted. Supersedes the implicit v1 format.

## Context

v1 stored `chunk_count` and an array of 1 KB **hex** chunks. In practice it had these problems:

- 2× size overhead on the wire and in storage;
- no total size, so the device grew its buffer with `realloc` on every chunk;
- no checksum, so a truncated or half-overwritten node was only discovered when the JPEG decoder failed, and a partial image could reach the screen;
- no version marker, so the format could not evolve safely.

## Decision

```
images/image_N/meta   = { v: 2, encoding: "base64", size, crc32, chunk_count }
images/image_N/chunks = [ base64(≤ 4096 raw bytes), ... ]
```

- **Versioned.** The device reads `meta.v`. If `meta` is absent, it falls back to v1, so databases written by the old uploader keep working without coordinated upgrades.
- **Sized.** The device allocates exactly `size` bytes once and decodes each chunk in place.
- **Verified.** Before the screen is touched, the device checks the size, CRC-32 (IEEE, equal to Python's `zlib.crc32`) and the JPEG SOI/EOI markers.
- **4 KB chunks.** The base64 body (~5.5 KB) stays well under the firmware's 8 KB per-response limit, while a typical 10–30 KB image needs only 3–8 requests.
- **Migration.** `instascreen migrate` re-encodes v1 slots as v2 byte-for-byte, with no re-compression.

## Consequences

- **+** ~33 % less data than v1. One allocation, no fragmentation from repeated `realloc`.
- **+** Partial or corrupt images are rejected deterministically.
- **−** Two decoders live in the firmware until v1 data is gone.
- **Contract test:** the Python encoder generates `firmware/test/test_core/fixture.h`. The firmware's native tests decode it, and the Python tests validate the same file. A change that breaks either side fails CI.
