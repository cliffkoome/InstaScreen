# InstaScreen - ESP32 Firebase Image Viewer

This project demonstrates how to send large JPEG images to **Firebase Realtime Database** in chunks from a computer, and later retrieve, reconstruct, and display them on a **TFT display connected to an ESP32** using the `TJpg_Decoder` library.

---

## 📚 Table of Contents

1. [Arduino Implementation](#arduino-implementation)
2. [Image Chunker](#image-chunker)

---

## 1. Arduino Implementation

The Arduino component runs on an ESP32 microcontroller and performs the following:

- Connects to Wi-Fi and Firebase Realtime Database
- Fetches the number of image chunks for the current image
- Downloads and reassembles JPEG image chunks from Firebase
- Decodes the JPEG image using `TJpg_Decoder`
- Displays the image on a TFT display using `TFT_eSPI`

### 🔌 Requirements

- ESP32 board (e.g., NodeMCU-32S)
- 320x240 TFT screen (e.g., ILI9341)
- Arduino libraries:
  - `WiFi.h`, `WiFiClientSecure.h`
  - `HTTPClient.h`
  - `ArduinoJson.h`
  - `TJpg_Decoder`
  - `TFT_eSPI`

### 🔧 Configuration

Wiring configuration can be found at:

```
Arduino/
└── Schematic/
    └── schematic-diagram.png
```

For more detailed information about the ESP32 to UNO TFT Shield wiring and setup, refer to:

**Source**: [ESP32 WROOM-32 and UNO Shield Parallel TFT Displays](https://thesolaruniverse.wordpress.com/2021/06/01/esp32-wroom-32-and-uno-shield-parallel-tft-displays/)  
*By Floris Wouterlood – The Netherlands – June 1, 2021 –*


In your Arduino sketch, update the following variables:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* firebase_host = "https://your-project-id.firebaseio.com/";
```

Make sure your Firebase project allows read access or uses authentication appropriately.

### 🖼️ How it Works

1. ESP32 fetches `/images/image_COUNT/chunk_count` to determine how many chunks exist.
2. It iteratively downloads `/images/image_COUNT/chunks/N` from Firebase.
3. All chunks are joined into a single buffer.
4. The JPEG buffer is passed to `TJpg_Decoder` for rendering on the TFT display.

---

## 2. Image Chunker

This is a Python script that prepares and uploads images from your computer to Firebase in a chunked format suitable for ESP32 consumption.

### 📦 Features

- Resizes images to 320x240 (configurable)
- Converts them to JPEG
- Encodes JPEG to hex
- Splits hex string into chunks (default 1024 bytes)
- Uploads to Firebase and updates image count

### 🛠️ Technologies

- Python 3
- `Pillow` for image processing
- `Requests` for Firebase upload
- `python-dotenv` for credentials

### 📁 Project Structure

```plaintext
image-to-firebase-chunker/
├── chunker.py
├── images/
├── .env
└── .gitignore
```

### 🔐 .env Setup

Create a `.env` file with the following:

```env
FIREBASE_URL=https://your-project-id.firebaseio.com/
```

### 🚀 How to Use

1. Install dependencies:

```bash
pip install pillow requests python-dotenv
```

2. Add an image to the `images/` folder (e.g. `nfs-heat.jpg`)
3. Edit the script’s `IMAGE_NUMBER` and `LOCAL_IMAGE_FILE`
4. Run:

```bash
python chunker.py
```

The image will be resized, chunked, and uploaded to:

```
/images/image_1/
    ├── chunk_count: 5
    └── chunks: [0, 1, ..., n]
```

Firebase will also have `/images/image_count` set accordingly.

---

## 📲 Why Chunking?

ESP32 devices have limited RAM. Chunking allows large images to be handled in pieces, preventing memory overflow and enabling smooth display from the cloud.

---

## 📜 License

MIT License — see LICENSE for details.

---

## 🙌 Credits

Made by Cliff Koome for the InstaScreen project.
