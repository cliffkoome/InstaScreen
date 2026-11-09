"""The firmware's native tests decode firmware/test/test_core/fixture.h.

This checks the same file from the Python side: it must be a valid v2 (and
v1) node by the uploader's own decoder. Together the two suites pin the wire
format from both ends.
"""

import re
from pathlib import Path

from instascreen_uploader.encode import decode_node

FIXTURE = Path(__file__).resolve().parents[2] / "firmware/test/test_core/fixture.h"


def _array(src: str, name: str) -> list[str]:
    body = re.search(rf"{name}\[\] = \{{(.*?)\}};", src, re.S).group(1)
    return re.findall(r'"\\"([^"\\]*)\\""', body)


def _const(src: str, name: str) -> int:
    return int(re.search(rf"{name} = (0x[0-9A-F]+|\d+)", src).group(1), 0)


def test_fixture_is_a_valid_node_for_both_formats():
    src = FIXTURE.read_text()
    meta = {
        "v": 2,
        "encoding": "base64",
        "size": _const(src, "kSize"),
        "crc32": _const(src, "kCrc32"),
        "chunk_count": _const(src, "kChunkCount"),
    }
    v2 = decode_node({"meta": meta, "chunks": _array(src, "kBase64Chunks")})
    hex_chunks = _array(src, "kHexChunks")
    v1 = decode_node({"chunk_count": len(hex_chunks), "chunks": hex_chunks})
    assert v1 == v2 and len(v2) == meta["size"]
