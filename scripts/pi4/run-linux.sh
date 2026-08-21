#!/usr/bin/env bash
# Boot the pinned upstream Pi 4 lab image under qemu-pi4.
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
artifacts_dir=${PI4_LINUX_ARTIFACTS_DIR:-$repo_root/build-pi4-linux/artifacts}
qemu_binary=${QEMU_SYSTEM_AARCH64:-$repo_root/build/qemu-system-aarch64}
extra_append=

usage()
{
    cat <<EOF
Usage: $0 [--qemu PATH] [--artifacts DIR] [--append TEXT]

Boot the Linux artifacts produced by build-linux.sh on the raspi4b machine.
The test initramfs prints hardware state, reports a success marker, and powers
the guest off.  QEMU_SYSTEM_AARCH64 and PI4_LINUX_ARTIFACTS_DIR provide the
same settings through the environment.
EOF
}

while (($#)); do
    case $1 in
    --qemu)
        (($# >= 2)) || { echo "--qemu requires a path" >&2; exit 2; }
        qemu_binary=$2
        shift 2
        ;;
    --artifacts)
        (($# >= 2)) || { echo "--artifacts requires a directory" >&2; exit 2; }
        artifacts_dir=$2
        shift 2
        ;;
    --append)
        (($# >= 2)) || { echo "--append requires text" >&2; exit 2; }
        extra_append=$2
        shift 2
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        echo "unknown option: $1" >&2
        usage >&2
        exit 2
        ;;
    esac
done

[[ -x $qemu_binary ]] || {
    echo "QEMU binary is not executable: $qemu_binary" >&2
    exit 1
}
for artifact in Image bcm2711-rpi-4-b.dtb initramfs.cpio.gz; do
    [[ -f $artifacts_dir/$artifact ]] || {
        echo "missing Linux artifact: $artifacts_dir/$artifact" >&2
        exit 1
    }
done

kernel_append='earlycon=pl011,mmio32,0xfe201000'
kernel_append+=' console=ttyAMA0,115200'
kernel_append+=' rdinit=/init panic=-1 clk_ignore_unused'
[[ -z $extra_append ]] || kernel_append+=" $extra_append"

exec "$qemu_binary" \
    -machine raspi4b \
    -kernel "$artifacts_dir/Image" \
    -dtb "$artifacts_dir/bcm2711-rpi-4-b.dtb" \
    -initrd "$artifacts_dir/initramfs.cpio.gz" \
    -append "$kernel_append" \
    -nographic \
    -no-reboot
