#!/bin/sh
# Probe the experimental QEMU CYW43455 SDIO endpoint from a Linux initramfs.
# SPDX-License-Identifier: GPL-2.0-or-later

PATH=/bin:/sbin
export PATH

mount -t devtmpfs devtmpfs /dev 2>/dev/null || true
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t debugfs debugfs /sys/kernel/debug 2>/dev/null || true

echo "CYW43455-QEMU: guest probe started"

show_sdio()
{
    found=false

    for device in /sys/bus/sdio/devices/*; do
        [ -e "$device" ] || continue
        found=true
        echo "CYW43455-QEMU: SDIO device ${device##*/}"
        for attribute in vendor device class function; do
            if [ -r "$device/$attribute" ]; then
                value=$(cat "$device/$attribute")
                echo "CYW43455-QEMU: $attribute=$value"
            fi
        done
    done

    $found || echo "CYW43455-QEMU: no SDIO functions enumerated"
}

load_module()
{
    module=$1

    insmod "/modules/$module.ko"
    result=$?
    echo "CYW43455-QEMU: insmod $module rc=$result"
}

show_sdio
echo "CYW43455-QEMU: SDHCI platform device"
ls -l /sys/bus/platform/devices/fe300000.mmc
echo "CYW43455-QEMU: deferred probes"
cat /sys/kernel/debug/devices_deferred 2>/dev/null || true
load_module rfkill
load_module cfg80211
load_module brcmutil
load_module brcmfmac
sleep 2
show_sdio

echo "CYW43455-QEMU: loaded modules"
cat /proc/modules
echo "CYW43455-QEMU: network devices"
ls -l /sys/class/net
echo "CYW43455-QEMU: final kernel messages"
dmesg | tail -n 120
echo "CYW43455-QEMU: guest probe complete"

sync
reboot -f
