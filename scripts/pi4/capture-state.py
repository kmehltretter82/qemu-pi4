#!/usr/bin/env python3
"""Capture read-only Linux hardware state for Pi 400/QEMU comparison."""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
from typing import Iterable, Optional


SCHEMA_VERSION = 1
PROC_FILES = (
    "cmdline",
    "cpuinfo",
    "interrupts",
    "iomem",
    "meminfo",
    "modules",
    "version",
)
COMMANDS = {
    "dmesg.txt": ["dmesg"],
    "ip-link.txt": ["ip", "-details", "link", "show"],
    "lspci.txt": ["lspci", "-nn"],
    "lsusb.txt": ["lsusb"],
    "uname.txt": ["uname", "-a"],
}


def write_text(root: Path, relative: str, text: str) -> None:
    destination = root / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(text, encoding="utf-8")


def read_text(path: Path) -> str:
    try:
        return path.read_bytes().decode("utf-8", errors="replace")
    except OSError as error:
        return f"[unavailable: {error}]\n"


def normalized_lines(text: str) -> list[str]:
    return [line.rstrip() for line in text.splitlines() if line.strip()]


def normalize_cpuinfo(text: str) -> str:
    records = [record for record in text.split("\n\n") if record.strip()]
    interesting = {
        "CPU architecture",
        "CPU implementer",
        "CPU part",
        "CPU revision",
        "CPU variant",
        "Features",
        "Hardware",
        "Revision",
        "model name",
    }
    values: dict[str, set[str]] = {}

    for record in records:
        for line in record.splitlines():
            if ":" not in line:
                continue
            key, value = (part.strip() for part in line.split(":", 1))
            if key in interesting:
                values.setdefault(key, set()).add(value)

    logical_cpus = sum("processor" in record for record in records)
    output = [f"logical_cpus={logical_cpus}"]
    for key in sorted(values):
        output.append(f"{key}={' | '.join(sorted(values[key]))}")
    return "\n".join(output) + "\n"


def normalize_iomem(text: str) -> tuple[str, str]:
    mappings = []
    regions = set()
    pattern = re.compile(r"^\s*([0-9a-fA-F]+)-([0-9a-fA-F]+)\s*:\s*(.*)$")

    for line in text.splitlines():
        match = pattern.match(line)
        if not match:
            continue
        start, end, name = match.groups()
        name = name.strip()
        mappings.append(f"{start.lower()}-{end.lower()} : {name}")
        regions.add(name)
    return "\n".join(mappings) + "\n", "\n".join(sorted(regions)) + "\n"


def normalize_interrupts(text: str) -> str:
    sources = set()

    for line in text.splitlines():
        if ":" not in line:
            continue
        irq, details = line.split(":", 1)
        irq = irq.strip()
        if irq == "":
            continue
        fields = details.split()
        while fields and fields[0].isdigit():
            fields.pop(0)
        if fields:
            category = "irq" if irq.isdigit() else irq.lower()
            sources.add(f"{category}: {' '.join(fields)}")
    return "\n".join(sorted(sources)) + "\n"


def list_directory(path: Path) -> str:
    try:
        return "\n".join(sorted(entry.name for entry in path.iterdir())) + "\n"
    except OSError as error:
        return f"[unavailable: {error}]\n"


def decode_dt_property(data: bytes) -> dict[str, object]:
    printable = all(byte == 0 or 32 <= byte < 127 for byte in data)
    if data and printable:
        strings = [
            part.decode("ascii")
            for part in data.rstrip(b"\0").split(b"\0")
        ]
        return {"encoding": "strings", "value": strings}
    return {"encoding": "hex", "value": data.hex()}


def capture_device_tree(root: Path) -> tuple[dict[str, object], str]:
    candidates = (
        root / "sys/firmware/devicetree/base",
        root / "proc/device-tree",
    )
    tree_root = next(
        (candidate for candidate in candidates if candidate.is_dir()),
        None,
    )
    if tree_root is None:
        return {}, "[unavailable: device tree filesystem not mounted]\n"

    properties: dict[str, object] = {}
    for directory, directory_names, filenames in os.walk(tree_root):
        directory_names.sort()
        for filename in sorted(filenames):
            path = Path(directory) / filename
            relative = path.relative_to(tree_root).as_posix()
            try:
                properties[relative] = decode_dt_property(path.read_bytes())
            except OSError as error:
                properties[relative] = {
                    "encoding": "error",
                    "value": str(error),
                }
    return properties, "\n".join(sorted(properties)) + "\n"


def run_command(arguments: list[str]) -> str:
    if shutil.which(arguments[0]) is None:
        return f"[unavailable: command not found: {arguments[0]}]\n"
    try:
        result = subprocess.run(
            arguments,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            timeout=30,
            check=False,
            env={**os.environ, "LC_ALL": "C"},
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return f"[unavailable: {error}]\n"
    suffix = (
        "" if result.returncode == 0
        else f"\n[exit status: {result.returncode}]\n"
    )
    return result.stdout + suffix


def capture(output: Path, label: str, root: Path, skip_commands: bool) -> None:
    if output.exists() and any(output.iterdir()):
        raise ValueError(f"output directory is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    proc: dict[str, str] = {}
    for name in PROC_FILES:
        proc[name] = read_text(root / "proc" / name)
        write_text(output, f"raw/proc/{name}.txt", proc[name])

    write_text(output, "compare/cpu.txt", normalize_cpuinfo(proc["cpuinfo"]))
    mappings, regions = normalize_iomem(proc["iomem"])
    write_text(output, "compare/iomem.txt", mappings)
    write_text(output, "compare/iomem-regions.txt", regions)
    write_text(output, "compare/interrupt-sources.txt",
               normalize_interrupts(proc["interrupts"]))
    write_text(output, "compare/kernel-cmdline.txt",
               "\n".join(normalized_lines(proc["cmdline"])) + "\n")

    properties, property_paths = capture_device_tree(root)
    write_text(output, "raw/device-tree.json",
               json.dumps(properties, indent=2, sort_keys=True) + "\n")
    write_text(output, "compare/device-tree-paths.txt", property_paths)

    platform_devices = list_directory(root / "sys/bus/platform/devices")
    network_interfaces = list_directory(root / "sys/class/net")
    write_text(output, "raw/platform-devices.txt", platform_devices)
    write_text(output, "compare/platform-devices.txt", platform_devices)
    write_text(output, "compare/network-interfaces.txt", network_interfaces)

    command_results: dict[str, str] = {}
    if not skip_commands and root == Path("/"):
        for name, arguments in COMMANDS.items():
            command_results[name] = run_command(arguments)
            write_text(output, f"raw/commands/{name}", command_results[name])
        for name in ("lspci.txt", "lsusb.txt"):
            write_text(output, f"compare/{name}", command_results[name])

    manifest = {
        "capture_time_utc": datetime.now(timezone.utc).isoformat(),
        "kernel_release": platform.release(),
        "label": label,
        "machine": platform.machine(),
        "root": str(root),
        "schema": SCHEMA_VERSION,
    }
    write_text(output, "manifest.json",
               json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main(arguments: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "output", type=Path, help="new or empty capture directory")
    parser.add_argument("--label", default=platform.node() or "unnamed",
                        help="human-readable system label")
    parser.add_argument("--root", type=Path, default=Path("/"),
                        help="alternate proc/sys root, primarily for tests")
    parser.add_argument("--skip-commands", action="store_true",
                        help="capture files only; do not invoke helper "
                             "commands")
    args = parser.parse_args(arguments)

    try:
        capture(args.output, args.label, args.root, args.skip_commands)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Pi 4 state captured in {args.output}")
    print("Review raw/commands/dmesg.txt and device-tree data before sharing "
          "publicly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
