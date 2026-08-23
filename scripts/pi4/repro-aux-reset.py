#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Reproduce stale BCM2835 AUX UART state across a system reset.

This is a qemu-pi4 research aid, not an upstream submission artifact.  Run it
against an unmodified QEMU build and this fork separately:

    python3 scripts/pi4/repro-aux-reset.py /path/to/qemu-system-aarch64

Exit status 1 means the AUX interrupt-enable or interrupt state did not return
to its QEMU startup value after a guest-requested cold reset.  MCR values are
also printed so the fork's RTS register support can be compared directly.
"""

import subprocess
import sys


AUX_BASE = 0xFE215000
AUX_IRQ = AUX_BASE + 0x00
AUX_MU_IER = AUX_BASE + 0x44
AUX_MU_MCR = AUX_BASE + 0x50
PM_RSTC = 0xFE10001C
PM_WDOG = 0xFE100024
PM_PASSWORD = 0x5A000000
PM_RSTC_FULL = 0x20
PM_WDOG_ONE_SECOND = 0x10000
TX_INTERRUPT_ENABLE = 0x02


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
        initial = (readl(AUX_MU_IER), readl(AUX_IRQ), readl(AUX_MU_MCR))

        writel(AUX_MU_IER, TX_INTERRUPT_ENABLE)
        writel(AUX_MU_MCR, 0xFFFFFFFF)
        armed = (readl(AUX_MU_IER), readl(AUX_IRQ), readl(AUX_MU_MCR))

        writel(PM_WDOG, PM_PASSWORD | PM_WDOG_ONE_SECOND)
        writel(PM_RSTC, PM_PASSWORD | PM_RSTC_FULL)
        command("clock_step 1000000000")
        post_reset = (readl(AUX_MU_IER), readl(AUX_IRQ), readl(AUX_MU_MCR))

        print("                 IER IRQ MCR")
        print(
            f"initial:    0x{initial[0]:02x}  "
            f"{initial[1]}  0x{initial[2]:02x}"
        )
        print(f"armed:      0x{armed[0]:02x}  {armed[1]}  0x{armed[2]:02x}")
        print(
            f"post-reset: 0x{post_reset[0]:02x}  "
            f"{post_reset[1]}  0x{post_reset[2]:02x}"
        )
        expected_initial = (0xC0, 0, 0)
        expected_armed_prefix = (0xC2, 1)
        expected_post_reset = (0xC0, 0, 0)
        passed = (
            initial == expected_initial
            and armed[:2] == expected_armed_prefix
            and post_reset == expected_post_reset
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
