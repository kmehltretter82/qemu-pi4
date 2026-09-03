#!/usr/bin/env python3
"""Tests for the qemu-pi4 upstream-kernel and differential lab tools."""

# SPDX-License-Identifier: GPL-2.0-or-later

import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
PI4_SCRIPTS = REPO_ROOT / "scripts" / "pi4"

AUDIO_SPEC = importlib.util.spec_from_file_location(
    "pi4_test_audio", PI4_SCRIPTS / "test-audio.py")
AUDIO_TOOLS = importlib.util.module_from_spec(AUDIO_SPEC)
AUDIO_SPEC.loader.exec_module(AUDIO_TOOLS)

DISPLAY_SPEC = importlib.util.spec_from_file_location(
    "pi4_test_display", PI4_SCRIPTS / "test-display.py")
DISPLAY_TOOLS = importlib.util.module_from_spec(DISPLAY_SPEC)
DISPLAY_SPEC.loader.exec_module(DISPLAY_TOOLS)

INPUT_SPEC = importlib.util.spec_from_file_location(
    "pi4_test_input", PI4_SCRIPTS / "test-input.py")
INPUT_TOOLS = importlib.util.module_from_spec(INPUT_SPEC)
INPUT_SPEC.loader.exec_module(INPUT_TOOLS)

V3D_PROBE_SPEC = importlib.util.spec_from_file_location(
    "pi4_test_v3d_probe", PI4_SCRIPTS / "test-v3d-probe.py")
V3D_PROBE_TOOLS = importlib.util.module_from_spec(V3D_PROBE_SPEC)
V3D_PROBE_SPEC.loader.exec_module(V3D_PROBE_TOOLS)

AUX_SPI_SPEC = importlib.util.spec_from_file_location(
    "pi4_test_aux_spi", PI4_SCRIPTS / "test-aux-spi.py")
AUX_SPI_TOOLS = importlib.util.module_from_spec(AUX_SPI_SPEC)
AUX_SPI_SPEC.loader.exec_module(AUX_SPI_TOOLS)


class InitramfsTests(unittest.TestCase):
    def test_initramfs_is_deterministic(self):
        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            init = temporary / "init"
            first = temporary / "first.cpio.gz"
            second = temporary / "second.cpio.gz"
            init.write_bytes(b"test-static-init\n")

            command = [
                sys.executable,
                str(PI4_SCRIPTS / "mkinitramfs.py"),
                "--mtime", "1234",
                str(init),
            ]
            subprocess.run([*command, str(first)], check=True)
            subprocess.run([*command, str(second)], check=True)

            self.assertEqual(hashlib.sha256(first.read_bytes()).digest(),
                             hashlib.sha256(second.read_bytes()).digest())
            self.assertEqual(first.stat().st_mode & 0o777, 0o644)
            archive = gzip.decompress(first.read_bytes())
            self.assertTrue(archive.startswith(b"070701"))
            self.assertIn(b"dev/console\0", archive)
            self.assertIn(b"dev/null\0", archive)
            self.assertIn(b"init\0", archive)
            self.assertIn(b"TRAILER!!!\0", archive)


class HardwareBootTests(unittest.TestCase):
    def test_tryboot_template_uses_return_initramfs(self):
        tryboot = (PI4_SCRIPTS / "tryboot.txt").read_text(encoding="utf-8")
        cmdline = (PI4_SCRIPTS / "cmdline-hardware.txt").read_text(
            encoding="utf-8")

        self.assertIn("kernel=Image-qemu-pi4\n", tryboot)
        self.assertIn(
            "device_tree=bcm2711-rpi-400-qemu-pi4.dtb\n", tryboot)
        self.assertIn(
            "initramfs initramfs-qemu-pi4-hardware.cpio.gz followkernel\n",
            tryboot)
        self.assertIn("cmdline=cmdline-qemu-pi4-hardware.txt\n", tryboot)
        self.assertEqual(len(cmdline.splitlines()), 1)
        self.assertIn("rdinit=/init", cmdline)
        self.assertIn("panic=10", cmdline)


class CaptureTests(unittest.TestCase):
    def test_capture_from_fixture_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            root = temporary / "root"
            output = temporary / "capture"
            (root / "proc").mkdir(parents=True)
            (root / "sys/bus/platform/devices").mkdir(parents=True)
            (root / "sys/class/net/eth0").mkdir(parents=True)
            device_tree = root / "sys/firmware/devicetree/base"
            device_tree.mkdir(parents=True)

            proc_files = {
                "cmdline": "console=ttyAMA0\n",
                "cpuinfo": (
                    "processor : 0\nCPU implementer : 0x41\nCPU part : 0xd08\n\n"
                    "processor : 1\nCPU implementer : 0x41\nCPU part : 0xd08\n"
                ),
                "interrupts": (
                    "           CPU0 CPU1\n"
                    " 32:       1    2 GICv2 99 Level arch_timer\n"
                ),
                "iomem": (
                    "fe000000-feffffff : soc\n"
                    "  fe201000-fe201fff : serial\n"
                ),
                "meminfo": "MemTotal: 2048000 kB\n",
                "modules": "",
                "version": "Linux version test\n",
            }
            for name, content in proc_files.items():
                (root / "proc" / name).write_text(content, encoding="utf-8")
            (root / "sys/bus/platform/devices/fe201000.serial").mkdir()
            (device_tree / "compatible").write_bytes(b"raspberrypi,4-model-b\0")

            subprocess.run([
                sys.executable,
                str(PI4_SCRIPTS / "capture-state.py"),
                "--root", str(root),
                "--skip-commands",
                "--label", "fixture",
                str(output),
            ], check=True)

            cpu = (output / "compare/cpu.txt").read_text(encoding="utf-8")
            self.assertIn("logical_cpus=2", cpu)
            self.assertIn("CPU part=0xd08", cpu)
            self.assertEqual(
                (output / "compare/device-tree-paths.txt").read_text(
                    encoding="utf-8"),
                "compatible\n",
            )
            manifest = json.loads((output / "manifest.json").read_text(
                encoding="utf-8"))
            self.assertEqual(manifest["label"], "fixture")


class ComparisonTests(unittest.TestCase):
    def make_capture(self, path, label, values):
        (path / "compare").mkdir(parents=True)
        (path / "manifest.json").write_text(
            json.dumps({"label": label, "schema": 1}), encoding="utf-8")
        for name, value in values.items():
            destination = path / "compare" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(value, encoding="utf-8")

    def test_compare_reports_differences_without_failing_by_default(self):
        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            left = temporary / "pi400"
            right = temporary / "qemu"
            self.make_capture(left, "Pi 400", {
                "cpu.txt": "logical_cpus=4\n",
                "platform-devices.txt": "serial\nv3d\n",
            })
            self.make_capture(right, "QEMU", {
                "cpu.txt": "logical_cpus=4\n",
                "platform-devices.txt": "serial\n",
            })
            command = [
                sys.executable,
                str(PI4_SCRIPTS / "compare-state.py"),
                str(left), str(right),
            ]
            result = subprocess.run(command, check=True, text=True,
                                    stdout=subprocess.PIPE)
            self.assertIn("| Identical | 1 |", result.stdout)
            self.assertIn("| Different | 1 |", result.stdout)
            self.assertIn("-v3d", result.stdout)

            result = subprocess.run([*command, "--fail-on-difference"],
                                    check=False, stdout=subprocess.PIPE)
            self.assertEqual(result.returncode, 1)


class AudioCaptureTests(unittest.TestCase):
    def write_capture(self, path, silent_frame=None, zero_lengths=False):
        values = [0, 0] * 128
        for frame in range(AUDIO_TOOLS.AUDIO_FRAMES):
            left = 16383 if frame % 48 < 24 else -16383
            right = 8191 if frame % 24 < 12 else -8191
            if frame == silent_frame:
                left = right = 0
            values.extend((left, right))
        values.extend([0, 0] * 64)
        payload = struct.pack(f"<{len(values)}h", *values)
        wave_format = struct.pack(
            "<HHIIHH", 1, 2, 48000, 192000, 4, 16)
        riff_length = 0 if zero_lengths else len(payload) + 36
        data_length = 0 if zero_lengths else len(payload)
        path.write_bytes(
            b"RIFF" + struct.pack("<I", riff_length) + b"WAVE" +
            b"fmt " + struct.pack("<I", len(wave_format)) + wave_format +
            b"data" + struct.pack("<I", data_length) + payload)

    def test_hdmi_audio_validator_accepts_expected_stereo_tones(self):
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.wav"
            self.write_capture(capture, zero_lengths=True)

            AUDIO_TOOLS.repair_qemu_wav_lengths(capture)
            result = AUDIO_TOOLS.validate_wav(capture)

            self.assertEqual(result["active_frames"], 48000)
            self.assertAlmostEqual(result["left_frequency"], 1000,
                                   delta=1)
            self.assertAlmostEqual(result["right_frequency"], 2000,
                                   delta=1)
            blob = capture.read_bytes()
            self.assertEqual(struct.unpack_from("<I", blob, 4)[0],
                             len(blob) - 8)
            self.assertEqual(struct.unpack_from("<I", blob, 40)[0],
                             len(blob) - 44)

    def test_hdmi_audio_validator_rejects_internal_silence(self):
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.wav"
            self.write_capture(capture, silent_frame=24000)

            with self.assertRaisesRegex(RuntimeError, "silent frames"):
                AUDIO_TOOLS.validate_wav(capture)


class DisplayCaptureTests(unittest.TestCase):
    def write_composited_capture(self, path):
        width = DISPLAY_TOOLS.DISPLAY_WIDTH
        height = DISPLAY_TOOLS.DISPLAY_HEIGHT
        primary = ((248, 0, 0), (0, 252, 0),
                   (0, 0, 248), (248, 252, 248))
        overlay = ((0, 252, 0), (0, 0, 248),
                   (248, 252, 248), (0, 0, 0))
        row = bytearray()

        for x in range(width):
            row.extend(primary[(x * 4) // width])
        pixels = bytearray(row * height)
        for y in range(200, 600):
            for x in range(320, 960):
                quadrant = (2 if y >= 400 else 0) + (x >= 640)
                offset = (y * width + x) * 3
                pixels[offset:offset + 3] = bytes(overlay[quadrant])
        path.write_bytes(
            f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)

    def test_display_validator_accepts_scaled_overlay(self):
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.ppm"

            self.write_composited_capture(capture)
            DISPLAY_TOOLS.validate_pattern(capture)

    def test_display_validator_rejects_bad_overlay_pixel(self):
        with tempfile.TemporaryDirectory() as temporary:
            capture = Path(temporary) / "capture.ppm"

            self.write_composited_capture(capture)
            contents = bytearray(capture.read_bytes())
            header_size = contents.index(b"255\n") + len(b"255\n")
            offset = header_size + (300 * DISPLAY_TOOLS.DISPLAY_WIDTH + 480) * 3
            contents[offset:offset + 3] = b"\x00\x00\x00"
            capture.write_bytes(contents)
            with self.assertRaisesRegex(RuntimeError,
                                         "green overlay quadrant mismatch"):
                DISPLAY_TOOLS.validate_pattern(capture)


class InputToolTests(unittest.TestCase):
    def test_input_event_payloads(self):
        self.assertEqual(
            INPUT_TOOLS.key_event("a", True),
            {
                "type": "key",
                "data": {
                    "down": True,
                    "key": {"type": "qcode", "data": "a"},
                },
            },
        )
        self.assertEqual(
            INPUT_TOOLS.rel_event("x", 17),
            {"type": "rel", "data": {"axis": "x", "value": 17}},
        )
        self.assertEqual(
            INPUT_TOOLS.button_event("left", False),
            {"type": "btn", "data": {"button": "left", "down": False}},
        )
        self.assertEqual(
            INPUT_TOOLS.CONSUMER_ENDPOINT_MARKER,
            "PI4-LAB: Pi 400 consumer-control input endpoint /dev/input/event",
        )


class V3DProbeToolTests(unittest.TestCase):
    def test_v3d_probe_validator_accepts_driver_registration(self):
        output = "\n".join((
            V3D_PROBE_TOOLS.V3D_DRIVER_MARKER,
            V3D_PROBE_TOOLS.V3D_HUB_MAPPING_MARKER,
            V3D_PROBE_TOOLS.V3D_CORE_MAPPING_MARKER,
            V3D_PROBE_TOOLS.SUCCESS_MARKER,
        ))

        V3D_PROBE_TOOLS.validate_output(output)

    def test_v3d_probe_validator_rejects_asb_failure(self):
        output = "\n".join((
            V3D_PROBE_TOOLS.V3D_DRIVER_MARKER,
            V3D_PROBE_TOOLS.V3D_HUB_MAPPING_MARKER,
            V3D_PROBE_TOOLS.V3D_CORE_MAPPING_MARKER,
            V3D_PROBE_TOOLS.SUCCESS_MARKER,
            V3D_PROBE_TOOLS.ASB_FAILURE_MARKERS[0],
        ))

        with self.assertRaisesRegex(RuntimeError, "bridge failure"):
            V3D_PROBE_TOOLS.validate_output(output)


class AuxSpiToolTests(unittest.TestCase):
    def test_aux_spi_validator_accepts_driver_and_flash_read(self):
        output = "\n".join((
            AUX_SPI_TOOLS.AUX_SPI_CHECK_MARKER,
            AUX_SPI_TOOLS.AUX_SPI_READ_MARKER,
            AUX_SPI_TOOLS.SUCCESS_MARKER,
        ))

        AUX_SPI_TOOLS.validate_output(output)

    def test_aux_spi_validator_rejects_missing_flash_read(self):
        output = "\n".join((
            AUX_SPI_TOOLS.AUX_SPI_CHECK_MARKER,
            AUX_SPI_TOOLS.SUCCESS_MARKER,
        ))

        with self.assertRaisesRegex(RuntimeError, "erased read"):
            AUX_SPI_TOOLS.validate_output(output)


class ShellScriptTests(unittest.TestCase):
    LINUX_SUCCESS = "PI4-LAB: upstream Linux boot successful"
    AON_REGISTERED = (
        "irq_brcmstb_l2: registered L2 intc "
        "(/soc/interrupt-controller@7ef00100, parent irq: 14)"
    )
    DDC_CHECKED = "PI4-LAB: HDMI0 DDC advertises HDMI stereo audio ok"
    DRM_CARD_CHECKED = "PI4-LAB: VC4 DRM card0 registered ok"
    DRM_FB_CHECKED = (
        "PI4-LAB: VC4 DRM framebuffer is 1280x800 RGB565 ok"
    )

    def successful_linux_output(self):
        return "\n".join((
            self.LINUX_SUCCESS,
            self.AON_REGISTERED,
            self.DDC_CHECKED,
            self.DRM_CARD_CHECKED,
            self.DRM_FB_CHECKED,
        ))

    def test_shell_scripts_parse(self):
        subprocess.run([
            "bash", "-n",
            str(PI4_SCRIPTS / "build-linux.sh"),
            str(PI4_SCRIPTS / "run-linux.sh"),
        ], check=True)

    def run_linux_with_fake_qemu(self, output, qemu_status=0):
        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            artifacts = temporary / "artifacts"
            artifacts.mkdir()
            for name in ("Image", "bcm2711-rpi-4-b.dtb",
                         "initramfs.cpio.gz"):
                (artifacts / name).touch()

            qemu = temporary / "qemu-system-aarch64"
            qemu.write_text(
                "#!/bin/sh\n"
                f"printf '%s\\n' {output!r}\n"
                f"exit {qemu_status}\n",
                encoding="utf-8",
            )
            qemu.chmod(0o755)

            return subprocess.run([
                "bash", str(PI4_SCRIPTS / "run-linux.sh"),
                "--qemu", str(qemu),
                "--artifacts", str(artifacts),
                "--machine", "raspi4b",
            ], check=False, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)

    def test_linux_runner_requires_success_marker(self):
        result = self.run_linux_with_fake_qemu(
            self.AON_REGISTERED)

        self.assertEqual(result.returncode, 1)
        self.assertIn("acceptance marker was not produced", result.stderr)

    def test_linux_runner_requires_aon_registration(self):
        result = self.run_linux_with_fake_qemu(self.LINUX_SUCCESS)

        self.assertEqual(result.returncode, 1)
        self.assertIn("AON L2 interrupt controller was not registered",
                      result.stderr)

    def test_linux_runner_accepts_success_marker(self):
        result = self.run_linux_with_fake_qemu(self.successful_linux_output())

        self.assertEqual(result.returncode, 0)

    def test_linux_runner_requires_display_foundation_checks(self):
        result = self.run_linux_with_fake_qemu(
            f"{self.LINUX_SUCCESS}\n{self.AON_REGISTERED}")

        self.assertEqual(result.returncode, 1)
        self.assertIn("DVP/DDC/EDID acceptance checks were not run",
                      result.stderr)

    def test_linux_runner_requires_native_display_checks(self):
        result = self.run_linux_with_fake_qemu(
            f"{self.LINUX_SUCCESS}\n{self.AON_REGISTERED}\n{self.DDC_CHECKED}")

        self.assertEqual(result.returncode, 1)
        self.assertIn("native VC4 DRM acceptance checks were not run",
                      result.stderr)

    def test_linux_runner_propagates_qemu_failure(self):
        result = self.run_linux_with_fake_qemu(
            self.successful_linux_output(),
            qemu_status=7)

        self.assertEqual(result.returncode, 1)
        self.assertIn("QEMU exited before completing", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
