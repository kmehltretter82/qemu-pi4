#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare BCM2835 AUX enable-gate and register behavior.

This is a qemu-pi4 research aid, not an upstream submission artifact.  Run it
against an unmodified QEMU build and this fork separately:

    python3 scripts/pi4/repro-aux-enable.py /path/to/qemu-system-aarch64

The expected values come from repeated Pi 400 MMIO observations.  Exit status
1 means at least one startup, gate, readback, or retained-write contract does
not match that hardware.
"""

import subprocess
import sys


AUX_BASE = 0xFE215000
AUX_IRQ = AUX_BASE + 0x00
AUX_ENABLES = AUX_BASE + 0x04
AUX_MU_IER = AUX_BASE + 0x44
AUX_MU_MCR = AUX_BASE + 0x50
AUX_MU_MSR = AUX_BASE + 0x58
AUX_MU_SCRATCH = AUX_BASE + 0x5C
TX_INTERRUPT_ENABLE = 0x02
UART_INTERRUPT_PENDING = 0x01


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

    def snapshot() -> tuple[int, int, int, int, int, int]:
        return (
            readl(AUX_ENABLES),
            readl(AUX_MU_IER),
            readl(AUX_IRQ) & UART_INTERRUPT_PENDING,
            readl(AUX_MU_MCR),
            readl(AUX_MU_MSR),
            readl(AUX_MU_SCRATCH),
        )

    def print_snapshot(name: str, values: tuple[int, ...]) -> None:
        print(
            f"{name:<13} 0x{values[0]:02x} 0x{values[1]:02x}  "
            f"{values[2]} 0x{values[3]:02x} 0x{values[4]:02x}  "
            f"0x{values[5]:02x}"
        )

    try:
        startup = snapshot()

        writel(AUX_ENABLES, 0xFFFFFFFF)
        low_byte = readl(AUX_ENABLES)
        writel(AUX_ENABLES, 0)

        writel(AUX_MU_IER, TX_INTERRUPT_ENABLE)
        writel(AUX_MU_MCR, 0xFFFFFFFF)
        writel(AUX_MU_SCRATCH, 0x1A5)
        hidden = snapshot()

        writel(AUX_ENABLES, 1)
        visible = snapshot()

        print("              EN   IER  IRQ MCR  MSR  SCRATCH")
        print_snapshot("startup:", startup)
        print(f"low-byte:     0x{low_byte:02x}")
        print_snapshot("disabled:", hidden)
        print_snapshot("enabled:", visible)

        expected_startup = (0, 0, 0, 0, 0, 0)
        expected_hidden = (0, 0, 1, 0, 0, 0)
        expected_visible = (1, 2, 1, 2, 0x10, 0xA5)
        passed = (
            startup == expected_startup
            and low_byte == 0xFF
            and hidden == expected_hidden
            and visible == expected_visible
        )
        return 0 if passed else 1
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
