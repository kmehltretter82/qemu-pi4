#!/usr/bin/env python3
"""Build a small, deterministic gzip-compressed newc initramfs."""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import gzip
import os
from pathlib import Path
import stat
import tempfile


def _pad(archive: bytearray) -> None:
    archive.extend(b"\0" * (-len(archive) % 4))


def _append_newc(archive: bytearray, inode: int, name: str, mode: int,
                 data: bytes, mtime: int, nlink: int = 1,
                 rdevmajor: int = 0, rdevminor: int = 0) -> None:
    encoded_name = name.encode("utf-8") + b"\0"
    fields = (
        inode,
        mode,
        0,                  # uid
        0,                  # gid
        nlink,
        mtime,
        len(data),
        0,                  # devmajor
        0,                  # devminor
        rdevmajor,
        rdevminor,
        len(encoded_name),
        0,                  # check
    )
    header = "070701" + "".join(f"{value:08x}" for value in fields)
    archive.extend(header.encode("ascii"))
    archive.extend(encoded_name)
    _pad(archive)
    archive.extend(data)
    _pad(archive)


def build_archive(init_path: Path, mtime: int) -> bytes:
    archive = bytearray()
    directories = (".", "dev", "proc", "sys", "tmp")

    for inode, name in enumerate(directories, start=1):
        mode = stat.S_IFDIR | (0o1777 if name == "tmp" else 0o755)
        _append_newc(archive, inode, name, mode, b"", mtime, nlink=2)

    next_inode = len(directories) + 1
    _append_newc(archive, next_inode, "dev/console",
                 stat.S_IFCHR | 0o600, b"", mtime,
                 rdevmajor=5, rdevminor=1)
    _append_newc(archive, next_inode + 1, "dev/null",
                 stat.S_IFCHR | 0o666, b"", mtime,
                 rdevmajor=1, rdevminor=3)
    _append_newc(archive, next_inode + 2, "init",
                 stat.S_IFREG | 0o755, init_path.read_bytes(), mtime)
    _append_newc(archive, 0, "TRAILER!!!", 0, b"", mtime)
    return bytes(archive)


def write_gzip(archive: bytes, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = tempfile.NamedTemporaryFile(
        prefix=f".{output_path.name}.", dir=output_path.parent, delete=False)
    temporary_path = Path(temporary.name)

    try:
        with temporary:
            with gzip.GzipFile(filename="", mode="wb", compresslevel=9,
                               fileobj=temporary, mtime=0) as compressed:
                compressed.write(archive)
        os.replace(temporary_path, output_path)
        output_path.chmod(0o644)
    except BaseException:
        temporary_path.unlink(missing_ok=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("init", type=Path, help="statically linked init binary")
    parser.add_argument("output", type=Path, help="output .cpio.gz path")
    parser.add_argument("--mtime", type=int, default=0,
                        help="fixed cpio entry timestamp (default: 0)")
    args = parser.parse_args()

    if not args.init.is_file():
        parser.error(f"init binary does not exist: {args.init}")
    write_gzip(build_archive(args.init, args.mtime), args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
