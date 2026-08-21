#!/usr/bin/env python3
"""Compare state captured from a real Pi 400 and qemu-pi4."""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import difflib
from pathlib import Path
import json
import sys
from typing import Iterable, Optional


def load_label(capture: Path) -> str:
    try:
        manifest_text = (capture / "manifest.json").read_text(
            encoding="utf-8")
        manifest = json.loads(manifest_text)
        return str(manifest.get("label", capture.name))
    except (OSError, ValueError, TypeError):
        return capture.name


def collect(capture: Path, include_all: bool) -> dict[str, str]:
    content_root = capture if include_all else capture / "compare"
    if not content_root.is_dir():
        raise ValueError(f"capture directory is incomplete: {content_root}")

    files: dict[str, str] = {}
    for path in sorted(content_root.rglob("*")):
        if not path.is_file() or path.name == "manifest.json":
            continue
        relative = path.relative_to(content_root).as_posix()
        files[relative] = path.read_bytes().decode("utf-8", errors="replace")
    return files


def escaped_table_text(value: str) -> str:
    return value.replace("|", "\\|").replace("\n", " ")


def build_report(left_dir: Path, right_dir: Path, include_all: bool,
                 max_diff_lines: int) -> tuple[str, bool]:
    left_label = load_label(left_dir)
    right_label = load_label(right_dir)
    left = collect(left_dir, include_all)
    right = collect(right_dir, include_all)

    left_names = set(left)
    right_names = set(right)
    common = sorted(left_names & right_names)
    identical = [name for name in common if left[name] == right[name]]
    different = [name for name in common if left[name] != right[name]]
    left_only = sorted(left_names - right_names)
    right_only = sorted(right_names - left_names)
    has_differences = bool(different or left_only or right_only)
    scope = "all captured files" if include_all else "normalized files"

    report = [
        "# Pi 4 hardware-state comparison",
        "",
        f"- Left: `{left_label}` (`{left_dir}`)",
        f"- Right: `{right_label}` (`{right_dir}`)",
        f"- Scope: `{scope}`",
        "",
        "## Summary",
        "",
        "| Result | Count |",
        "| --- | ---: |",
        f"| Identical | {len(identical)} |",
        f"| Different | {len(different)} |",
        f"| Only in {escaped_table_text(left_label)} | {len(left_only)} |",
        f"| Only in {escaped_table_text(right_label)} | {len(right_only)} |",
        "",
    ]

    if left_only or right_only:
        report.extend(("## Capture coverage differences", ""))
        for name in left_only:
            report.append(f"- Only in `{left_label}`: `{name}`")
        for name in right_only:
            report.append(f"- Only in `{right_label}`: `{name}`")
        report.append("")

    if different:
        report.extend(("## Content differences", ""))
        for name in different:
            diff = list(difflib.unified_diff(
                left[name].splitlines(),
                right[name].splitlines(),
                fromfile=f"{left_label}/{name}",
                tofile=f"{right_label}/{name}",
                lineterm="",
            ))
            omitted = max(0, len(diff) - max_diff_lines)
            diff = diff[:max_diff_lines]
            if omitted:
                diff.append(f"... {omitted} diff lines omitted ...")
            report.extend((f"### `{name}`", "", "```diff", *diff, "```", ""))
    else:
        report.extend(("No differences were found in the selected scope.", ""))

    report.extend((
        "Differences are observations, not automatically bugs. Board identity, RAM",
        "size and currently unimplemented devices are expected to differ.",
        "",
    ))
    return "\n".join(report), has_differences


def main(arguments: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "left", type=Path, help="first capture (normally Pi 400)")
    parser.add_argument(
        "right", type=Path, help="second capture (normally QEMU)")
    parser.add_argument("-o", "--output", type=Path,
                        help="write Markdown report instead of stdout")
    parser.add_argument("--all", action="store_true",
                        help="include raw captures instead of normalized files "
                             "only")
    parser.add_argument("--max-diff-lines", type=int, default=200,
                        help="maximum diff lines per file (default: 200)")
    parser.add_argument("--fail-on-difference", action="store_true",
                        help="return status 1 when a difference is found")
    args = parser.parse_args(arguments)

    if args.max_diff_lines < 1:
        parser.error("--max-diff-lines must be positive")
    try:
        report, has_differences = build_report(
            args.left, args.right, args.all, args.max_diff_lines)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
        print(f"Comparison written to {args.output}")
    else:
        print(report, end="")
    return int(args.fail_on_difference and has_differences)


if __name__ == "__main__":
    sys.exit(main())
