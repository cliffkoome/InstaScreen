// Minimal Firebase Realtime Database REST client.
//
// Owns a single TLS connection and reuses it (HTTP keep-alive) across
// requests, so downloading an N-chunk image costs one TLS handshake instead of
// N. Handshakes dominate latency on the ESP32 (~0.5-1.5 s each).
#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

class RtdbClient {
 public:
  enum class Status {
    Ok,         // 200 with a non-null body
    Null,       // 200 with body "null": the path does not exist
    TooLarge,   // Content-Length exceeds the caller's limit
    HttpError,  // non-200 status (rules denied, bad URL, server error)
    Transport,  // connect/TLS/read failure
  };

  RtdbClient(const char* host, const char* auth);

  // Configures TLS. With verifyTls=false the server certificate is NOT
  // checked; only use that for debugging.
  void begin(bool verifyTls);

  // GET <host>/<path>.json and store the raw JSON body in `body`.
  Status get(const String& path, String& body, size_t maxBodyBytes);

  // Drops the pooled connection (e.g. after a WiFi drop).
  void reset();

  int lastHttpCode() const { return lastHttpCode_; }

 private:
  String url(const String& path) const;

  WiFiClientSecure tls_;
  HTTPClient http_;
  String host_;
  String auth_;
  int lastHttpCode_ = 0;
};

const char* toString(RtdbClient::Status s);
