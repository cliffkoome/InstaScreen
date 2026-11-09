#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>
#include "SPI.h"
#include <TFT_eSPI.h>

// WiFi & Firebase Credentials
const char* ssid             = ""; // Your WiFi SSID
const char* password         = ""; // Your WiFi Password
const char* firebase_host    = ""; // e.g., "https://your-project-id.firebaseio.com/" (with trailing slash)
const char* base_image_path  = "images"; // Base path for images in Firebase, e.g., "images"

// Globals
TFT_eSPI tft = TFT_eSPI();
const unsigned long timerDelay = 15000;
unsigned long lastTime         = 0;

int totalImages        = 0;
int currentImageIndex  = 1;

// This buffer holds one chunk of the image at a time.
#define CHUNK_BUFFER_SIZE 2048
uint8_t chunkBuffer[CHUNK_BUFFER_SIZE];

// Callback for TJpg_Decoder to draw pixels to the screen
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;  // Continue decoding
}

// Parses a hex string ("FFD8...") into a byte buffer.
int parseHexChunk(const char* hexString, uint8_t* buffer, int maxLen) {
  int byteCount = 0;
  const char* p  = hexString;
  while (*p && byteCount < maxLen) {
    while (*p && !isxdigit(*p)) {
      p++;
    }
    if (*p) {
      char hex[3] = { *p, *(p + 1), 0 };
      buffer[byteCount++] = (uint8_t)strtol(hex, NULL, 16);
      if (*(p + 1)) p += 2; 
      else p++;
    }
  }
  return byteCount;
}

// Fetches and assembles the JPEG chunks for imageIndex, then draws it.
void displayImage(int imageIndex) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected, cannot display image.");
    return;
  }

  // 1) Get chunk_count
  HTTPClient http;
  char countUrl[256];
  snprintf(countUrl, sizeof(countUrl),
           "%s%s/image_%d/chunk_count.json",
           firebase_host, base_image_path, imageIndex);

  Serial.printf("Fetching chunk count from: %s\n", countUrl);
  WiFiClientSecure client;
  client.setInsecure();  // skip cert validation
  if (!http.begin(client, countUrl)) {
    Serial.println("http.begin() failed");
    return;
  }
  int httpCode = http.GET();
  int totalChunks = 0;
  if (httpCode == HTTP_CODE_OK) {
    totalChunks = http.getString().toInt();
    Serial.printf("Image %d has %d chunks.\n", imageIndex, totalChunks);
  } else {
    Serial.printf("Failed to fetch chunk_count (%d)\n", httpCode);
    http.end();
    return;
  }
  http.end();

  // 2) Download each chunk and assemble
  if (totalChunks > 0) {
    uint8_t* fullImageBuffer = NULL;
    size_t totalImageSize     = 0;

    tft.fillScreen(TFT_BLUE);
    tft.setCursor(10, 10);
    tft.printf("Downloading image %d...", imageIndex);

    for (int i = 0; i < totalChunks; i++) {
      char chunkUrl[256];
      snprintf(chunkUrl, sizeof(chunkUrl),
               "%s%s/image_%d/chunks/%d.json",
               firebase_host, base_image_path, imageIndex, i);

      if (!http.begin(client, chunkUrl)) {
        Serial.println("http.begin() failed");
        break;
      }
      httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        payload.replace("\"", "");
        int chunkSize = parseHexChunk(payload.c_str(), chunkBuffer, CHUNK_BUFFER_SIZE);

        uint8_t* tmp = (uint8_t*)realloc(fullImageBuffer, totalImageSize + chunkSize);
        if (tmp) {
          fullImageBuffer = tmp;
          memcpy(fullImageBuffer + totalImageSize, chunkBuffer, chunkSize);
          totalImageSize += chunkSize;
        } else {
          Serial.println("realloc failed!");
          free(fullImageBuffer);
          fullImageBuffer = NULL;
          break;
        }
      } else {
        Serial.printf("Chunk %d GET failed (%d)\n", i, httpCode);
        break;
      }
      http.end();
    }

    // 3) Decode & draw
    if (fullImageBuffer) {
      tft.fillScreen(TFT_BLACK);
      JRESULT res = TJpgDec.drawJpg(0, 0, fullImageBuffer, totalImageSize);
      if (res != JDR_OK) {
        Serial.printf("Decode Error: %d\n", res);
        tft.fillScreen(TFT_RED);
      }
      free(fullImageBuffer);
    }
  }
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // TJpg_Decoder init
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Fetch & display image #1 immediately
  displayImage(currentImageIndex);
  currentImageIndex++;
  lastTime = millis() - timerDelay;
}

void loop() {
  // Only swap images once DISPLAY_TIME has elapsed since last draw
  if (millis() - lastTime >= timerDelay) {
    // 1) Update totalImages count from Firebase
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    char countUrl[256];
    snprintf(countUrl, sizeof(countUrl),
             "%s%s/image_count.json",
             firebase_host, base_image_path);

    if (http.begin(client, countUrl)) {
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        totalImages = http.getString().toInt();
        Serial.printf("Found %d total images.\n", totalImages);
      }
      http.end();
    }

    // 2) Display next image (if any)
    if (totalImages > 0) {
      displayImage(currentImageIndex);

      // 3) Record finish time & advance index
      lastTime = millis();
      currentImageIndex++;
      if (currentImageIndex > totalImages) {
        currentImageIndex = 1;
      }
    }
  }
}
