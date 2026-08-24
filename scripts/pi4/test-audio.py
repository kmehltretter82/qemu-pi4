#!/usr/bin/env python3
"""Validate Linux-driven BCM2711 HDMI0 audio through a QEMU WAV capture."""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import json
import math
import queue
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


AUDIO_RATE = 48000
AUDIO_CHANNELS = 2
AUDIO_BITS = 16
AUDIO_FRAMES = AUDIO_RATE
AUDIO_MARKER = "PI4-LAB: HDMI0 48 kHz stereo MAI/DMA playback"
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
        help="retain the validated WAV capture at this path",
    )
    parser.add_argument("--timeout", type=float, default=30.0)
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


def wait_for_guest(messages, process, timeout):
    deadline = time.monotonic() + timeout
    audio_ok = False
    success = False

    while time.monotonic() < deadline and not (audio_ok and success):
        if process.poll() is not None:
            raise RuntimeError(
                "QEMU exited before HDMI audio readiness "
                f"({process.returncode})"
            )
        try:
            remaining = max(0.001, deadline - time.monotonic())
            line = messages.get(timeout=min(0.25, remaining))
        except queue.Empty:
            continue
        if line is None:
            raise RuntimeError(
                "QEMU console closed before HDMI audio readiness"
            )
        if AUDIO_MARKER in line:
            if line.rstrip().endswith("FAIL"):
                raise RuntimeError("guest HDMI audio playback failed")
            if line.rstrip().endswith("ok"):
                audio_ok = True
        if SUCCESS_MARKER in line:
            success = True

    if not audio_ok:
        raise TimeoutError("guest did not complete HDMI audio playback")
    if not success:
        raise RuntimeError(
            "guest HDMI audio run failed another acceptance check"
        )


def repair_qemu_wav_lengths(path):
    blob = bytearray(path.read_bytes())
    if (len(blob) < 44 or blob[:4] != b"RIFF" or blob[8:12] != b"WAVE" or
            blob[12:16] != b"fmt " or blob[36:40] != b"data"):
        return
    if struct.unpack_from("<I", blob, 4)[0] == 0:
        struct.pack_into("<I", blob, 4, len(blob) - 8)
    if struct.unpack_from("<I", blob, 40)[0] == 0:
        struct.pack_into("<I", blob, 40, len(blob) - 44)
    path.write_bytes(blob)


def wav_payload(path):
    blob = path.read_bytes()
    if len(blob) < 12 or blob[:4] != b"RIFF" or blob[8:12] != b"WAVE":
        raise RuntimeError("capture is not a RIFF/WAVE file")

    wave_format = None
    samples = None
    offset = 12
    while offset + 8 <= len(blob):
        chunk = blob[offset:offset + 4]
        size = struct.unpack_from("<I", blob, offset + 4)[0]
        start = offset + 8
        end = start + size

        if chunk == b"data" and size == 0:
            # QEMU's WAV backend can leave lengths zero at process exit.
            end = len(blob)
        if end > len(blob):
            raise RuntimeError(f"truncated {chunk!r} WAV chunk")
        if chunk == b"fmt ":
            if size < 16:
                raise RuntimeError("truncated WAV format chunk")
            wave_format = struct.unpack_from("<HHIIHH", blob, start)
        elif chunk == b"data":
            samples = blob[start:end]
            break
        offset = end + (size & 1)

    if wave_format is None or samples is None:
        raise RuntimeError("capture lacks WAV format or sample data")
    expected_format = (
        1,
        AUDIO_CHANNELS,
        AUDIO_RATE,
        AUDIO_RATE * AUDIO_CHANNELS * AUDIO_BITS // 8,
        AUDIO_CHANNELS * AUDIO_BITS // 8,
        AUDIO_BITS,
    )
    if wave_format != expected_format:
        raise RuntimeError(
            f"unexpected WAV format {wave_format}, expected {expected_format}"
        )
    if len(samples) % (AUDIO_CHANNELS * AUDIO_BITS // 8):
        raise RuntimeError("WAV sample data ends inside a stereo frame")
    return samples


def channel_frequency(samples):
    crossings = sum(
        (first < 0) != (second < 0)
        for first, second in zip(samples, samples[1:])
    )
    return crossings * AUDIO_RATE / (2 * (len(samples) - 1))


def channel_rms(samples):
    return math.sqrt(sum(sample * sample for sample in samples) / len(samples))


def validate_wav(path):
    payload = wav_payload(path)
    values = struct.unpack(f"<{len(payload) // 2}h", payload)
    left = values[0::2]
    right = values[1::2]
    frame_count = len(left)

    start = next(
        (frame for frame, pair in enumerate(zip(left, right)) if any(pair)),
        None,
    )
    if start is None:
        raise RuntimeError("WAV capture contains only silence")
    end = frame_count
    while end > start and left[end - 1] == 0 and right[end - 1] == 0:
        end -= 1
    active_frames = end - start
    if start > 4096:
        raise RuntimeError(f"excessive leading silence: {start} frames")
    if abs(active_frames - AUDIO_FRAMES) > 1024:
        raise RuntimeError(
            f"active span is {active_frames} frames, expected approximately "
            f"{AUDIO_FRAMES}"
        )

    left_active = left[start:end]
    right_active = right[start:end]
    silent = sum(
        left_sample == 0 and right_sample == 0
        for left_sample, right_sample in zip(left_active, right_active)
    )
    if silent:
        raise RuntimeError(f"active stream contains {silent} silent frames")

    left_min, left_max = min(left_active), max(left_active)
    right_min, right_max = min(right_active), max(right_active)
    if not (-17000 <= left_min <= -15500 and 15500 <= left_max <= 17000):
        raise RuntimeError(
            f"unexpected left-channel range {left_min}..{left_max}"
        )
    if not (-9000 <= right_min <= -7500 and 7500 <= right_max <= 9000):
        raise RuntimeError(
            f"unexpected right-channel range {right_min}..{right_max}"
        )

    left_frequency = channel_frequency(left_active)
    right_frequency = channel_frequency(right_active)
    if abs(left_frequency - 1000) > 25:
        raise RuntimeError(f"left frequency is {left_frequency:.2f} Hz")
    if abs(right_frequency - 2000) > 40:
        raise RuntimeError(f"right frequency is {right_frequency:.2f} Hz")

    left_rms = channel_rms(left_active)
    right_rms = channel_rms(right_active)
    ratio = left_rms / right_rms
    if not 1.98 <= ratio <= 2.02:
        raise RuntimeError(f"left/right RMS ratio is {ratio:.4f}")

    return {
        "total_frames": frame_count,
        "leading_silence": start,
        "active_frames": active_frames,
        "left_frequency": left_frequency,
        "right_frequency": right_frequency,
        "rms_ratio": ratio,
    }


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
            "pi4lab.hdmi_audio_test=1",
        )
    )

    with tempfile.TemporaryDirectory(
        prefix="qemu-pi4-hdmi-audio-", dir="/tmp"
    ) as temporary:
        temporary = Path(temporary)
        qmp_path = temporary / "qmp.sock"
        capture = temporary / "hdmi.wav"
        command = (
            str(qemu),
            "-machine", args.machine,
            "-kernel", str(artifacts / "Image"),
            "-dtb", str(artifacts / dtb_name),
            "-initrd", str(artifacts / "initramfs.cpio.gz"),
            "-append", kernel_append,
            "-nic", "user,model=genet",
            "-qmp", f"unix:{qmp_path},server=on,wait=off",
            "-display", "none",
            "-monitor", "none",
            "-serial", "stdio",
            "-no-reboot",
            "-audiodev",
            f"wav,id=hdmi,path={capture},out.frequency={AUDIO_RATE},"
            "out.channels=2,out.format=s16",
            "-audiodev", "none,id=silent",
            "-global", "bcm2711-hdmi.audiodev=hdmi",
            "-global", "bcm2835-i2s.audiodev=silent",
            "-global", "bcm2835-pwm.audiodev=silent",
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
            wait_for_guest(messages, process,
                           max(0.1, deadline - time.monotonic()))
            qmp_command(stream, "quit")
            returncode = process.wait(timeout=5)
            if returncode:
                raise RuntimeError(
                    f"QEMU exited after HDMI playback ({returncode})"
                )
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

        repair_qemu_wav_lengths(capture)
        result = validate_wav(capture)
        print(
            f"PI4-AUDIO: {args.machine} HDMI0 captured "
            f"{result['active_frames']} active frames; "
            f"left {result['left_frequency']:.2f} Hz, "
            f"right {result['right_frequency']:.2f} Hz, "
            f"RMS ratio {result['rms_ratio']:.4f}"
        )
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(capture, args.output)
            print(f"PI4-AUDIO: retained capture at {args.output}")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, TimeoutError,
            subprocess.TimeoutExpired) as error:
        print(f"PI4-AUDIO: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
