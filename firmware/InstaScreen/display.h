// TFT + JPEG rendering. Pin mapping lives in the TFT_eSPI setup
// (platformio.ini build flags, or firmware/tft_setup/ for Arduino IDE).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace display {

void begin();

// Full-screen status message. Used only when there is no image to keep on
// screen (boot, empty library) or for a fatal condition.
void showStatus(const char* title, const char* detail = nullptr);

// Thin progress bar along the bottom edge of a status screen. While a photo is
// on screen it is a no-op: the previous picture simply stays up until the
// next one has been downloaded and verified.
void drawProgress(uint16_t done, uint16_t total);
void clearProgress();

// Decodes and draws a verified JPEG, centred. Returns false if the decoder
// rejects it (e.g. progressive JPEG, which TJpgDec does not support).
bool drawJpeg(const uint8_t* data, size_t size);

}  // namespace display
