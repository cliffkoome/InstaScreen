// InstaScreen: cloud-synced ESP32 photo frame.
//
// Runtime loop (non-blocking between actions):
//   wait until `nextActionAt`
//   -> ensure WiFi
//   -> read image_count, pick the next slot
//   -> download + verify into RAM (the old image stays on screen meanwhile)
//   -> draw, then schedule the next slide
// Any failure schedules a retry with capped exponential backoff instead of
// spinning on the network.

#include "app.h"

void setup() { app::setup(); }
void loop() { app::loop(); }
