#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise published Raspberry Pi property-interface contracts.

This is a qemu-pi4 research aid, not an upstream submission artifact.  Run it
against an unmodified QEMU build and this fork separately:

    python3 scripts/pi4/repro-property-contracts.py QEMU palette
    python3 scripts/pi4/repro-property-contracts.py QEMU framebuffer-order

The palette mode checks the documented no-partial-apply rule by placing a
sentinel immediately after the 256-entry palette.  The framebuffer-order mode
observes whether a Get tag sees a later Set in the same framebuffer operation.
It remains evidence only until its contract interpretation and output are
independently reviewed and manually validated.
"""

import selectors
import struct
import subprocess
import sys


PROPERTY_BUFFER = 0x1000
MBOX_READ = 0xFE00B880
MBOX_WRITE = 0xFE00B8A0
MBOX_CHAN_PROPERTY = 8
VCRAM_BASE = 0x3C000000
PROPERTY_RESPONSE_SUCCESS = 0x80000000
COMMAND_TIMEOUT = 2


def main() -> int:
    if len(sys.argv) != 3 or sys.argv[2] not in {"palette", "framebuffer-order"}:
        print(f"usage: {sys.argv[0]} QEMU palette|framebuffer-order", file=sys.stderr)
        return 2

    mode = sys.argv[2]
    proc = subprocess.Popen(
        [
            sys.argv[1],
            "-machine",
            "raspi4b",
            "-accel",
            "qtest",
            "-qtest",
            "stdio",
            "-display",
            "none",
            "-audio",
            "none",
            "-qtest-log",
            "/dev/null",
            "-serial",
            "none",
            "-monitor",
            "none",
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    selector = selectors.DefaultSelector()
    assert proc.stdout is not None
    selector.register(proc.stdout, selectors.EVENT_READ)

    def command(request: str) -> str:
        assert proc.stdin is not None
        proc.stdin.write(request + "\n")
        proc.stdin.flush()
        if not selector.select(COMMAND_TIMEOUT):
            raise TimeoutError(request)
        response = proc.stdout.readline().strip()
        if not response.startswith("OK"):
            raise RuntimeError(f"{request!r}: {response!r}")
        return response

    def writel(address: int, value: int) -> None:
        command(f"writel 0x{address:x} 0x{value:x}")

    def readl(address: int) -> int:
        return int(command(f"readl 0x{address:x}").split()[1], 16)

    def submit(words: list[int]) -> None:
        data = b"".join(struct.pack("<I", word) for word in words)
        command(f"write 0x{PROPERTY_BUFFER:x} {len(data)} 0x{data.hex()}")
        writel(MBOX_WRITE, PROPERTY_BUFFER | MBOX_CHAN_PROPERTY)
        expected = PROPERTY_BUFFER | MBOX_CHAN_PROPERTY
        if readl(MBOX_READ) != expected:
            raise RuntimeError("property response address mismatch")

    try:
        if mode == "palette":
            colors = [0x11111111, 0x22222222, 0x33333333, 0x44444444]
            writel(VCRAM_BASE + 254 * 4, 0xA5A5A5A5)
            writel(VCRAM_BASE + 255 * 4, 0xB5B5B5B5)
            writel(VCRAM_BASE + 256 * 4, 0xC5C5C5C5)
            words = [
                60,
                0,
                0x0004800B,
                36,
                0,
                254,
                len(colors),
                *colors,
                0,
                0,
                0,
                0,
                0,
            ]
            submit(words)
            tag_status = readl(PROPERTY_BUFFER + 16)
            response = readl(PROPERTY_BUFFER + 20)
            after = readl(VCRAM_BASE + 256 * 4)
            print(f"tag status:       0x{tag_status:08x}")
            print(f"palette response: 0x{response:08x}")
            print(f"entry 256:        0x{after:08x}")
            if tag_status == PROPERTY_RESPONSE_SUCCESS | 4 and after == colors[2]:
                print("out-of-interval palette write reproduced")
            else:
                print("no out-of-interval write observed")
            return 0

        words = [
            44,
            0,
            0x00040005,
            4,
            0,
            0xA5A5A5A5,
            0x00048005,
            4,
            0,
            32,
            0,
        ]
        submit(words)
        response_status = readl(PROPERTY_BUFFER + 4)
        get_status = readl(PROPERTY_BUFFER + 16)
        get_depth = readl(PROPERTY_BUFFER + 20)
        set_status = readl(PROPERTY_BUFFER + 32)
        set_depth = readl(PROPERTY_BUFFER + 36)
        print(f"response status:  0x{response_status:08x}")
        print(f"GET status/depth: 0x{get_status:08x} / {get_depth}")
        print(f"SET status/depth: 0x{set_status:08x} / {set_depth}")
        tags_accepted = (
            response_status == PROPERTY_RESPONSE_SUCCESS
            and get_status == (PROPERTY_RESPONSE_SUCCESS | 4)
            and set_status == (PROPERTY_RESPONSE_SUCCESS | 4)
        )
        if tags_accepted and get_depth == 16 and set_depth == 32:
            print("framebuffer tags were applied in guest order")
        elif tags_accepted and get_depth == 32 and set_depth == 32:
            print("framebuffer GET observed the final SET state")
        else:
            print("framebuffer order differs or SET was rejected")
        return 0
    finally:
        selector.close()
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        if proc.stderr is not None:
            stderr = proc.stderr.read()
            if stderr:
                print(stderr, file=sys.stderr, end="")


if __name__ == "__main__":
    raise SystemExit(main())
