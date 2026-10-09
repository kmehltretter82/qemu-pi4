#!/usr/bin/env python3
"""Differential BCM2711 HVS testing against Raspberry Pi 400 captures.

``capture`` composes planes on a real board through hvs_probe.c and stores
the display list, source bytes and writeback pixels as one JSON file per job.
``replay`` loads such a capture word for word into the emulated HVS over
qtest, takes a screendump and compares it with the hardware pixels.
"""

# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import base64
import hashlib
import json
import random
import shlex
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


HVS_BASE = 0xFE400000
HVS_DLIST = 0x4000
DLIST_WORDS = 4096
MOD_T_TILED = 0x0700000000000001

# fourcc: (kind, bytes per pixel or (hsub, vsub, chroma layout))
RGB_FORMATS = {
    "XR24": 4, "AR24": 4, "XB24": 4, "AB24": 4,
    "RG16": 2, "BG16": 2, "RG24": 3, "BG24": 3,
    "AR15": 2, "XR15": 2, "XR30": 4, "AR30": 4, "XB30": 4, "AB30": 4,
    "RGB8": 1, "BGR8": 1,
}
YUV_FORMATS = {
    # fourcc: (hsub, vsub, planes, swap chroma)
    "YU12": (2, 2, 3, False), "YV12": (2, 2, 3, True),
    "YU16": (2, 1, 3, False), "YV16": (2, 1, 3, True),
    "YU24": (1, 1, 3, False), "YV24": (1, 1, 3, True),
    "NV12": (2, 2, 2, False), "NV21": (2, 2, 2, True),
    "NV16": (2, 1, 2, False), "NV61": (2, 1, 2, True),
}


YUV_HVS_FORMATS = (8, 9, 10, 11, 12, 17)
THREE_PLANE_HVS_FORMATS = (8, 10)
# SCL field -> (horizontal, vertical) scaler
SCL_MODES = (("ppf", "ppf"), ("tpz", "ppf"), ("ppf", "tpz"), ("tpz", "tpz"),
             ("ppf", None), (None, "ppf"), (None, "tpz"), ("tpz", None))


def parse_dlist(words, start):
    """Decode the HVS5 plane entries of the display list at ``start``."""
    planes = []
    index = start
    while index < len(words) and not words[index] & 0x80000000:
        ctl0 = words[index]
        size = (ctl0 >> 24) & 0x3F
        if not ctl0 & 0x40000000 or not size:
            break
        entry = words[index:index + size]
        fmt = ctl0 & 0x1F
        unity = bool(ctl0 & 0x8000)
        yuv = fmt in YUV_HVS_FORMATS
        count = 3 if fmt in THREE_PLANE_HVS_FORMATS else 2 if yuv else 1
        plane = {
            "index": index, "size": size, "ctl0": ctl0, "format": fmt,
            "order": (ctl0 >> 13) & 3, "tiling": (ctl0 >> 20) & 3,
            "unity": unity, "pos0": entry[1], "ctl2": entry[2],
        }
        at = 3
        if not unity:
            plane["pos1"] = entry[at]
            at += 1
        plane["pos2"] = entry[at]
        at += 2
        plane["ptr"] = entry[at:at + count]
        at += 2 * count
        plane["pitch"] = entry[at:at + count]
        at += count
        if yuv:
            plane["csc"] = entry[at:at + 3]
            at += 3
        scl0 = SCL_MODES[(ctl0 >> 5) & 7]
        scl1 = SCL_MODES[(ctl0 >> 8) & 7]
        if yuv:
            channels = [scl0, (None, None) if unity else scl1]
        else:
            channels = [(None, None) if unity else scl0]
        if any(v for _, v in channels):
            plane["lbm"] = entry[at]
            at += 1
        plane["scale"] = []
        for horizontal, vertical in channels:
            channel = {}
            if horizontal == "ppf":
                channel["hppf"] = entry[at]
                at += 1
            if vertical == "ppf":
                channel["vppf"] = entry[at]
                at += 2
            if horizontal == "tpz":
                channel["htpz"] = entry[at:at + 2]
                at += 2
            if vertical == "tpz":
                channel["vtpz"] = entry[at:at + 2]
                at += 3
            plane["scale"].append(channel)
        if any("ppf" in mode for mode in channels):
            plane["kernels"] = entry[at:at + 4]
            at += 4
        plane["words"] = entry
        planes.append(plane)
        index += size
    return planes, index


def sparse_dlist(words, start):
    """Keep the active list and the PPF kernels it points at."""
    planes, end = parse_dlist(words, start)
    segments = {start: words[start:end + 1]}
    for plane in planes:
        for kernel in plane.get("kernels", ()):
            offset = kernel & 0x3FFF
            segments[offset] = words[offset:offset + 11]
    return {str(k): v for k, v in sorted(segments.items())}


def pattern_bytes(pattern, width, height, channels):
    """Return ``channels`` lists of width*height 8-bit samples."""
    kind, _, arg = pattern.partition(":")
    count = width * height
    out = []
    if kind == "rand":
        rng = random.Random(int(arg or 0))
        for _ in range(channels):
            out.append([rng.randrange(256) for _ in range(count)])
    elif kind == "smooth":
        # Band-limited noise: random values on a coarse grid, repeated.
        rng = random.Random(int(arg or 0))
        for _ in range(channels):
            grid = {}
            plane = []
            for y in range(height):
                for x in range(width):
                    key = (x // 3, y // 3)
                    if key not in grid:
                        grid[key] = rng.randrange(256)
                    plane.append(grid[key])
            out.append(plane)
    elif kind == "impulse":
        ix, iy, value, background = (int(v) for v in arg.split(","))
        for _ in range(channels):
            plane = [background] * count
            plane[iy * width + ix] = value
            out.append(plane)
    elif kind == "flat":
        values = [int(v) for v in arg.split(",")]
        for channel in range(channels):
            out.append([values[channel % len(values)]] * count)
    elif kind == "xramp":
        step = int(arg or 1)
        for channel in range(channels):
            out.append([(x * step + channel * 40) & 0xFF
                        for _ in range(height) for x in range(width)])
    elif kind == "yramp":
        step = int(arg or 1)
        for channel in range(channels):
            out.append([(y * step + channel * 40) & 0xFF
                        for y in range(height) for _ in range(width)])
    elif kind == "checker":
        low, high = (int(v) for v in (arg or "0,255").split(","))
        for channel in range(channels):
            out.append([high if (x + y + channel) & 1 else low
                        for y in range(height) for x in range(width)])
    elif kind == "step":
        # Vertical edge at column ``arg``.
        edge = int(arg)
        for channel in range(channels):
            out.append([255 if x >= edge else 0
                        for _ in range(height) for x in range(width)])
    else:
        raise ValueError(f"unknown pattern {pattern}")
    return out


def t_tile_offset(x, y, cpp, tiles_w):
    """Byte offset of a pixel in a VC4 T-tiled buffer."""
    utile_w = 8 if cpp == 2 else 4
    tile_w = utile_w * 8
    tile_x = x // tile_w
    tile_row = y // 32
    utile_x = (x % tile_w) // utile_w
    utile_y = (y % 32) // 4
    subtile = ((utile_y >> 2) << 1) | (utile_x >> 2)
    sub = ((2, 1, 3, 0) if tile_row & 1 else (0, 3, 1, 2))[subtile]
    physical_x = tiles_w - tile_x - 1 if tile_row & 1 else tile_x
    return ((tile_row * tiles_w + physical_x) * 4096 + sub * 1024 +
            ((utile_y & 3) * 4 + (utile_x & 3)) * 64 +
            ((y & 3) * utile_w + x % utile_w) * cpp)


def pack_rgb(fourcc, r, g, b, a):
    if fourcc in ("XR24", "AR24"):
        return bytes((b, g, r, a))
    if fourcc in ("XB24", "AB24"):
        return bytes((r, g, b, a))
    if fourcc == "RG24":
        return bytes((b, g, r))
    if fourcc == "BG24":
        return bytes((r, g, b))
    if fourcc == "RG16":
        return ((r >> 3) << 11 | (g >> 2) << 5 | b >> 3).to_bytes(2, "little")
    if fourcc == "BG16":
        return ((b >> 3) << 11 | (g >> 2) << 5 | r >> 3).to_bytes(2, "little")
    if fourcc in ("AR15", "XR15"):
        return ((a >> 7) << 15 | (r >> 3) << 10 | (g >> 3) << 5 |
                b >> 3).to_bytes(2, "little")
    if fourcc in ("XR30", "AR30"):
        return ((a >> 6) << 30 | (r << 2 | r >> 6) << 20 |
                (g << 2 | g >> 6) << 10 | (b << 2 | b >> 6)).to_bytes(
                    4, "little")
    if fourcc in ("XB30", "AB30"):
        return ((a >> 6) << 30 | (b << 2 | b >> 6) << 20 |
                (g << 2 | g >> 6) << 10 | (r << 2 | r >> 6)).to_bytes(
                    4, "little")
    if fourcc == "RGB8":
        return bytes(((r >> 5) << 5 | (g >> 5) << 2 | b >> 6,))
    if fourcc == "BGR8":
        return bytes(((b >> 6) << 6 | (g >> 5) << 3 | r >> 5,))
    raise ValueError(fourcc)


def build_plane(plane):
    """Fill in size, pitches, offsets and data for one plane description."""
    fourcc = plane["fourcc"]
    width, height = plane["fb"]
    pattern = plane.get("pattern", "rand:1")
    tiled = plane.get("tiled", False)

    if fourcc in RGB_FORMATS:
        cpp = RGB_FORMATS[fourcc]
        r, g, b, a = pattern_bytes(pattern, width, height, 4)
        if tiled:
            tile_w = 64 if cpp == 2 else 32
            tiles_w = -(-width // tile_w)
            tile_rows = -(-height // 32)
            data = bytearray(tiles_w * tile_rows * 4096)
            for y in range(height):
                for x in range(width):
                    i = y * width + x
                    offset = t_tile_offset(x, y, cpp, tiles_w)
                    data[offset:offset + cpp] = pack_rgb(
                        fourcc, r[i], g[i], b[i], a[i])
            pitches = [tiles_w * 128]
            plane["modifier"] = MOD_T_TILED
        else:
            pitch = plane.get("pitch", (width * cpp + 3) & ~3)
            data = bytearray(pitch * height)
            for y in range(height):
                for x in range(width):
                    i = y * width + x
                    offset = y * pitch + x * cpp
                    data[offset:offset + cpp] = pack_rgb(
                        fourcc, r[i], g[i], b[i], a[i])
            pitches = [pitch]
        offsets = [0]
    elif fourcc in YUV_FORMATS:
        hsub, vsub, planes, swap = YUV_FORMATS[fourcc]
        cw, ch = -(-width // hsub), -(-height // vsub)
        (luma,) = pattern_bytes(pattern, width, height, 1)
        cb, cr = pattern_bytes(plane.get("chroma_pattern", pattern),
                               cw, ch, 3)[1:]
        if swap:
            cb, cr = cr, cb
        ypitch = (width + 3) & ~3
        data = bytearray(ypitch * height)
        for y in range(height):
            data[y * ypitch:y * ypitch + width] = bytes(
                luma[y * width:(y + 1) * width])
        if planes == 3:
            cpitch = (cw + 3) & ~3
            offsets = [0, len(data), len(data) + cpitch * ch]
            pitches = [ypitch, cpitch, cpitch]
            for chroma in (cb, cr):
                block = bytearray(cpitch * ch)
                for y in range(ch):
                    block[y * cpitch:y * cpitch + cw] = bytes(
                        chroma[y * cw:(y + 1) * cw])
                data += block
        else:
            cpitch = (cw * 2 + 3) & ~3
            offsets = [0, len(data)]
            pitches = [ypitch, cpitch]
            block = bytearray(cpitch * ch)
            for y in range(ch):
                for x in range(cw):
                    block[y * cpitch + 2 * x] = cb[y * cw + x]
                    block[y * cpitch + 2 * x + 1] = cr[y * cw + x]
            data += block
    else:
        raise ValueError(f"unsupported fourcc {fourcc}")

    plane["size"] = len(data)
    plane["pitches"] = pitches
    plane["offsets"] = offsets
    plane["data"] = base64.b64encode(bytes(data)).decode("ascii")
    return bytes(data)


def fixed(value):
    return int(round(value * 65536))


def job_line(job, data_names):
    tokens = [f"name={job['name']}",
              f"mode={job['mode'][0]}x{job['mode'][1]}"]
    for name, value in job.get("crtc_props", {}).items():
        tokens.append(f"crtc:{name}={value}")
    for index, plane in enumerate(job["planes"]):
        src = plane.get("src", [0, 0, plane["fb"][0], plane["fb"][1]])
        dst = plane["dst"]
        tokens += [
            "plane",
            f"fourcc={plane['fourcc']}",
            f"mod={plane.get('modifier', 0):#x}",
            f"fb={plane['fb'][0]}x{plane['fb'][1]}",
            f"size={plane['size']}",
            "pitches=" + ",".join(str(v) for v in plane["pitches"]),
            "offsets=" + ",".join(str(v) for v in plane["offsets"]),
            f"data={data_names[index]}",
            "src=" + ",".join(str(fixed(v)) for v in src),
            "dst=" + ",".join(str(v) for v in dst),
        ]
        for name, value in plane.get("props", {}).items():
            tokens.append(f"prop:{name.replace(' ', '~')}={value}")
    return " ".join(tokens)


def run(command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def capture(args):
    jobs = json.loads(args.jobs.read_text())
    ssh = ["ssh", "-o", "BatchMode=yes"] + shlex.split(args.ssh_options)
    scp = ["scp", "-q"] + shlex.split(args.ssh_options)
    remote = args.remote_dir
    args.captures.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / "data").mkdir()
        lines = []
        for job in jobs:
            names = []
            for index, plane in enumerate(job["planes"]):
                name = f"{job['name']}.{index}.bin"
                (tmp / "data" / name).write_bytes(build_plane(plane))
                names.append(name)
            lines.append(job_line(job, names))
        (tmp / "data" / "spec.txt").write_text("\n".join(lines) + "\n")
        run(["tar", "-C", str(tmp), "-czf", str(tmp / "job.tgz"), "data"])
        run(ssh + [args.host, f"sudo rm -rf {remote}/data {remote}/out && "
                   f"mkdir -p {remote}"])
        run(scp + [str(tmp / "job.tgz"), f"{args.host}:{remote}/job.tgz"])
        probe = run(ssh + [args.host,
                           f"cd {remote} && tar -xzf job.tgz && "
                           f"sudo {args.probe} data/spec.txt data out; "
                           "sudo chown -R $(id -u) out; "
                           "tar -czf out.tgz out"],
                    capture_output=True, text=True)
        sys.stderr.write(probe.stderr)
        print(probe.stdout.strip())
        run(scp + [f"{args.host}:{remote}/out.tgz", str(tmp / "out.tgz")])
        run(["tar", "-C", str(tmp), "-xzf", str(tmp / "out.tgz")])

        for job in jobs:
            cap = tmp / "out" / f"{job['name']}.cap"
            if not cap.exists():
                print(f"no capture for {job['name']}")
                continue
            result = {"bases": []}
            for line in cap.read_text().splitlines():
                key, _, rest = line.partition(" ")
                if key == "channel":
                    result["channel"] = int(rest)
                elif key == "base":
                    result["bases"].append(int(rest.split()[1], 16))
                elif key in ("regs", "dlist", "out"):
                    result[key] = [int(v, 16) for v in rest.split()]
            start = result["regs"][(0x20 >> 2) + result["channel"]] & 0xFFF
            result["dlist"] = sparse_dlist(result["dlist"], start)
            job["capture"] = result
            (args.captures / f"{job['name']}.json").write_text(
                json.dumps(job, separators=(",", ":")) + "\n")


class Qemu:
    """A raspi4b under qtest with a QMP monitor."""

    def __init__(self, binary, machine="raspi4b"):
        self.tmp = tempfile.TemporaryDirectory()
        tmp = Path(self.tmp.name)
        self.dump = tmp / "dump.ppm"
        self.capture_regions = []
        qtest_path, qmp_path = tmp / "qtest", tmp / "qmp"
        self.process = subprocess.Popen(
            [str(binary), "-machine", machine, "-nic", "none",
             "-display", "none", "-accel", "qtest",
             "-qtest", f"unix:{qtest_path},server=on,wait=off",
             # The qtest log defaults to stderr and would fill the pipe.
             "-qtest-log", "/dev/null",
             "-qmp", f"unix:{qmp_path},server=on,wait=off"],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 30
        self.qtest = self._connect(qtest_path, deadline)
        self.qmp = self._connect(qmp_path, deadline)
        self.qtest_file = self.qtest.makefile("rwb", buffering=0)
        self.qmp_file = self.qmp.makefile("rwb", buffering=0)
        json.loads(self.qmp_file.readline())
        self.qmp_command("qmp_capabilities")

    def _connect(self, path, deadline):
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(self.process.stderr.read().decode())
            client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                client.connect(str(path))
                return client
            except (FileNotFoundError, ConnectionRefusedError):
                client.close()
                time.sleep(0.05)
        raise TimeoutError(f"cannot connect to {path}")

    def command(self, text):
        self.qtest_file.write(text.encode() + b"\n")
        while True:
            reply = self.qtest_file.readline().decode()
            if not reply:
                raise RuntimeError("qtest connection closed")
            if reply.startswith("OK"):
                return reply.split()[1:]
            if reply.startswith(("FAIL", "ERR")):
                raise RuntimeError(f"{text}: {reply}")

    def writel(self, address, value):
        self.command(f"writel {address:#x} {value:#x}")

    def write(self, address, data):
        for start in range(0, len(data), 0x8000):
            chunk = data[start:start + 0x8000]
            encoded = base64.b64encode(chunk).decode()
            self.command(f"b64write {address + start:#x} {len(chunk):#x} "
                         f"{encoded}")

    def qmp_command(self, name, arguments=None):
        request = {"execute": name}
        if arguments:
            request["arguments"] = arguments
        self.qmp_file.write(json.dumps(request).encode() + b"\n")
        while True:
            reply = json.loads(self.qmp_file.readline())
            if "return" in reply:
                return reply["return"]
            if "error" in reply:
                raise RuntimeError(f"{name}: {reply['error']}")

    def screendump(self):
        self.qmp_command("screendump", {"filename": str(self.dump)})
        data = self.dump.read_bytes()
        magic, dimensions, maximum, pixels = data.split(b"\n", 3)
        if magic != b"P6" or maximum != b"255":
            raise RuntimeError("unexpected screendump format")
        width, height = (int(v) for v in dimensions.split())
        if len(pixels) != width * height * 3:
            raise RuntimeError("unexpected screendump length")
        return width, height, pixels

    def close(self):
        try:
            self.qmp_command("quit")
        except (RuntimeError, OSError, ValueError):
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
        self.tmp.cleanup()


def full_dlist(cap):
    words = [0] * DLIST_WORDS
    for offset, segment in cap["dlist"].items():
        words[int(offset):int(offset) + len(segment)] = segment
    return words


def replay_one(qemu, job):
    cap = job["capture"]
    width, height = job["mode"]
    channel = cap["channel"]

    for index in range(3):
        qemu.writel(HVS_BASE + 0x40 + index * 0x10, 0)
    for address, length in qemu.capture_regions:
        qemu.command(f"memset {address:#x} {length:#x} 0")
    qemu.capture_regions = []
    qemu.writel(HVS_BASE + 0x18, channel << 30)
    qemu.writel(HVS_BASE + 0x14, 3 << 30)
    for index, plane in enumerate(job["planes"]):
        data = base64.b64decode(plane["data"])
        address = cap["bases"][index] & 0x3FFFFFFF
        qemu.write(address, data)
        qemu.capture_regions.append((address, len(data)))
    # One bulk write: QEMU splits it into 32-bit display-list accesses.
    words = full_dlist(cap)
    qemu.write(HVS_BASE + HVS_DLIST,
               b"".join(w.to_bytes(4, "little") for w in words))
    regs = cap["regs"]
    qemu.writel(HVS_BASE + 0x44 + channel * 0x10,
                regs[(0x44 + channel * 0x10) >> 2])
    qemu.writel(HVS_BASE + 0x20 + channel * 4, regs[(0x20 >> 2) + channel])
    qemu.writel(HVS_BASE + 0x40 + channel * 0x10,
                regs[(0x40 + channel * 0x10) >> 2])

    got_w, got_h, pixels = qemu.screendump()
    if (got_w, got_h) != (width, height):
        return None, f"size {got_w}x{got_h}, expected {width}x{height}"
    diffs = []
    for index, word in enumerate(cap["out"]):
        want = ((word >> 16) & 0xFF, (word >> 8) & 0xFF, word & 0xFF)
        got = tuple(pixels[index * 3:index * 3 + 3])
        diffs.append(max(abs(a - b) for a, b in zip(want, got)))
    return diffs, None



def dump_job(job, pixels=None):
    """Print the capture, and the emulated pixels when given."""
    cap = job["capture"]
    width, height = job["mode"]
    start = cap["regs"][(0x20 >> 2) + cap["channel"]] & 0xFFF
    for plane in parse_dlist(full_dlist(cap), start)[0]:
        print("  plane:", " ".join(f"{w:08x}" for w in plane["words"]))
    for y in range(min(height, 24)):
        row = cap["out"][y * width:(y + 1) * width][:20]
        print("  hw  ", " ".join(f"{p & 0xffffff:06x}" for p in row))
        if pixels:
            print("  qemu", " ".join(
                pixels[(y * width + x) * 3:(y * width + x) * 3 + 3].hex()
                for x in range(min(width, 20))))


def replay(args):
    files = sorted(args.captures.glob("*.json"))
    if args.only:
        files = [f for f in files if any(o in f.stem for o in args.only)]
    if not files:
        print("no captures")
        return 2
    qemu = Qemu(args.qemu, args.machine)
    failed = 0
    results = []
    try:
        for path in files:
            job = json.loads(path.read_text())
            diffs, error = replay_one(qemu, job)
            if error:
                status = f"ERROR {error}"
                failed += 1
            else:
                worst = max(diffs)
                wrong = sum(1 for d in diffs if d > args.tolerance)
                status = (f"max {worst:3d}  off {wrong:5d}/{len(diffs)}  "
                          f"mean {sum(diffs) / len(diffs):.3f}")
                if wrong:
                    failed += 1
                    status += "  FAIL"
            results.append({
                "capture": str(path.resolve()),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "error": error,
                "max_difference": max(diffs) if diffs else None,
                "mismatched_pixels": sum(d != 0 for d in diffs) if diffs else None,
                "pixels": len(diffs) if diffs else 0,
            })
            print(f"{path.stem:40s} {status}")
            if args.verbose and (error or max(diffs) > args.tolerance):
                dump_job(job, qemu.screendump()[2])
    finally:
        qemu.close()
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({
            "qemu": str(args.qemu.resolve()), "machine": args.machine,
            "qemu_sha256": hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            "tolerance": args.tolerance, "captures": len(files),
            "failed": failed, "results": results,
        }, indent=2) + "\n")
    print(f"{len(files)} captures, {failed} failed")
    return 1 if failed else 0


def show(args):
    for path in sorted(args.captures.glob("*.json")):
        if args.only and not any(o in path.stem for o in args.only):
            continue
        job = json.loads(path.read_text())
        print(path.stem)
        dump_job(job)
    return 0


def main():
    repo_root = Path(__file__).resolve().parent.parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    cap = sub.add_parser("capture", help="capture jobs on a Raspberry Pi")
    cap.add_argument("jobs", type=Path, help="JSON list of jobs")
    cap.add_argument("--host", required=True, help="ssh destination")
    cap.add_argument("--ssh-options", default="")
    cap.add_argument("--remote-dir", default="/tmp/hvsprobe")
    cap.add_argument("--probe", default="$HOME/hvsprobe/hvs_probe")
    cap.add_argument("--captures", type=Path, required=True)
    cap.set_defaults(func=capture)

    rep = sub.add_parser("replay", help="replay captures into QEMU")
    rep.add_argument("--qemu", type=Path,
                     default=repo_root / "build" / "qemu-system-aarch64")
    rep.add_argument("--machine", default="raspi4b")
    rep.add_argument("--captures", type=Path, required=True)
    rep.add_argument("--tolerance", type=int, default=0)
    rep.add_argument("--only", action="append")
    rep.add_argument("--report", type=Path,
                     help="save the per-capture results as JSON")
    rep.add_argument("-v", "--verbose", action="store_true")
    rep.set_defaults(func=replay)

    shw = sub.add_parser("show", help="print captures")
    shw.add_argument("--captures", type=Path, required=True)
    shw.add_argument("--only", action="append")
    shw.set_defaults(func=show)

    args = parser.parse_args()
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
