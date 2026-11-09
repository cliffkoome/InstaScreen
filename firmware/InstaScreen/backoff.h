// Capped exponential backoff. Header-only and platform-independent so it can
// be unit-tested on the host.
#pragma once

#include <stdint.h>

class Backoff {
 public:
  constexpr Backoff(uint32_t initialMs, uint32_t maxMs)
      : initialMs_(initialMs), maxMs_(maxMs), currentMs_(initialMs) {}

  // Returns the delay to wait after a failure, then doubles it (up to max).
  uint32_t fail() {
    const uint32_t delay = currentMs_;
    currentMs_ = (currentMs_ > maxMs_ / 2) ? maxMs_ : currentMs_ * 2;
    ++failures_;
    return delay;
  }

  void reset() {
    currentMs_ = initialMs_;
    failures_ = 0;
  }

  uint32_t consecutiveFailures() const { return failures_; }

 private:
  uint32_t initialMs_;
  uint32_t maxMs_;
  uint32_t currentMs_;
  uint32_t failures_ = 0;
};
