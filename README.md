# InstaScreen - Image to Firebase Chunker

This component of the **InstaScreen** project handles the process of preparing and uploading images to Firebase Realtime Database in a format optimized for low-memory microcontrollers like the **ESP32**.

---

## 📦 What It Does

The **image-to-firebase-chunker**:
- Resizes a local JPEG image to fit a 320x240 screen (configurable).
- Converts the image to raw JPEG bytes.
- Encodes the image data into a hex string.
- Splits the hex string into manageable chunks (e.g., 1024 bytes per chunk).
- Uploads the chunked data to a Firebase Realtime Database using REST API.
- Automatically updates the `image_count` node to reflect the latest image index.

---

## 🛠️ Technologies Used

- **Python 3**
- [Pillow](https://pillow.readthedocs.io/en/stable/) – for image processing
- [Requests](https://docs.python-requests.org/) – for HTTP requests to Firebase
- [python-dotenv](https://pypi.org/project/python-dotenv/) – for managing Firebase secrets

---

## 📁 Folder Structure

```plaintext
image-to-firebase-chunker/
├── chunker.py          # Main script to resize, chunk, and upload image
├── images/             # Folder containing input images
├── .env                # Environment file (not committed)
└── .gitignore          # File to exclude venv, images, etc.
```

---

## ✅ Prerequisites

- Python 3.7+
- A Firebase Realtime Database (public rules or authenticated access)
- A valid `.env` file with your Firebase URL

---

## 🔐 .env File

Create a `.env` file inside the `image-to-firebase-chunker/` folder:

```env
FIREBASE_URL=https://your-project-id.firebaseio.com/
```

Make sure your Firebase rules allow writing to the database.

---

## 📸 How to Use

1. **Install dependencies**:

```bash
pip install pillow requests python-dotenv
```

2. **Place your image** (e.g., `nfs-heat.jpg`) inside the `images/` folder.

3. **Edit config values** in `chunker.py`:
   - `LOCAL_IMAGE_FILE = "images/nfs-heat.jpg"`
   - `IMAGE_NUMBER = 1` *(change this to 2, 3, etc. for subsequent uploads)*

4. **Run the script**:

```bash
python chunker.py
```

5. **Result**:
   - Image will be resized to `320x240`
   - Uploaded in chunks to:
     ```
     /images/image_1/
         ├── chunk_count: 5
         └── chunks: [hex_chunk_0, hex_chunk_1, ..., hex_chunk_n]
     ```
   - `image_count` will be updated in:
     ```
     /images/image_count: 1
     ```

---

## 📲 Why Chunking?

The ESP32 has limited memory. Sending or downloading large image strings in one go can cause memory issues or crashes. Chunking the image makes it possible to:
- Upload large images to Firebase
- Later download and reconstruct them piece by piece on the ESP32

---

## 🧹 To Do

- [ ] Add support for automatic image numbering
- [ ] Add CLI options for resizing and quality settings
- [ ] ESP32 companion code for reassembling and displaying the image

---

## 📜 License

MIT License — see [LICENSE](../LICENSE) for details.

---

## 🙌 Credits

Made by Cliff Koome for the InstaScreen project.
