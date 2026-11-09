from __future__ import annotations

import copy
import io
import itertools
from typing import Any

import pytest
from PIL import Image


class FakeStore:
    """In-memory RTDB with path addressing and ETag compare-and-set."""

    def __init__(self) -> None:
        self.root: dict[str, Any] = {}
        self._versions = itertools.count(1)
        self.etags: dict[str, str] = {}
        self.conflicts_to_inject = 0
        self.writes: list[tuple[str, str]] = []

    def _parts(self, path: str) -> list[str]:
        return [p for p in path.strip("/").split("/") if p]

    def get(self, path: str, *, shallow: bool = False) -> Any:
        node: Any = self.root
        for part in self._parts(path):
            if isinstance(node, list):
                idx = int(part)
                node = node[idx] if idx < len(node) else None
            elif isinstance(node, dict):
                node = node.get(part)
            else:
                return None
            if node is None:
                return None
        return copy.deepcopy(node)

    def put(self, path: str, value: Any) -> None:
        *parents, leaf = self._parts(path)
        node = self.root
        for part in parents:
            node = node.setdefault(part, {})
        node[leaf] = copy.deepcopy(value)
        self.etags[path] = f"e{next(self._versions)}"
        self.writes.append(("put", path))

    def delete(self, path: str) -> None:
        *parents, leaf = self._parts(path)
        node = self.root
        for part in parents:
            node = node.get(part, {})
        node.pop(leaf, None)
        self.etags.pop(path, None)
        self.writes.append(("delete", path))

    def get_with_etag(self, path: str) -> tuple[Any, str]:
        return self.get(path), self.etags.get(path, "NULL_ETAG")

    def put_if_match(self, path: str, value: Any, etag: str) -> tuple[bool, Any, str]:
        if self.conflicts_to_inject:  # simulate a concurrent writer bumping the value
            self.conflicts_to_inject -= 1
            self.put(path, int(self.get(path) or 0) + 1)
        current_etag = self.etags.get(path, "NULL_ETAG")
        if etag != current_etag:
            return False, self.get(path), current_etag
        self.put(path, value)
        return True, value, self.etags[path]


@pytest.fixture
def store() -> FakeStore:
    return FakeStore()


def make_image(mode: str = "RGB", size: tuple[int, int] = (640, 480), **save: Any) -> io.BytesIO:
    img = Image.new(mode, size)
    # Non-uniform content so JPEG size depends on quality.
    px = img.load()
    for x in range(0, size[0], 7):
        for y in range(0, size[1], 5):
            px[x, y] = (
                (x * 31 % 256, y * 17 % 256, (x ^ y) % 256, 128)[: len(mode)]
                if mode not in ("P", "L")
                else (x + y) % 256
            )
    buf = io.BytesIO()
    img.save(buf, format=save.pop("format", "PNG"), **save)
    buf.seek(0)
    return buf
