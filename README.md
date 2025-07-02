# InstaScreen – ESP32 TFT Display with Firebase Realtime Database

**A complete guide to connecting a TFT LCD (ILI9341) to ESP32 and displaying images stored in Firebase RTDB.**

This project demonstrates how to:
- Interface an **ESP32** with a **320x240 TFT LCD display** (e.g., ILI9341) using **TFT_eSPI**
- Upload JPEG images from a PC to **Firebase Realtime Database** in small memory-safe chunks
- Download, reassemble, and render the images on an **ESP32 display** using `TJpg_Decoder`

Perfect for hobbyists and developers searching for:
- "How to connect TFT display to ESP32"
- "ESP32 ILI9341 image display"
- "Stream JPEG to ESP32 without Firebase Storage"
- "ESP32 Firebase Realtime Database image viewer"

Supports:
- ESP32 DevKit / NodeMCU-32S
- ILI9341 TFT displays (and compatible models)
- Firebase RTDB (no billing required)

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

- ESP32 board (e.g., ESP32-Wroom-32)
- 320x240 TFT screen (e.g., ILI9341)
- Arduino libraries:
  - `WiFi.h`, `WiFiClientSecure.h`
  - `HTTPClient.h`
  - `ArduinoJson.h` by Benoit Blanchon. [ArduinoJson-Github](https://github.com/bblanchon/ArduinoJson.git)
  - `TJpg_Decoder` by Bodmer. [TJpg_Decoder-Github](https://github.com/Bodmer/TJpg_Decoder.git)
  - `TFT_eSPI` by Bodmer. [TFT_eSPI-Github](https://github.com/Bodmer/TFT_eSPI.git)

- You can add the libraries through Arduino IDE Library Manager.
- To install an Arduino library from GitHub **(Recommended)**, download it as a ZIP file and then navigate to Sketch > Include Library > Add .zip Library in the Arduino IDE, and select the downloaded ZIP file.

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

### Arduino Sketch

Find and copy the arduino sketch provided at: 

```
Arduino/
└── InstaScreen.ino
```

In your Arduino sketch, update the following variables:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* firebase_host = "https://your-project-id.firebaseio.com/"; //This is the Firebase RTDB Url
```

Make sure your Firebase project allows read access or uses authentication appropriately.

### 🖼️ How it Works

1. ESP32 fetches `/images/image_COUNT/chunk_count` to determine how many chunks exist.
2. It iteratively downloads `/images/image_COUNT/chunks/N` from Firebase.
3. All chunks are joined into a single buffer.
4. The JPEG buffer is passed to `TJpg_Decoder` for rendering on the TFT display.

<div align=center>
  <img src="https://github.com/user-attachments/assets/330485a7-d288-4071-90ea-a8c359332277" alt="Example output" width="400" height="350">
</div>

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

## 💡 Why Use Firebase Realtime Database Instead of Firebase Storage?

While Firebase Storage supports direct JPEG uploads and streaming with `TJpg_Decoder`, it now requires billing for most usage.

**Firebase Realtime Database (RTDB)** remains **free** under the Spark plan and allows:
- Storing image data as hex chunks
- Downloading and reconstructing on ESP32

This approach avoids storage fees and is ideal for students, makers, and prototypes.

| Feature                | Firebase Storage | Firebase RTDB      |
|------------------------|------------------|---------------------|
| Direct JPEG support    | ✅ Yes           | ❌ No               |
| Free under Spark plan  | ❌ No            | ✅ Yes              |
| `TJpg_Decoder` support | ✅ Streamable    | ✅ After chunk join |

---

## 📲 Why Chunking?

ESP32 devices have limited RAM. Chunking allows large images to be handled in pieces, preventing memory overflow and enabling smooth display from the cloud.

---

## 📜 License

MIT License — see LICENSE for details.

---

## 🙌 Credits

Floris Wouterlood for providing Wiring configuration for esp32 and TFT LCD SHIELD display.
