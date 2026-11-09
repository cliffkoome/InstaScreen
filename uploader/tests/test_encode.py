import base64
import io
import zlib

import pytest
from conftest import make_image
from PIL import Image

from instascreen_uploader.encode import (
    PANEL_SIZE,
    ImageTooLargeError,
    decode_node,
    encode,
    prepare_jpeg,
)


def _open(jpeg: bytes) -> Image.Image:
    return Image.open(io.BytesIO(jpeg))


@pytest.mark.parametrize("mode", ["RGB", "RGBA", "L", "P"])
def test_prepare_produces_panel_sized_baseline_jpeg(mode):
    jpeg = prepare_jpeg(make_image(mode))
    img = _open(jpeg)
    assert img.format == "JPEG"
    assert img.size == PANEL_SIZE
    assert not img.info.get("progressive") and not img.info.get("progression")


def _is(px, rgb, tol=40):
    return all(abs(a - b) <= tol for a, b in zip(px, rgb, strict=True))


def test_prepare_crops_instead_of_stretching():
    # 640x240, left quarter red. Stretching would keep red at x=0; a centre
    # crop (160..480) removes it entirely.
    src = Image.new("RGB", (640, 240), (0, 0, 255))
    src.paste((255, 0, 0), (0, 0, 160, 240))
    buf = io.BytesIO()
    src.save(buf, format="PNG")
    buf.seek(0)
    out = _open(prepare_jpeg(buf)).convert("RGB")
    assert out.size == PANEL_SIZE
    assert _is(out.getpixel((5, 120)), (0, 0, 255))


def test_prepare_honours_exif_orientation():
    # Stored portrait 100x300, top third red, with Orientation=6 (display
    # rotated 90 deg CW): the red band must end up on the right edge.
    img = Image.new("RGB", (100, 300), (0, 255, 0))
    img.paste((255, 0, 0), (0, 0, 100, 100))
    exif = Image.Exif()
    exif[0x0112] = 6
    buf = io.BytesIO()
    img.save(buf, format="JPEG", exif=exif.tobytes(), quality=95)
    buf.seek(0)
    out = _open(prepare_jpeg(buf)).convert("RGB")
    assert _is(out.getpixel((315, 120)), (255, 0, 0))
    assert _is(out.getpixel((5, 120)), (0, 255, 0))


def test_prepare_steps_quality_down_to_fit_budget():
    noisy = Image.effect_noise(PANEL_SIZE, 100).convert("RGB")
    buf = io.BytesIO()
    noisy.save(buf, format="PNG")
    buf.seek(0)
    high = len(prepare_jpeg(io.BytesIO(buf.getvalue()), max_bytes=10**7, quality=95))
    budget = high * 2 // 3
    assert len(prepare_jpeg(io.BytesIO(buf.getvalue()), max_bytes=budget, quality=95)) <= budget


def test_prepare_raises_when_budget_unreachable():
    with pytest.raises(ImageTooLargeError):
        prepare_jpeg(make_image(), max_bytes=500)


@pytest.mark.parametrize("chunk", [1, 3, 1000, 4096, 6144])
def test_encode_roundtrip(chunk):
    jpeg = prepare_jpeg(make_image())
    enc = encode(jpeg, chunk)
    assert enc.meta["size"] == len(jpeg)
    assert enc.meta["crc32"] == zlib.crc32(jpeg)
    assert enc.meta["chunk_count"] == -(-len(jpeg) // chunk)
    assert decode_node(enc.to_node()) == jpeg


def test_crc_matches_firmware_check_value():
    # Same vector as firmware/test/test_core (codec::crc32).
    assert zlib.crc32(b"123456789") == 0xCBF43926


def test_decode_detects_corruption():
    enc = encode(prepare_jpeg(make_image()), 1024)
    node = enc.to_node()
    raw = bytearray(base64.b64decode(node["chunks"][0]))
    raw[10] ^= 0xFF
    node["chunks"][0] = base64.b64encode(bytes(raw)).decode()
    with pytest.raises(ValueError, match="CRC"):
        decode_node(node)


def test_decode_reads_legacy_v1_hex():
    jpeg = prepare_jpeg(make_image())
    hexs = jpeg.hex().upper()
    chunks = [hexs[i : i + 2048] for i in range(0, len(hexs), 2048)]
    assert decode_node({"chunk_count": len(chunks), "chunks": chunks}) == jpeg


def test_encode_rejects_non_jpeg():
    with pytest.raises(ValueError):
        encode(b"not a jpeg")
