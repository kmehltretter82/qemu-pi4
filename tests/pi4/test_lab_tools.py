#!/usr/bin/env python3
"""Tests for the qemu-pi4 upstream-kernel and differential lab tools."""

# SPDX-License-Identifier: GPL-2.0-or-later

import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
PI4_SCRIPTS = REPO_ROOT / "scripts" / "pi4"


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


class ShellScriptTests(unittest.TestCase):
    LINUX_SUCCESS = "PI4-LAB: upstream Linux boot successful"
    AON_REGISTERED = (
        "irq_brcmstb_l2: registered L2 intc "
        "(/soc/interrupt-controller@7ef00100, parent irq: 14)"
    )
    DDC_CHECKED = "PI4-LAB: HDMI0 DDC reads a valid 128-byte EDID ok"
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
