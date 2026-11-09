// Application orchestration: slideshow scheduling, WiFi lifecycle, retries.
#include "app.h"

#include <WiFi.h>

#include "backoff.h"
#include "config.h"
#include "display.h"
#include "image_store.h"
#include "rtdb_client.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your WiFi and Firebase settings."
#endif

// Define INSTASCREEN_INSECURE_TLS to skip certificate checks (debugging only).
#ifdef INSTASCREEN_INSECURE_TLS
constexpr bool kVerifyTls = false;
#else
constexpr bool kVerifyTls = true;
#endif

namespace {

RtdbClient rtdb(FIREBASE_HOST, FIREBASE_AUTH);
ImageStore store(rtdb);
Backoff backoff(cfg::kBackoffInitialMs, cfg::kBackoffMaxMs);

int currentSlot = 1;          // next slot to show (1-based)
uint32_t nextActionAt = 0;    // millis() timestamp of the next fetch
bool wasConnected = false;

bool due(uint32_t now) { return static_cast<int32_t>(now - nextActionAt) >= 0; }

void scheduleIn(uint32_t ms) { nextActionAt = millis() + ms; }

void scheduleRetry(const char* reason) {
  const uint32_t delayMs = backoff.fail();
  Serial.printf("[loop] %s; retry #%u in %u ms\n", reason, backoff.consecutiveFailures(), delayMs);
  scheduleIn(delayMs);
}

void connectWifiOrRestart() {
  display::showStatus("Connecting to WiFi", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > cfg::kWifiConnectTimeoutMs) {
      Serial.println(F("[wifi] connect timed out; restarting"));
      display::showStatus("WiFi unavailable", "Restarting...");
      delay(2000);
      ESP.restart();
    }
    delay(250);
  }
  wasConnected = true;
  Serial.printf("[wifi] connected, ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// Returns true when the station is connected. Reconnection itself is handled
// by the WiFi driver (auto-reconnect); we only drop the stale TLS session.
bool wifiReady() {
  const bool up = WiFi.status() == WL_CONNECTED;
  if (wasConnected && !up) {
    Serial.println(F("[wifi] link lost"));
    rtdb.reset();
  }
  wasConnected = up;
  return up;
}

void showNext() {
  // Reuse one TLS session within a slide, but start each slide fresh: the
  // server may have closed the idle socket during the slide interval, and a
  // half-closed connection would otherwise surface as a spurious failure.
  rtdb.reset();

  int count = 0;
  FetchError err = store.fetchImageCount(count);
  if (err != FetchError::None) {
    scheduleRetry(toString(err));
    return;
  }
  if (count == 0) {
    display::showStatus("No images yet", "Upload with: instascreen upload <file>");
    backoff.reset();
    scheduleIn(cfg::kEmptyLibraryPollMs);
    return;
  }
  if (currentSlot > count) currentSlot = 1;  // library shrank or we wrapped

  const uint32_t started = millis();
  ImageBuffer image;
  err = store.fetchImage(currentSlot, image, display::drawProgress);
  display::clearProgress();

  if (err == FetchError::None && !display::drawJpeg(image.data(), image.size())) {
    err = FetchError::Corrupt;  // decoder rejected it (e.g. progressive JPEG)
  }
  Serial.printf("[slide] slot %d/%d: %s (%u bytes, %u ms, free heap %u)\n", currentSlot, count,
                toString(err), static_cast<unsigned>(image.size()),
                static_cast<unsigned>(millis() - started), ESP.getFreeHeap());

  switch (err) {
    case FetchError::None:
      backoff.reset();
      currentSlot = currentSlot % count + 1;
      scheduleIn(cfg::kSlideIntervalMs);
      break;
    case FetchError::Network:
      scheduleRetry("network error");  // same slot again
      break;
    default:
      // The slot itself is bad (missing, corrupt, oversized). Skip it, but
      // still back off so a library of only-bad slots doesn't spin.
      currentSlot = currentSlot % count + 1;
      scheduleRetry("skipping bad slot");
      break;
  }
}

}  // namespace

namespace app {

void setup() {
  Serial.begin(115200);
  display::begin();
  connectWifiOrRestart();
  rtdb.begin(kVerifyTls);
  nextActionAt = millis();  // show the first image immediately
}

void loop() {
  if (!due(millis())) {
    delay(20);
    return;
  }
  if (!wifiReady()) {
    scheduleRetry("wifi down");
    return;
  }
  showNext();
}

}  // namespace app
