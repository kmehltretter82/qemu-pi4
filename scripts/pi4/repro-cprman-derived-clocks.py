#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Reproduce stale BCM2835 CPRMAN derived-clock state.

This is a qemu-pi4 research aid, not an upstream submission artifact.  Run it
against an unmodified QEMU build and this fork separately:

    python3 scripts/pi4/repro-cprman-derived-clocks.py \
        /path/to/qemu-system-aarch64

Exit status 1 means at least one current-upstream defect was reproduced:

* a clock-mux DIV write left its output unchanged;
* a zero PLL-channel divider selected 255 instead of 256; and
* migration restored mux registers but lost the derived output clock.

Exit status 0 means all three defects were absent, as expected with the
corresponding qemu-pi4 fixes.
"""

import json
import os
import socket
import subprocess
import sys
import tempfile
import time


CPRMAN_BASE = 0xFE101000
CM_VPUCTL = CPRMAN_BASE + 0x008
CM_VPUDIV = CPRMAN_BASE + 0x00C
A2W_PLLD_PER = CPRMAN_BASE + 0x1540
PASSWORD = 0x5A000000
ENABLE = 1 << 4
SRC_XOSC = 1
PERIOD_1SEC = 1_000_000_000 << 32
CPRMAN_PATH = "/machine/soc/peripherals/cprman"


class Qemu:
    def __init__(self, binary: str, directory: str, name: str,
                 incoming: str | None = None) -> None:
        self.qtest_path = os.path.join(directory, name + ".qtest")
        self.qmp_path = os.path.join(directory, name + ".qmp")
        args = [
            binary,
            "-machine", "raspi4b",
            "-accel", "qtest",
            "-qtest", f"unix:{self.qtest_path},server=on,wait=off",
            "-qtest-log", "/dev/null",
            "-qmp", f"unix:{self.qmp_path},server=on,wait=off",
            "-display", "none",
            "-audio", "none",
            "-serial", "none",
            "-monitor", "none",
        ]
        if incoming is not None:
            args += ["-incoming", incoming]
        self.proc = subprocess.Popen(
            args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            text=True,
        )
        self.qtest = self._connect(self.qtest_path)
        self.qmp = self._connect(self.qmp_path)
        self.qtest_file = self.qtest.makefile("rwb", buffering=0)
        self.qmp_file = self.qmp.makefile("rwb", buffering=0)
        greeting = self._qmp_read()
        if "QMP" not in greeting:
            raise RuntimeError(f"bad QMP greeting: {greeting!r}")
        self.qmp_command("qmp_capabilities")

    def _connect(self, path: str) -> socket.socket:
        deadline = time.monotonic() + 10
        last_error: OSError | None = None
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                stderr = self.proc.stderr.read() if self.proc.stderr else ""
                raise RuntimeError(
                    f"QEMU exited {self.proc.returncode}: {stderr}"
                )
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                sock.connect(path)
                return sock
            except OSError as error:
                last_error = error
                sock.close()
                time.sleep(0.01)
        raise RuntimeError(f"could not connect to {path}: {last_error}")

    def _qmp_read(self) -> dict:
        line = self.qmp_file.readline()
        if not line:
            raise RuntimeError("unexpected QMP EOF")
        return json.loads(line)

    def qmp_command(self, execute: str, arguments: dict | None = None):
        request = {"execute": execute, "id": execute}
        if arguments is not None:
            request["arguments"] = arguments
        self.qmp_file.write(json.dumps(request).encode() + b"\n")
        while True:
            response = self._qmp_read()
            if response.get("id") != execute:
                continue
            if "error" in response:
                raise RuntimeError(f"{execute}: {response['error']}")
            return response.get("return")

    def qtest_command(self, request: str) -> str:
        self.qtest_file.write(request.encode() + b"\n")
        response = self.qtest_file.readline().decode().strip()
        if not response.startswith("OK"):
            raise RuntimeError(f"{request!r}: {response!r}")
        return response

    def writel(self, address: int, value: int) -> None:
        self.qtest_command(f"writel 0x{address:x} 0x{value:x}")

    def readl(self, address: int) -> int:
        response = self.qtest_command(f"readl 0x{address:x}")
        return int(response.split()[1], 16)

    def clock_period(self, child: str) -> int:
        return self.qmp_command(
            "qom-get",
            {
                "path": f"{CPRMAN_PATH}/{child}",
                "property": "qtest-clock-period",
            },
        )

    def wait_migration(self) -> None:
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            status = self.qmp_command("query-migrate")["status"]
            if status == "completed":
                return
            if status in ("failed", "cancelled"):
                raise RuntimeError(f"migration {status}")
            time.sleep(0.01)
        raise RuntimeError("migration timed out")

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                self.qmp_command("quit")
            except (BrokenPipeError, ConnectionError, RuntimeError):
                pass
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        if self.proc.stderr is not None:
            stderr = self.proc.stderr.read()
            if stderr:
                print(stderr, file=sys.stderr, end="")


def period_from_hz(hz: int) -> int:
    return PERIOD_1SEC // hz


def hz_from_period(period: int) -> int:
    return PERIOD_1SEC // period if period else 0


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} QEMU", file=sys.stderr)
        return 2

    binary = os.path.abspath(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix="cprman-upstream-") as directory:
        qemu = Qemu(binary, directory, "registers")
        try:
            qemu.writel(CM_VPUDIV, PASSWORD | (9 << 12))
            qemu.writel(CM_VPUCTL, PASSWORD | ENABLE | SRC_XOSC)
            mux_before = qemu.clock_period("vpu/out")
            qemu.writel(CM_VPUDIV, PASSWORD | (18 << 12))
            mux_div_reg = qemu.readl(CM_VPUDIV)
            mux_after = qemu.clock_period("vpu/out")

            plld_period = qemu.clock_period("plld/out")
            plld_hz = hz_from_period(plld_period)
            qemu.writel(A2W_PLLD_PER, PASSWORD)
            zero_div_period = qemu.clock_period("plld-per/out")
            zero_div_hz = hz_from_period(zero_div_period)
            expected_255 = plld_hz // 255
            expected_256 = plld_hz // 256
        finally:
            qemu.close()

        source = Qemu(binary, directory, "source")
        destination = None
        state_path = os.path.join(directory, "state")
        uri = "file:" + state_path
        try:
            source.writel(CM_VPUDIV, PASSWORD | (9 << 12))
            source.writel(CM_VPUCTL, PASSWORD | ENABLE | SRC_XOSC)
            migration_before = source.clock_period("vpu/out")
            source.qmp_command("stop")
            source.qmp_command("migrate", {"uri": uri})
            source.wait_migration()
            destination = Qemu(binary, directory, "destination", uri)
            destination.wait_migration()
            migration_after = destination.clock_period("vpu/out")
            migrated_ctl = destination.readl(CM_VPUCTL)
            migrated_div = destination.readl(CM_VPUDIV)
        finally:
            if destination is not None:
                destination.close()
            source.close()

    divider_stale = mux_after == mux_before
    zero_is_255 = zero_div_period == period_from_hz(expected_255)
    zero_is_not_256 = zero_div_period != period_from_hz(expected_256)
    migration_stale = (
        migration_before != 0 and migration_after != migration_before
    )

    print(f"mux before DIV write: {hz_from_period(mux_before)} Hz")
    print(f"mux after DIV write:  {hz_from_period(mux_after)} Hz")
    print(f"stored VPU DIV:       0x{mux_div_reg:08x}")
    print(f"divider output:       {zero_div_hz} Hz")
    print(f"PLLD / 255:           {expected_255} Hz")
    print(f"PLLD / 256:           {expected_256} Hz")
    print(f"migration before:     {hz_from_period(migration_before)} Hz")
    print(f"migration after:      {hz_from_period(migration_after)} Hz")
    print(f"migrated VPUCTL/DIV:  0x{migrated_ctl:08x}/0x{migrated_div:08x}")
    print(f"stale divider output: {divider_stale}")
    print(f"zero means 255:       {zero_is_255 and zero_is_not_256}")
    print(f"lost migrated output: {migration_stale}")

    return 1 if divider_stale or zero_is_255 or migration_stale else 0


if __name__ == "__main__":
    raise SystemExit(main())
