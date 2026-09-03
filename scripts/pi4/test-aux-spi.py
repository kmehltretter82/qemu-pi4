#!/usr/bin/env python3
"""Validate Linux's BCM2835 AUX SPI1 driver against a QEMU M25P80 device.

The guest issues one full-duplex spidev transfer, which is the exchange the
controller's native chip select can hold; see the overlay for why a SPI-NOR
message cannot be used here.
"""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


AUX_SPI_CHECK_MARKER = (
    "PI4-LAB: AUX SPI1 M25P80 Linux driver and JEDEC exchange"
)
AUX_SPI_READ_MARKER = "PI4-LAB: AUX SPI1 M25P80 JEDEC id 20 20 14 from /dev/spidev"
SUCCESS_MARKER = "PI4-LAB: upstream Linux boot successful"


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent.parent

    parser.add_argument(
        "--qemu",
        type=Path,
        default=repo_root / "build" / "qemu-system-aarch64",
    )
    parser.add_argument(
        "--artifacts",
        type=Path,
        default=repo_root / "build-pi4-linux" / "artifacts",
    )
    parser.add_argument(
        "--machine", choices=("raspi4b", "raspi400"), default="raspi4b"
    )
    parser.add_argument("--timeout", type=float, default=30.0)
    return parser.parse_args()


def validate_output(output):
    for marker in (
        AUX_SPI_CHECK_MARKER,
        AUX_SPI_READ_MARKER,
        SUCCESS_MARKER,
    ):
        if marker not in output:
            raise RuntimeError(f"guest did not produce: {marker}")


def build_overlay(source, output, input_dtb, output_dtb):
    dtc = shutil.which("dtc")
    fdtoverlay = shutil.which("fdtoverlay")

    if not dtc or not fdtoverlay:
        raise FileNotFoundError("dtc and fdtoverlay are required")
    subprocess.run(
        (dtc, "-@", "-I", "dts", "-O", "dtb", "-o", str(output),
         str(source)),
        check=True,
    )
    subprocess.run(
        (fdtoverlay, "-i", str(input_dtb), "-o", str(output_dtb),
         str(output)),
        check=True,
    )


def main():
    args = parse_args()
    script_dir = Path(__file__).resolve().parent
    qemu = args.qemu.resolve()
    artifacts = args.artifacts.resolve()
    dtb_name = {
        "raspi4b": "bcm2711-rpi-4-b.dtb",
        "raspi400": "bcm2711-rpi-400.dtb",
    }[args.machine]
    overlay_source = script_dir / "aux-spi1-m25p80-overlay.dts"

    for path in (
        qemu,
        artifacts / "Image",
        artifacts / dtb_name,
        artifacts / "initramfs.cpio.gz",
        overlay_source,
    ):
        if not path.exists():
            raise FileNotFoundError(path)

    kernel_append = " ".join((
        "earlycon=pl011,mmio32,0xfe201000",
        "console=ttyAMA0,115200",
        "rdinit=/init",
        "panic=-1",
        "clk_ignore_unused",
        "ip=dhcp",
        "pi4lab.aux_spi_test=1",
    ))

    with tempfile.TemporaryDirectory(prefix="qemu-pi4-aux-spi-", dir="/tmp") as tmp:
        tmpdir = Path(tmp)
        overlay = tmpdir / "aux-spi1.dtbo"
        dtb = tmpdir / "aux-spi1.dtb"

        build_overlay(overlay_source, overlay, artifacts / dtb_name, dtb)
        command = (
            str(qemu),
            "-machine", args.machine,
            "-kernel", str(artifacts / "Image"),
            "-dtb", str(dtb),
            "-initrd", str(artifacts / "initramfs.cpio.gz"),
            "-append", kernel_append,
            "-device", "m25p80,id=spi1flash,bus=spi1",
            "-nic", "user,model=genet",
            "-display", "none",
            "-monitor", "none",
            "-serial", "stdio",
            "-no-reboot",
        )
        process = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            output, _ = process.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                output, _ = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                output, _ = process.communicate()
            raise TimeoutError("QEMU did not finish the AUX SPI test run")

    if process.returncode:
        sys.stdout.write(output)
        raise RuntimeError(
            "QEMU exited before completing the AUX SPI test "
            f"({process.returncode})"
        )
    try:
        validate_output(output)
    except RuntimeError:
        sys.stdout.write(output)
        raise
    print(
        f"PI4-AUX-SPI: {args.machine} Linux SPI1 driver and single "
        "full-duplex M25P80 JEDEC exchange verified"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, TimeoutError,
            subprocess.CalledProcessError) as error:
        print(f"PI4-AUX-SPI: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
