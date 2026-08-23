#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Reproduce stale Raspberry Pi property-mailbox state across reset.

This is a qemu-pi4 research aid, not an upstream submission artifact.  Run it
against an unmodified QEMU build and this fork separately:

    python3 scripts/pi4/repro-property-reset.py /path/to/qemu-system-aarch64

Exit status 1 means the first mailbox read after reset returned something
other than the response address for the valid post-reset property request.
"""

import subprocess
import sys


PROPERTY_BUFFER = 0x1000
MBOX_READ = 0xFE00B880
MBOX_WRITE = 0xFE00B8A0
PM_RSTC = 0xFE10001C
PM_WDOG = 0xFE100024
MBOX_CHAN_PROPERTY = 8
MBOX_SIZE = 32


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

    def command(request: str) -> str:
        assert proc.stdin is not None
        assert proc.stdout is not None
        proc.stdin.write(request + "\n")
        proc.stdin.flush()
        response = proc.stdout.readline().strip()
        if not response.startswith("OK"):
            raise RuntimeError(f"{request!r}: {response!r}")
        return response

    def writel(address: int, value: int) -> None:
        command(f"writel 0x{address:x} 0x{value:x}")

    def readl(address: int) -> int:
        return int(command(f"readl 0x{address:x}").split()[1], 16)

    try:
        # GET_CLOCK_STATE(ARM), with room for its two-word response.
        writel(PROPERTY_BUFFER, 32)
        writel(PROPERTY_BUFFER + 8, 0x00030001)
        writel(PROPERTY_BUFFER + 12, 8)
        writel(PROPERTY_BUFFER + 20, 3)
        writel(PROPERTY_BUFFER + 24, 0xFFFFFFFF)
        writel(PROPERTY_BUFFER + 28, 0)

        # Fill the ARM response FIFO, then leave one child response pending.
        for _ in range(MBOX_SIZE + 1):
            writel(PROPERTY_BUFFER + 4, 0)
            writel(PROPERTY_BUFFER + 16, 0)
            writel(MBOX_WRITE, PROPERTY_BUFFER | MBOX_CHAN_PROPERTY)

        # Request an ordinary guest reset through the emulated Pi watchdog.
        writel(PM_WDOG, 0x5A010000)
        writel(PM_RSTC, 0x5A000020)
        command("clock_step 1000000000")

        # Submit one valid request after reset and read its response address.
        writel(PROPERTY_BUFFER + 4, 0)
        writel(PROPERTY_BUFFER + 16, 0)
        writel(MBOX_WRITE, PROPERTY_BUFFER | MBOX_CHAN_PROPERTY)
        response = readl(MBOX_READ)
        expected = PROPERTY_BUFFER | MBOX_CHAN_PROPERTY
        print(f"post-reset mailbox response: 0x{response:08x}")
        print(f"expected property response: 0x{expected:08x}")
        return 0 if response == expected else 1
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        if proc.stderr is not None:
            stderr = proc.stderr.read()
            if stderr:
                print(stderr, file=sys.stderr, end="")


if __name__ == "__main__":
    raise SystemExit(main())
