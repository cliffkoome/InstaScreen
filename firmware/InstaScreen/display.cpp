#include "display.h"

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

namespace display {
namespace {

TFT_eSPI tft;
constexpr int16_t kProgressHeight = 3;
bool hasImage = false;

bool onBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;  // stop decoding below the screen
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

}  // namespace

void begin() {
  tft.begin();
  tft.setRotation(3);  // landscape, 320x240
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(onBlock);
}

void showStatus(const char* title, const char* detail) {
  hasImage = false;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title, tft.width() / 2, tft.height() / 2 - 10, 2);
  if (detail) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(detail, tft.width() / 2, tft.height() / 2 + 12, 1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  tft.setTextDatum(TL_DATUM);
}

void drawProgress(uint16_t done, uint16_t total) {
  // Over a photo, stay invisible: the old image remains until the new one is
  // verified, and a bar left behind by a failed download would look like a glitch.
  if (total == 0 || hasImage) return;
  const int16_t y = tft.height() - kProgressHeight;
  const int32_t w = static_cast<int32_t>(tft.width()) * done / total;
  tft.fillRect(0, y, w, kProgressHeight, TFT_CYAN);
}

void clearProgress() {
  if (!hasImage) tft.fillRect(0, tft.height() - kProgressHeight, tft.width(), kProgressHeight, TFT_BLACK);
}

bool drawJpeg(const uint8_t* data, size_t size) {
  uint16_t w = 0, h = 0;
  if (TJpgDec.getJpgSize(&w, &h, data, size) != JDR_OK) return false;

  const int16_t x = w < tft.width() ? (tft.width() - w) / 2 : 0;
  const int16_t y = h < tft.height() ? (tft.height() - h) / 2 : 0;
  if (w < tft.width() || h < tft.height()) tft.fillScreen(TFT_BLACK);

  const bool ok = TJpgDec.drawJpg(x, y, data, size) == JDR_OK;
  hasImage = ok;
  return ok;
}

}  // namespace display
