#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the BCM2835 property parser's exact zero-progress tag size.

This is a qemu-pi4 research aid, not an upstream submission artifact.  Run it
against an unmodified QEMU build and this fork separately:

    python3 scripts/pi4/repro-property-parser.py /path/to/qemu-system-aarch64

Exit status 1 means the mailbox MMIO write did not complete, or the malformed
request was not rejected safely.  The old parser hangs because
``0xfffffff4 + 12`` wraps to zero before advancing its tag cursor.
"""

import selectors
import subprocess
import sys


PROPERTY_BUFFER = 0x1000
MBOX_READ = 0xFE00B880
MBOX_WRITE = 0xFE00B8A0
MBOX_CHAN_PROPERTY = 8
PROPERTY_RESPONSE_ERROR = 0x80000001
COMMAND_TIMEOUT = 2


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} QEMU", file=sys.stderr)
        return 2

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

    try:
        # One tag, a nominal end word, and the exact old zero-progress size.
        writel(PROPERTY_BUFFER, 24)
        writel(PROPERTY_BUFFER + 4, 0)
        writel(PROPERTY_BUFFER + 8, 0x00000001)
        writel(PROPERTY_BUFFER + 12, 0xFFFFFFF4)
        writel(PROPERTY_BUFFER + 16, 0)
        writel(PROPERTY_BUFFER + 20, 0)

        try:
            writel(MBOX_WRITE, PROPERTY_BUFFER | MBOX_CHAN_PROPERTY)
        except TimeoutError:
            print("mailbox MMIO write timed out: property parser lost progress")
            return 1

        response = readl(MBOX_READ)
        status = readl(PROPERTY_BUFFER + 4)
        tag_status = readl(PROPERTY_BUFFER + 16)
        expected = PROPERTY_BUFFER | MBOX_CHAN_PROPERTY
        print(f"mailbox response: 0x{response:08x}")
        print(f"buffer status:   0x{status:08x}")
        print(f"tag status:      0x{tag_status:08x}")
        if (response == expected and status == PROPERTY_RESPONSE_ERROR and
                tag_status == 0):
            print("malformed tag rejected safely")
            return 0
        print("unexpected malformed-tag result")
        return 1
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
