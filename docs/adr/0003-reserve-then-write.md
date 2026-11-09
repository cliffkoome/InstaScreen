# ADR-0003: Reserve-then-write for playlist appends

- **Status:** Accepted

## Context

The playlist is the slot range `1..image_count`. v1 did an unguarded read-modify-write of `image_count`, so two uploads at once could pick the same slot and one image would silently overwrite the other.

There is no server to serialise writes. RTDB's REST API offers ETag-conditional PUTs (`X-Firebase-ETag` / `if-match`, with 412 on conflict).

## Options

1. **Write-then-publish.** Write `image_{count+1}`, then compare-and-set `count → count+1`. If two writers both target the same slot, one CAS wins, but the loser has already overwritten the winner's image.
2. **Reserve-then-write.** Compare-and-set `count → count+1` first. The winner of each CAS owns that slot exclusively, then writes it with a single atomic PUT.

## Decision

Use reserve-then-write. `image_count` is only ever modified through `rtdb.transaction`, a bounded CAS retry loop.

## Consequences

- **+** Concurrent appends always receive distinct slots. This is covered by tests against an in-memory RTDB with real ETag semantics.
- **−** Between reserving and writing, the device can observe a slot that doesn't exist yet. The firmware already treats a missing or invalid slot as "skip and back off", so the worst case is one skipped slide.
- **Deletes** compact the playlist by moving the last slot into the hole. That multi-step operation assumes a single writer, which is acceptable for a personal frame and documented in `library.remove`.
