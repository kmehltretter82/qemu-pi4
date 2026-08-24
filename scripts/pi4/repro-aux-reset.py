#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Reproduce stale BCM2835 AUX UART state across a system reset.

This is a qemu-pi4 research aid, not an upstream submission artifact.  Run it
against an unmodified QEMU build and this fork separately:

    python3 scripts/pi4/repro-aux-reset.py /path/to/qemu-system-aarch64

Exit status 1 means the AUX enable, interrupt-enable, or interrupt state did
not return to its QEMU startup value after a guest-requested cold reset.  The
script explicitly enables the mini UART before comparing its internal state,
so it works with both QEMU's historical always-enabled model and models which
implement the hardware enable gate.  MCR values are printed as extra context.
"""

import subprocess
import sys


AUX_BASE = 0xFE215000
AUX_IRQ = AUX_BASE + 0x00
AUX_ENABLES = AUX_BASE + 0x04
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
        startup_enable = readl(AUX_ENABLES)
        writel(AUX_ENABLES, 1)
        baseline_enable = readl(AUX_ENABLES)
        baseline = (readl(AUX_MU_IER), readl(AUX_IRQ), readl(AUX_MU_MCR))

        writel(AUX_MU_IER, TX_INTERRUPT_ENABLE)
        writel(AUX_MU_MCR, 0xFFFFFFFF)
        armed_enable = readl(AUX_ENABLES)
        armed = (readl(AUX_MU_IER), readl(AUX_IRQ), readl(AUX_MU_MCR))

        writel(PM_WDOG, PM_PASSWORD | PM_WDOG_ONE_SECOND)
        writel(PM_RSTC, PM_PASSWORD | PM_RSTC_FULL)
        command("clock_step 1000000000")
        post_reset_enable = readl(AUX_ENABLES)
        gated = (readl(AUX_MU_IER), readl(AUX_IRQ), readl(AUX_MU_MCR))
        writel(AUX_ENABLES, 1)
        restored_enable = readl(AUX_ENABLES)
        restored = (readl(AUX_MU_IER), readl(AUX_IRQ), readl(AUX_MU_MCR))

        print(f"startup EN:     0x{startup_enable:02x}")
        print("                 EN IER IRQ MCR")
        print(
            f"baseline:   0x{baseline_enable:02x} 0x{baseline[0]:02x}  "
            f"{baseline[1]}  0x{baseline[2]:02x}"
        )
        print(
            f"armed:      0x{armed_enable:02x} 0x{armed[0]:02x}  "
            f"{armed[1]}  0x{armed[2]:02x}"
        )
        print(
            f"post-reset: 0x{post_reset_enable:02x} 0x{gated[0]:02x}  "
            f"{gated[1]}  0x{gated[2]:02x}"
        )
        print(
            f"re-enabled: 0x{restored_enable:02x} 0x{restored[0]:02x}  "
            f"{restored[1]}  0x{restored[2]:02x}"
        )
        expected_armed_ier = baseline[0] | TX_INTERRUPT_ENABLE
        expected_gated = baseline if post_reset_enable & 1 else (0, 0, 0)
        passed = (
            baseline_enable & 1
            and baseline[1:] == (0, 0)
            and armed[:2] == (expected_armed_ier, 1)
            and post_reset_enable == startup_enable
            and gated == expected_gated
            and restored_enable & 1
            and restored == baseline
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
