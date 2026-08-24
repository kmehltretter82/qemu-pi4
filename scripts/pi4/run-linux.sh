#!/usr/bin/env bash
# Boot the pinned upstream Pi 4 lab image under qemu-pi4.
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
artifacts_dir=${PI4_LINUX_ARTIFACTS_DIR:-$repo_root/build-pi4-linux/artifacts}
qemu_binary=${QEMU_SYSTEM_AARCH64:-$repo_root/build/qemu-system-aarch64}
machine=raspi4b
extra_append=

usage()
{
    cat <<EOF
Usage: $0 [--qemu PATH] [--artifacts DIR] [--machine MODEL] [--append TEXT]

Boot the Linux artifacts produced by build-linux.sh on raspi4b or raspi400.
The test initramfs prints hardware state, verifies data transfers to a
disposable USB mass-storage device, exercises GENET using DHCP, reports a
success marker, and powers the guest off.  QEMU_SYSTEM_AARCH64 and
PI4_LINUX_ARTIFACTS_DIR provide the same settings through the environment.
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
    --machine)
        (($# >= 2)) || { echo "--machine requires a model" >&2; exit 2; }
        machine=$2
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

case $machine in
raspi4b)
    dtb_name=bcm2711-rpi-4-b.dtb
    ;;
raspi400)
    dtb_name=bcm2711-rpi-400.dtb
    ;;
*)
    echo "unsupported Pi 4 machine: $machine" >&2
    exit 2
    ;;
esac

[[ -x $qemu_binary ]] || {
    echo "QEMU binary is not executable: $qemu_binary" >&2
    exit 1
}
for artifact in Image "$dtb_name" initramfs.cpio.gz; do
    [[ -f $artifacts_dir/$artifact ]] || {
        echo "missing Linux artifact: $artifacts_dir/$artifact" >&2
        exit 1
    }
done

usb_storage_image=$(mktemp \
    "${TMPDIR:-/tmp}/qemu-pi4-usb-storage.XXXXXX")
qemu_log=
cleanup()
{
    rm -f -- "$usb_storage_image"
    [[ -z $qemu_log ]] || rm -f -- "$qemu_log"
}
trap cleanup EXIT
qemu_log=$(mktemp "${TMPDIR:-/tmp}/qemu-pi4-linux-log.XXXXXX")

# Leave an 8 MiB sparse raw image.  The guest writes only this disposable file.
dd if=/dev/zero of="$usb_storage_image" bs=1 count=1 \
    seek=$((8 * 1024 * 1024 - 1)) 2>/dev/null

kernel_append='earlycon=pl011,mmio32,0xfe201000'
kernel_append+=' console=ttyAMA0,115200'
kernel_append+=' rdinit=/init panic=-1 clk_ignore_unused ip=dhcp'
kernel_append+=' pi4lab.usb_storage=1'
[[ -z $extra_append ]] || kernel_append+=" $extra_append"

if ! "$qemu_binary" \
    -machine "$machine" \
    -kernel "$artifacts_dir/Image" \
    -dtb "$artifacts_dir/$dtb_name" \
    -initrd "$artifacts_dir/initramfs.cpio.gz" \
    -append "$kernel_append" \
    -drive "if=none,id=pi4-usb-storage,file=$usb_storage_image,format=raw" \
    -device usb-storage,drive=pi4-usb-storage,bus=vl805.0,port=1.1 \
    -nic user,model=genet \
    -nographic \
    -no-reboot 2>&1 | tee "$qemu_log"; then
    echo "QEMU exited before completing the Pi 4 Linux acceptance run" >&2
    exit 1
fi

if ! grep -Fq 'PI4-LAB: upstream Linux boot successful' "$qemu_log"; then
    echo "Pi 4 Linux acceptance marker was not produced" >&2
    exit 1
fi

if ! grep -Fq \
    'registered L2 intc (/soc/interrupt-controller@7ef00100' "$qemu_log"; then
    echo "BCM2711 AON L2 interrupt controller was not registered" >&2
    exit 1
fi

if ! grep -Fq \
    'PI4-LAB: HDMI0 DDC reads a valid 128-byte EDID' "$qemu_log"; then
    echo "BCM2711 HDMI DVP/DDC/EDID acceptance checks were not run" >&2
    exit 1
fi
