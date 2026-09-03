#!/usr/bin/env python3
"""Verify Pi 400 keyboard and USB-mouse events reach the Linux guest."""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import json
import queue
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


SUCCESS_MARKER = "PI4-LAB: upstream Linux boot successful"
KEYBOARD_ENDPOINT_MARKER = "PI4-LAB: Pi 400 keyboard input endpoint /dev/input/event"
CONSUMER_ENDPOINT_MARKER = (
    "PI4-LAB: Pi 400 consumer-control input endpoint /dev/input/event"
)
MOUSE_ENDPOINT_MARKER = "PI4-LAB: QEMU USB mouse input endpoint /dev/input/event"
INPUT_READY_MARKER = "PI4-LAB: Pi 400 input event demo ready"


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
    parser.add_argument("--timeout", type=float, default=40.0)
    return parser.parse_args()


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


def console_reader(stream, messages):
    try:
        for raw_line in iter(stream.readline, b""):
            line = raw_line.decode("utf-8", errors="replace")
            sys.stdout.write(line)
            sys.stdout.flush()
            messages.put(line)
    finally:
        messages.put(None)


def wait_for_markers(messages, process, markers, timeout):
    deadline = time.monotonic() + timeout
    remaining = set(markers)

    while remaining and time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(
                f"QEMU exited before input verification ({process.returncode})"
            )
        try:
            line = messages.get(timeout=min(0.25, deadline - time.monotonic()))
        except queue.Empty:
            continue
        if line is None:
            raise RuntimeError("QEMU console closed before input verification")
        if "PI4-LAB: acceptance failed" in line:
            raise RuntimeError(line.strip())
        remaining = {marker for marker in remaining if marker not in line}

    if remaining:
        raise TimeoutError(
            "guest did not report: " + ", ".join(sorted(remaining))
        )


def send_input(stream, events):
    qmp_command(stream, "input-send-event", {"events": events})


def key_event(key, down):
    return {
        "type": "key",
        "data": {"down": down, "key": {"type": "qcode", "data": key}},
    }


def rel_event(axis, value):
    return {"type": "rel", "data": {"axis": axis, "value": value}}


def button_event(button, down):
    return {"type": "btn", "data": {"button": button, "down": down}}


def verify_consumer_key(stream, messages, process, key, linux_code):
    send_input(stream, (key_event(key, True),))
    wait_for_markers(
        messages,
        process,
        (f"PI4-LAB: consumer input key {linux_code} value 1",),
        5.0,
    )
    send_input(stream, (key_event(key, False),))
    wait_for_markers(
        messages,
        process,
        (f"PI4-LAB: consumer input key {linux_code} value 0",),
        5.0,
    )


def main():
    args = parse_args()
    qemu = args.qemu.resolve()
    artifacts = args.artifacts.resolve()
    required = (
        qemu,
        artifacts / "Image",
        artifacts / "bcm2711-rpi-400.dtb",
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
            "pi4lab.input_demo=1",
        )
    )

    with tempfile.TemporaryDirectory(prefix="qemu-pi4-input-", dir="/tmp") as tmp:
        qmp_path = Path(tmp) / "qmp.sock"
        command = (
            str(qemu),
            "-machine",
            "raspi400",
            "-kernel",
            str(artifacts / "Image"),
            "-dtb",
            str(artifacts / "bcm2711-rpi-400.dtb"),
            "-initrd",
            str(artifacts / "initramfs.cpio.gz"),
            "-append",
            kernel_append,
            "-nic",
            "user,model=genet",
            "-device",
            "usb-mouse,bus=vl805.0,port=1.1",
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
            wait_for_markers(
                messages,
                process,
                (
                    SUCCESS_MARKER,
                    KEYBOARD_ENDPOINT_MARKER,
                    CONSUMER_ENDPOINT_MARKER,
                    MOUSE_ENDPOINT_MARKER,
                    INPUT_READY_MARKER,
                ),
                max(0.1, deadline - time.monotonic()),
            )

            send_input(stream, (key_event("a", True),))
            wait_for_markers(
                messages,
                process,
                ("PI4-LAB: keyboard input key 30 value 1",),
                5.0,
            )
            send_input(stream, (key_event("a", False),))
            wait_for_markers(
                messages,
                process,
                ("PI4-LAB: keyboard input key 30 value 0",),
                5.0,
            )

            verify_consumer_key(stream, messages, process, "audioplay", 164)
            verify_consumer_key(stream, messages, process, "volumeup", 115)
            verify_consumer_key(stream, messages, process, "calculator", 140)

            send_input(stream, (rel_event("x", 17), rel_event("y", -9)))
            wait_for_markers(
                messages,
                process,
                (
                    "PI4-LAB: mouse input rel 0 value 17",
                    "PI4-LAB: mouse input rel 1 value -9",
                ),
                5.0,
            )
            send_input(stream, (button_event("left", True),))
            wait_for_markers(
                messages,
                process,
                ("PI4-LAB: mouse input key 272 value 1",),
                5.0,
            )
            send_input(stream, (button_event("left", False),))
            wait_for_markers(
                messages,
                process,
                ("PI4-LAB: mouse input key 272 value 0",),
                5.0,
            )

            print("PI4-INPUT: Pi 400 keyboard, consumer keys, and USB mouse events verified")
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
        print(f"PI4-INPUT: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
