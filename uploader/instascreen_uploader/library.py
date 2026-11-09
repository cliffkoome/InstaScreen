"""Library operations over the ``images/`` tree.

Invariant the device relies on: slots ``1..image_count`` are the playlist.
The device tolerates a missing or corrupt slot (it skips it), so operations
are ordered so that any intermediate state is at worst a skipped slide, never
a torn image:

* every image is written with a single PUT of the whole ``image_N`` node,
  which RTDB applies atomically;
* ``image_count`` only changes through a compare-and-set transaction.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from .encode import EncodedImage, decode_node, encode, node_version
from .rtdb import Store, transaction

BASE = "images"
COUNT_PATH = f"{BASE}/image_count"


def slot_path(slot: int) -> str:
    if slot < 1:
        raise ValueError("slots are 1-based")
    return f"{BASE}/image_{slot}"


@dataclass(frozen=True)
class SlotInfo:
    slot: int
    version: int | None  # None = empty slot
    size: int | None
    chunks: int | None


def image_count(store: Store) -> int:
    return int(store.get(COUNT_PATH) or 0)


def append(store: Store, image: EncodedImage) -> int:
    """Reserve the next slot, then write the image into it. Returns the slot.

    Reserve-then-write (rather than write-then-publish) is what makes
    concurrent appends safe: two uploaders can never be handed the same slot.
    The cost is a short window in which the device may see an empty slot,
    which it already treats as "skip".
    """
    new_count = transaction(store, COUNT_PATH, lambda n: int(n or 0) + 1)
    store.put(slot_path(new_count), image.to_node())
    return new_count


def replace(store: Store, slot: int, image: EncodedImage) -> None:
    """Overwrite ``slot`` in place, extending the playlist if needed."""
    store.put(slot_path(slot), image.to_node())
    transaction(store, COUNT_PATH, lambda n: max(int(n or 0), slot))


def remove(store: Store, slot: int) -> None:
    """Delete ``slot`` and keep the playlist contiguous.

    The last image moves into the hole first, then the count shrinks, then
    the old last slot is deleted. Assumes a single writer for deletes.
    """
    count = image_count(store)
    if not 1 <= slot <= count:
        raise ValueError(f"slot {slot} is outside 1..{count}")
    if slot != count:
        last = store.get(slot_path(count))
        if last is not None:
            store.put(slot_path(slot), last)
        else:
            store.delete(slot_path(slot))
    transaction(store, COUNT_PATH, lambda n: max(int(n or 0) - 1, 0))
    store.delete(slot_path(count))


def describe(store: Store) -> list[SlotInfo]:
    """Metadata for every slot in the playlist, without downloading chunks."""
    infos = []
    for slot in range(1, image_count(store) + 1):
        base = slot_path(slot)
        meta = store.get(f"{base}/meta")
        if meta:
            infos.append(
                SlotInfo(slot, int(meta.get("v", 0)), meta.get("size"), meta.get("chunk_count"))
            )
            continue
        legacy = store.get(f"{base}/chunk_count")
        if legacy is not None:
            infos.append(SlotInfo(slot, 1, None, int(legacy)))
        else:
            infos.append(SlotInfo(slot, None, None, None))
    return infos


def migrate(store: Store, chunk_bytes: int) -> list[int]:
    """Rewrite v1 (hex) slots as v2 (base64 + CRC) without re-compressing."""
    migrated = []
    for info in describe(store):
        if info.version != 1:
            continue
        node: dict[str, Any] = store.get(slot_path(info.slot))
        if node_version(node) != 1:
            continue  # changed underneath us
        jpeg = decode_node(node)
        store.put(slot_path(info.slot), encode(jpeg, chunk_bytes).to_node())
        migrated.append(info.slot)
    return migrated
