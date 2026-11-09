import os
import requests
import json
from dotenv import load_dotenv
from PIL import Image
import io

# --- Configuration ---
load_dotenv()
FIREBASE_URL = os.environ.get("FIREBASE_URL") # e.g., "https://your-project.firebaseio.com/" (with trailing slash)
# Ensure your database rules allow writes or use authentication.
BASE_IMAGE_PATH = "images"  # The base path for all images
LOCAL_IMAGE_FILE = "images/bmw.jpg"  # The local image you want to upload
IMAGE_NUMBER = 1  # <--- SET THIS to 1, 2, 3, etc. for each image you upload.
CHUNK_SIZE = 1024  # Size of each chunk in bytes. 1024 bytes = 1 KB.
OUTPUT_WIDTH = 320
OUTPUT_HEIGHT = 240


def resize_chunk_and_upload():
    """
    Resizes a local JPEG, splits it into chunks, and uploads them to a numbered
    slot in Firebase RTDB. Also updates the total image count.
    """
    try:
        # Open and resize the image
        with Image.open(LOCAL_IMAGE_FILE) as img:
            resized_img = img.resize((OUTPUT_WIDTH, OUTPUT_HEIGHT), Image.Resampling.LANCZOS)

            # Save the resized image to an in-memory buffer
            buffer = io.BytesIO()
            resized_img.save(buffer, format='JPEG', quality=90)
            image_data = buffer.getvalue()

    except FileNotFoundError:
        print(f"Error: Image file not found at '{LOCAL_IMAGE_FILE}'")
        return
    except Exception as e:
        print(f"An error occurred during image processing: {e}")
        return

    # Convert the raw byte data to a string of hex values
    hex_string = image_data.hex().upper()

    # Split the hex string into chunks
    hex_chunks = []
    # Each byte becomes two hex characters, so we chunk by CHUNK_SIZE * 2
    for i in range(0, len(hex_string), CHUNK_SIZE * 2):
        hex_chunks.append(hex_string[i:i + CHUNK_SIZE * 2])

    chunk_count = len(hex_chunks)
    print(f"Resized image to {OUTPUT_WIDTH}x{OUTPUT_HEIGHT} and split into {chunk_count} chunks.")

    # Prepare the data object for Firebase
    firebase_image_data = {
        "chunk_count": chunk_count,
        "chunks": hex_chunks
    }

    # The full URL for the PUT request for this specific image
    image_slot_path = f"{BASE_IMAGE_PATH}/image_{IMAGE_NUMBER}"
    full_image_url = f"{FIREBASE_URL}{image_slot_path}.json"

    print(f"Uploading image {IMAGE_NUMBER} to {full_image_url} ...")

    try:
        # Upload the image chunks
        response = requests.put(full_image_url, data=json.dumps(firebase_image_data))
        response.raise_for_status()
        print("Image upload successful!")

        # After uploading the image, update the total image count
        count_url = f"{FIREBASE_URL}{BASE_IMAGE_PATH}/image_count.json"

        # First, get the current count to see if we need to increase it
        current_count_resp = requests.get(count_url)
        current_count = 0
        if current_count_resp.status_code == 200 and current_count_resp.text != 'null':
            current_count = int(current_count_resp.text)

        # Only update the count if the new image number is higher
        if IMAGE_NUMBER > current_count:
            print(f"Updating image_count to {IMAGE_NUMBER}")
            count_response = requests.put(count_url, data=json.dumps(IMAGE_NUMBER))
            count_response.raise_for_status()
            print("image_count updated successfully.")
        else:
            print("image_count is already sufficient.")

    except requests.exceptions.RequestException as e:
        print(f"An error occurred during upload: {e}")


if __name__ == "__main__":
    resize_chunk_and_upload()
