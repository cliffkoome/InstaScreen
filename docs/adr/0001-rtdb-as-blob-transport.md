# ADR-0001: Firebase Realtime Database as the image transport

- **Status:** Accepted
- **Context date:** 2025-07 (v1); revisited 2026-11 (v2)

## Context

The frame has to fetch images published from anywhere, at zero running cost and with no server to operate. The ESP32 has ~320 KB of SRAM and no filesystem we want to depend on.

Options:

1. **Firebase Storage.** This is the natural home for blobs, and a JPEG could be streamed straight into the decoder. However, new projects need the paid Blaze plan to use it.
2. **A self-hosted HTTP server.** This gives full control, but means owning uptime, TLS certificates and a public endpoint.
3. **An MQTT broker.** Retained messages would give push semantics, but brokers cap payload sizes and free tiers are small, so the chunking problem stays the same.
4. **Firebase Realtime Database.** It's free on Spark, offers managed HTTPS, has a trivial REST API and supports atomic node writes and ETag compare-and-set. However, it stores JSON, not binary.

## Decision

Use RTDB. Store each image as a node of text-encoded chunks with a small metadata record, using the [wire format](0002-wire-format-v2.md).

## Consequences

- **+** No infrastructure. The device and the CLI talk to a managed endpoint over standard HTTPS.
- **+** RTDB's atomic writes and ETags are enough to build a consistent playlist without a backend ([ADR-0003](0003-reserve-then-write.md)).
- **−** Binary-to-text overhead (×1.33 with base64) on the wire and in the 1 GB Spark storage quota.
- **−** This is not a blob store. It's fine for one frame and a modest playlist, but a fleet should move image bytes to object storage and keep RTDB for the manifest.
