"""``instascreen`` command-line interface."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from . import library
from .encode import (
    DEFAULT_CHUNK_BYTES,
    MAX_IMAGE_BYTES,
    ImageTooLargeError,
    encode,
    prepare_jpeg,
)
from .rtdb import RtdbClient


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="instascreen", description="Publish images to an InstaScreen frame."
    )
    p.add_argument(
        "--url",
        default=os.environ.get("FIREBASE_URL"),
        help="Realtime Database URL (default: $FIREBASE_URL)",
    )
    p.add_argument(
        "--auth",
        default=os.environ.get("FIREBASE_AUTH"),
        help="database secret or ID token (default: $FIREBASE_AUTH)",
    )
    sub = p.add_subparsers(dest="command", required=True)

    up = sub.add_parser("upload", help="prepare and publish one or more images")
    up.add_argument("files", nargs="+", type=Path)
    up.add_argument(
        "--slot",
        type=int,
        help="overwrite this slot instead of appending (only with a single file)",
    )
    up.add_argument("--quality", type=int, default=85, help="starting JPEG quality (default 85)")
    up.add_argument(
        "--max-bytes",
        type=int,
        default=MAX_IMAGE_BYTES,
        help=f"device image budget in bytes (default {MAX_IMAGE_BYTES})",
    )
    up.add_argument(
        "--chunk-bytes",
        type=int,
        default=DEFAULT_CHUNK_BYTES,
        help=f"raw bytes per chunk (default {DEFAULT_CHUNK_BYTES})",
    )
    up.add_argument("--dry-run", action="store_true", help="prepare and report, don't upload")

    sub.add_parser("list", help="show the playlist")

    rm = sub.add_parser("delete", help="remove a slot and compact the playlist")
    rm.add_argument("slot", type=int)

    mg = sub.add_parser("migrate", help="convert legacy v1 (hex) slots to v2")
    mg.add_argument("--chunk-bytes", type=int, default=DEFAULT_CHUNK_BYTES)
    return p


def _upload(args: argparse.Namespace) -> int:
    if args.slot is not None and len(args.files) != 1:
        print("--slot takes exactly one file", file=sys.stderr)
        return 2

    store = None if args.dry_run else RtdbClient(args.url, args.auth)
    failures = 0
    for path in args.files:
        try:
            jpeg = prepare_jpeg(path, quality=args.quality, max_bytes=args.max_bytes)
            image = encode(jpeg, args.chunk_bytes)
        except (OSError, ImageTooLargeError, ValueError) as e:
            print(f"skip  {path}: {e}", file=sys.stderr)
            failures += 1
            continue

        summary = f"{path.name}: {len(jpeg)} B, {len(image.chunks)} chunks, crc32={image.crc32:08x}"
        if store is None:
            print(f"ready {summary}")
        elif args.slot is not None:
            library.replace(store, args.slot, image)
            print(f"slot {args.slot} <- {summary}")
        else:
            slot = library.append(store, image)
            print(f"slot {slot} <- {summary}")
    return 1 if failures else 0


def main(argv: list[str] | None = None) -> int:
    try:
        from dotenv import load_dotenv

        load_dotenv()
    except ImportError:  # optional convenience
        pass

    args = _build_parser().parse_args(argv)
    try:
        if args.command == "upload":
            return _upload(args)

        store = RtdbClient(args.url, args.auth)
        if args.command == "list":
            infos = library.describe(store)
            if not infos:
                print("playlist is empty")
            for i in infos:
                state = "empty" if i.version is None else f"v{i.version}"
                size = f"{i.size} B" if i.size else "-"
                print(f"{i.slot:>4}  {state:<5}  {size:>9}  {i.chunks or '-':>4} chunks")
        elif args.command == "delete":
            library.remove(store, args.slot)
            print(f"deleted slot {args.slot}")
        elif args.command == "migrate":
            done = library.migrate(store, args.chunk_bytes)
            print(f"migrated slots: {done or 'none'}")
        return 0
    except Exception as e:  # surface a clean message instead of a traceback
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
