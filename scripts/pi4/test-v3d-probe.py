#!/usr/bin/env python3
"""Validate the opt-in BCM2711 V3D Linux driver-probe path without jobs."""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import subprocess
import sys
from pathlib import Path


V3D_DRIVER_MARKER = "[drm] Initialized v3d 1.0.0 for fec00000.gpu"
V3D_HUB_MAPPING_MARKER = "fec00000-fec03fff : fec00000.gpu hub"
V3D_CORE_MAPPING_MARKER = "fec04000-fec07fff : fec00000.gpu core0"
SUCCESS_MARKER = "PI4-LAB: upstream Linux boot successful"
ASB_FAILURE_MARKERS = (
    "Failed to enable ASB master for v3d",
    "Failed to enable ASB slave for v3d",
    "Failed to disable ASB master for v3d",
    "Failed to disable ASB slave for v3d",
)


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
        V3D_DRIVER_MARKER,
        V3D_HUB_MAPPING_MARKER,
        V3D_CORE_MAPPING_MARKER,
        SUCCESS_MARKER,
    ):
        if marker not in output:
            raise RuntimeError(f"guest did not produce: {marker}")
    for marker in ASB_FAILURE_MARKERS:
        if marker in output:
            raise RuntimeError(f"guest reported V3D bridge failure: {marker}")


def main():
    args = parse_args()
    qemu = args.qemu.resolve()
    artifacts = args.artifacts.resolve()
    dtb_name = {
        "raspi4b": "bcm2711-rpi-4-b.dtb",
        "raspi400": "bcm2711-rpi-400.dtb",
    }[args.machine]

    for path in (
        qemu,
        artifacts / "Image",
        artifacts / dtb_name,
        artifacts / "initramfs.cpio.gz",
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
    ))
    command = (
        str(qemu),
        "-global", "bcm2711-v3d.enable-probe-dtb=true",
        "-machine", args.machine,
        "-kernel", str(artifacts / "Image"),
        "-dtb", str(artifacts / dtb_name),
        "-initrd", str(artifacts / "initramfs.cpio.gz"),
        "-append", kernel_append,
        "-nic", "user,model=genet",
        "-display", "none",
        "-monitor", "none",
        "-serial", "stdio",
        "-no-reboot",
    )

    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
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
        raise TimeoutError("QEMU did not finish the V3D probe run")

    if process.returncode:
        sys.stdout.write(output)
        raise RuntimeError(
            "QEMU exited before completing the V3D probe "
            f"({process.returncode})"
        )
    try:
        validate_output(output)
    except RuntimeError:
        sys.stdout.write(output)
        raise
    print(
        f"PI4-V3D: {args.machine} V3D 4.2 Linux driver probe verified "
        "without submitting GPU work"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, TimeoutError) as error:
        print(f"PI4-V3D: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
