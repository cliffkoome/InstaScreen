"""Image preparation and the InstaScreen wire format.

The device decodes with TJpgDec from a single contiguous RAM buffer, which
puts three hard constraints on what we publish:

* baseline (non-progressive) JPEG only - TJpgDec cannot decode progressive;
* exactly the panel resolution, so the device never scales;
* a byte budget (``MAX_IMAGE_BYTES``) that matches ``cfg::kMaxImageBytes`` in
  ``firmware/InstaScreen/config.h``.
"""

from __future__ import annotations

import base64
import io
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, BinaryIO

from PIL import Image, ImageOps

FORMAT_VERSION = 2
PANEL_SIZE = (320, 240)
MAX_IMAGE_BYTES = 60 * 1024  # keep in sync with cfg::kMaxImageBytes
DEFAULT_CHUNK_BYTES = 4096  # raw bytes per chunk; base64 body ~5.5 KB < kMaxChunkBodyBytes
MAX_CHUNK_BYTES = 6 * 1024  # base64 of this stays under the firmware's 8 KB body limit
MIN_QUALITY = 40


class ImageTooLargeError(ValueError):
    """The image cannot be compressed under the device's byte budget."""


@dataclass(frozen=True)
class EncodedImage:
    """A JPEG split into wire-format chunks plus its integrity metadata."""

    jpeg: bytes
    chunks: list[str]
    crc32: int

    @property
    def meta(self) -> dict[str, Any]:
        return {
            "v": FORMAT_VERSION,
            "encoding": "base64",
            "size": len(self.jpeg),
            "crc32": self.crc32,
            "chunk_count": len(self.chunks),
        }

    def to_node(self) -> dict[str, Any]:
        """The JSON written to ``images/image_N`` in one atomic PUT."""
        return {"meta": self.meta, "chunks": self.chunks}


def prepare_jpeg(
    source: str | Path | BinaryIO,
    *,
    size: tuple[int, int] = PANEL_SIZE,
    quality: int = 85,
    max_bytes: int = MAX_IMAGE_BYTES,
) -> bytes:
    """Return a baseline JPEG of exactly ``size`` that fits in ``max_bytes``.

    Honours EXIF orientation, flattens alpha/palette images onto RGB, and
    centre-crops to the panel aspect ratio instead of stretching. Quality is
    stepped down until the result fits the budget.
    """
    with Image.open(source) as img:
        img = ImageOps.exif_transpose(img)
        if img.mode != "RGB":
            img = _to_rgb(img)
        img = ImageOps.fit(img, size, Image.Resampling.LANCZOS)

        q = quality
        while True:
            data = _save_baseline(img, q)
            if len(data) <= max_bytes:
                return data
            if q <= MIN_QUALITY:
                raise ImageTooLargeError(
                    f"{len(data)} bytes at quality {q} exceeds the {max_bytes}-byte device budget"
                )
            q = max(MIN_QUALITY, q - 10)


def encode(jpeg: bytes, chunk_bytes: int = DEFAULT_CHUNK_BYTES) -> EncodedImage:
    """Split a JPEG into base64 chunks (wire format v2)."""
    if not 1 <= chunk_bytes <= MAX_CHUNK_BYTES:
        raise ValueError(f"chunk_bytes must be in 1..{MAX_CHUNK_BYTES}")
    if not is_jpeg(jpeg):
        raise ValueError("not a JPEG (missing SOI/EOI markers)")
    chunks = [
        base64.b64encode(jpeg[i : i + chunk_bytes]).decode("ascii")
        for i in range(0, len(jpeg), chunk_bytes)
    ]
    return EncodedImage(jpeg=jpeg, chunks=chunks, crc32=zlib.crc32(jpeg))


def decode_node(node: dict[str, Any]) -> bytes:
    """Reassemble and verify an ``images/image_N`` node (v1 or v2).

    Mirrors ``ImageStore::fetchImage`` in the firmware; raises ``ValueError``
    on anything the device would reject.
    """
    meta = node.get("meta")
    chunks = node.get("chunks") or []
    if meta:
        if meta.get("v") != 2 or meta.get("encoding") != "base64":
            raise ValueError(f"unsupported meta: {meta}")
        if len(chunks) != meta["chunk_count"]:
            raise ValueError("chunk count mismatch")
        data = b"".join(base64.b64decode(c) for c in chunks)
        if len(data) != meta["size"] or zlib.crc32(data) != meta["crc32"]:
            raise ValueError("size/CRC mismatch")
    elif "chunk_count" in node:  # v1: hex chunks, no checksum
        if len(chunks) != node["chunk_count"]:
            raise ValueError("chunk count mismatch")
        data = b"".join(bytes.fromhex(c) for c in chunks)
    else:
        raise ValueError("node has neither v2 meta nor v1 chunk_count")
    if not is_jpeg(data):
        raise ValueError("payload is not a complete JPEG")
    return data


def node_version(node: dict[str, Any] | None) -> int | None:
    if not node:
        return None
    if "meta" in node:
        return int(node["meta"].get("v", 0))
    return 1 if "chunk_count" in node else 0


def is_jpeg(data: bytes) -> bool:
    return len(data) >= 4 and data[:2] == b"\xff\xd8" and data[-2:] == b"\xff\xd9"


def _to_rgb(img: Image.Image) -> Image.Image:
    if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
        rgba = img.convert("RGBA")
        bg = Image.new("RGB", rgba.size, (0, 0, 0))
        bg.paste(rgba, mask=rgba.getchannel("A"))
        return bg
    return img.convert("RGB")


def _save_baseline(img: Image.Image, quality: int) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=quality, optimize=True, progressive=False)
    return buf.getvalue()
