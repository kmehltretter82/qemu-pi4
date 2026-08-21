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
    def test_shell_scripts_parse(self):
        subprocess.run([
            "bash", "-n",
            str(PI4_SCRIPTS / "build-linux.sh"),
            str(PI4_SCRIPTS / "run-linux.sh"),
        ], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
