#!/usr/bin/env python3
"""Validate native BCM2711 DRM/KMS scanout through a QEMU screendump."""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import json
import queue
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


DISPLAY_WIDTH = 1280
DISPLAY_HEIGHT = 800
PATTERN_MARKER = "VC4 scanout deterministic RGB565 pattern ready"
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
    parser.add_argument(
        "--output",
        type=Path,
        help="retain the validated PPM screendump at this path",
    )
    parser.add_argument("--timeout", type=float, default=30.0)
    return parser.parse_args()


def connect_qmp(path, process, deadline):
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(
                f"QEMU exited before QMP was ready ({process.returncode})"
            )
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            client.connect(str(path))
            stream = client.makefile("rwb", buffering=0)
            greeting = json.loads(stream.readline())
            if "QMP" not in greeting:
                raise RuntimeError(f"unexpected QMP greeting: {greeting!r}")
            qmp_command(stream, "qmp_capabilities")
            return client, stream
        except (FileNotFoundError, ConnectionRefusedError):
            client.close()
            time.sleep(0.05)
    raise TimeoutError("timed out connecting to QMP")


def qmp_command(stream, command, arguments=None):
    request = {"execute": command}
    if arguments is not None:
        request["arguments"] = arguments
    stream.write(json.dumps(request).encode("utf-8") + b"\n")

    while True:
        line = stream.readline()
        if not line:
            raise RuntimeError(f"QMP disconnected during {command}")
        response = json.loads(line)
        if "event" in response:
            continue
        if "error" in response:
            raise RuntimeError(f"QMP {command} failed: {response['error']!r}")
        if "return" in response:
            return response["return"]


def console_reader(stream, messages):
    try:
        for raw_line in iter(stream.readline, b""):
            line = raw_line.decode("utf-8", errors="replace")
            sys.stdout.write(line)
            sys.stdout.flush()
            messages.put(line)
    finally:
        messages.put(None)


def wait_for_guest(messages, process, timeout):
    deadline = time.monotonic() + timeout
    pattern_ready = False
    success = False

    while time.monotonic() < deadline and not (pattern_ready and success):
        if process.poll() is not None:
            raise RuntimeError(
                f"QEMU exited before display readiness ({process.returncode})"
            )
        try:
            line = messages.get(timeout=min(0.25, deadline - time.monotonic()))
        except queue.Empty:
            continue
        if line is None:
            raise RuntimeError("QEMU console closed before display readiness")
        if PATTERN_MARKER in line:
            if line.rstrip().endswith("FAIL"):
                raise RuntimeError("guest failed to write the framebuffer pattern")
            pattern_ready = True
        if SUCCESS_MARKER in line:
            success = True

    if not pattern_ready:
        raise TimeoutError("guest did not report a ready framebuffer pattern")
    if not success:
        raise RuntimeError("guest display run did not pass the Linux acceptance checks")


def read_ppm(path):
    with path.open("rb") as stream:
        if stream.readline().strip() != b"P6":
            raise RuntimeError("screendump is not a binary PPM")

        dimensions = stream.readline()
        while dimensions.startswith(b"#"):
            dimensions = stream.readline()
        width, height = map(int, dimensions.split())
        if int(stream.readline()) != 255:
            raise RuntimeError("screendump has an unsupported PPM sample range")
        pixels = stream.read()

    if (width, height) != (DISPLAY_WIDTH, DISPLAY_HEIGHT):
        raise RuntimeError(
            f"unexpected screendump geometry {width}x{height}, "
            f"expected {DISPLAY_WIDTH}x{DISPLAY_HEIGHT}"
        )
    expected_size = width * height * 3
    if len(pixels) != expected_size:
        raise RuntimeError(
            f"truncated screendump: {len(pixels)} bytes, expected {expected_size}"
        )
    return width, height, pixels


def validate_pattern(path):
    width, _height, pixels = read_ppm(path)
    samples = (
        (width // 8, (248, 0, 0), "red"),
        (3 * width // 8, (0, 252, 0), "green"),
        (5 * width // 8, (0, 0, 248), "blue"),
        (7 * width // 8, (248, 252, 248), "white"),
    )

    for y in (100, DISPLAY_HEIGHT // 2, DISPLAY_HEIGHT - 100):
        for x, expected, name in samples:
            offset = (y * width + x) * 3
            actual = tuple(pixels[offset : offset + 3])
            if any(abs(got - want) > 7 for got, want in zip(actual, expected)):
                raise RuntimeError(
                    f"{name} band mismatch at ({x}, {y}): "
                    f"got {actual}, expected approximately {expected}"
                )


def main():
    args = parse_args()
    qemu = args.qemu.resolve()
    artifacts = args.artifacts.resolve()
    dtb_name = {
        "raspi4b": "bcm2711-rpi-4-b.dtb",
        "raspi400": "bcm2711-rpi-400.dtb",
    }[args.machine]

    required = (
        qemu,
        artifacts / "Image",
        artifacts / dtb_name,
        artifacts / "initramfs.cpio.gz",
    )
    for path in required:
        if not path.exists():
            raise FileNotFoundError(path)

    kernel_append = " ".join(
        (
            "earlycon=pl011,mmio32,0xfe201000",
            "console=ttyAMA0,115200",
            "rdinit=/init",
            "panic=-1",
            "clk_ignore_unused",
            "ip=dhcp",
            "pi4lab.display_test=1",
        )
    )

    with tempfile.TemporaryDirectory(
        prefix="qemu-pi4-display-", dir="/tmp"
    ) as tmp:
        tmpdir = Path(tmp)
        qmp_path = tmpdir / "qmp.sock"
        screenshot = tmpdir / "scanout.ppm"
        command = (
            str(qemu),
            "-machine",
            args.machine,
            "-kernel",
            str(artifacts / "Image"),
            "-dtb",
            str(artifacts / dtb_name),
            "-initrd",
            str(artifacts / "initramfs.cpio.gz"),
            "-append",
            kernel_append,
            "-nic",
            "user,model=genet",
            "-qmp",
            f"unix:{qmp_path},server=on,wait=off",
            "-display",
            "none",
            "-audio",
            "none",
            "-monitor",
            "none",
            "-serial",
            "stdio",
            "-no-reboot",
        )
        process = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        messages = queue.Queue()
        reader = threading.Thread(
            target=console_reader, args=(process.stdout, messages), daemon=True
        )
        reader.start()
        client = None
        stream = None

        try:
            deadline = time.monotonic() + args.timeout
            client, stream = connect_qmp(qmp_path, process, deadline)
            wait_for_guest(messages, process, max(0.1, deadline - time.monotonic()))
            qmp_command(stream, "screendump", {"filename": str(screenshot)})
            validate_pattern(screenshot)
            print(
                f"PI4-DISPLAY: {args.machine} native 1280x800 "
                "RGB565 scanout verified"
            )
            if args.output:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(screenshot, args.output)
                print(f"PI4-DISPLAY: retained screendump at {args.output}")
            qmp_command(stream, "quit")
            process.wait(timeout=5)
        finally:
            if stream is not None:
                stream.close()
            if client is not None:
                client.close()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, TimeoutError) as error:
        print(f"PI4-DISPLAY: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
