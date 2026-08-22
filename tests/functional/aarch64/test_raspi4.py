#!/usr/bin/env python3
#
# Functional test that boots a Linux kernel on a Raspberry Pi machine
# and checks the console
#
# SPDX-License-Identifier: GPL-2.0-or-later

import hashlib

from qemu_test import LinuxKernelTest, Asset
from qemu_test import exec_command_and_wait_for_pattern


class Aarch64Raspi4Machine(LinuxKernelTest):

    """
    The kernel can be rebuilt using the kernel source referenced
    and following the instructions on the on:
    https://www.raspberrypi.org/documentation/linux/kernel/building.md
    """
    ASSET_KERNEL_20230106 = Asset(
        ('https://archive.raspberrypi.org/debian/'
         'pool/main/r/raspberrypi-firmware/'
         'raspberrypi-kernel_1.20230106-1_arm64.deb'),
        '56d5713c8f6eee8a0d3f0e73600ec11391144fef318b08943e9abd94c0a9baf7')

    ASSET_INITRD = Asset(
        ('https://github.com/groeck/linux-build-test/raw/'
         '86b2be1384d41c8c388e63078a847f1e1c4cb1de/rootfs/'
         'arm64/rootfs.cpio.gz'),
        '7c0b16d1853772f6f4c3ca63e789b3b9ff4936efac9c8a01fb0c98c05c7a7648')

    def assert_external_usb_path(self, pi400=False):
        checks = (
            ('cat /sys/bus/pci/devices/0000:00:00.0/vendor', '0x14e4'),
            ('cat /sys/bus/pci/devices/0000:00:00.0/device', '0x2711'),
            ('cat /sys/bus/pci/devices/0000:01:00.0/vendor', '0x1106'),
            ('cat /sys/bus/pci/devices/0000:01:00.0/device', '0x3483'),
            ('readlink -f /sys/bus/pci/devices/0000:01:00.0/driver',
             'xhci_hcd'),
            ('cat /sys/bus/usb/devices/*/idVendor', '2109'),
            ('cat /sys/bus/usb/devices/*/idProduct', '3431'),
        )
        for command, expected in checks:
            exec_command_and_wait_for_pattern(self, command, expected)

        if pi400:
            exec_command_and_wait_for_pattern(
                self, 'cat /sys/bus/usb/devices/*/idVendor', '04d9')
            exec_command_and_wait_for_pattern(
                self, 'cat /sys/bus/usb/devices/*/idProduct', '0007')

    def add_usb_storage(self):
        image_path = self.scratch_file('pi4-usb-storage.raw')
        pattern = bytes(range(256)) * 1024

        with open(image_path, 'wb') as image:
            image.write(pattern)
            image.truncate(8 * 1024 * 1024)
        self.vm.add_args(
            '-drive', f'file={image_path},if=none,id=pi4-usb,format=raw',
            '-device', 'usb-storage,drive=pi4-usb,bus=vl805.0,port=1.1')
        return hashlib.sha256(pattern).hexdigest()

    def assert_usb_storage_transfer(self, initial_digest):
        zero_digest = hashlib.sha256(bytes(256 * 1024)).hexdigest()

        exec_command_and_wait_for_pattern(
            self,
            'while [ ! -b /dev/sda ]; do sleep 1; done; '
            'cat /sys/block/sda/size',
            '16384')
        exec_command_and_wait_for_pattern(
            self,
            'dd if=/dev/sda bs=4096 count=64 2>/dev/null | sha256sum',
            initial_digest)
        exec_command_and_wait_for_pattern(
            self,
            'dd if=/dev/zero of=/dev/sda bs=4096 count=64 2>/dev/null; '
            'sync; dd if=/dev/sda bs=4096 count=64 2>/dev/null | sha256sum',
            zero_digest)

    def test_arm_raspi4(self):
        kernel_path = self.archive_extract(self.ASSET_KERNEL_20230106,
                                           member='boot/kernel8.img')
        dtb_path = self.archive_extract(self.ASSET_KERNEL_20230106,
                                        member='boot/bcm2711-rpi-4-b.dtb')

        self.set_machine('raspi4b')
        self.vm.set_console()
        kernel_command_line = (self.KERNEL_COMMON_COMMAND_LINE +
                               'earlycon=pl011,mmio32,0xfe201000 ' +
                               'console=ttyAMA0,115200 ' +
                               'root=/dev/mmcblk1p2 rootwait ' +
                               'dwc_otg.fiq_fsm_enable=0')
        self.vm.add_args('-kernel', kernel_path,
                         '-dtb', dtb_path,
                         '-append', kernel_command_line)
        self.vm.launch()
        console_pattern = 'Kernel command line: %s' % kernel_command_line
        self.wait_for_console_pattern(console_pattern)
        console_pattern = 'Waiting for root device'
        self.wait_for_console_pattern(console_pattern)


    def test_arm_raspi4_initrd(self):
        kernel_path = self.archive_extract(self.ASSET_KERNEL_20230106,
                                           member='boot/kernel8.img')
        dtb_path = self.archive_extract(self.ASSET_KERNEL_20230106,
                                        member='boot/bcm2711-rpi-4-b.dtb')
        initrd_path = self.uncompress(self.ASSET_INITRD)

        self.set_machine('raspi4b')
        self.vm.set_console()
        usb_storage_digest = self.add_usb_storage()
        kernel_command_line = (self.KERNEL_COMMON_COMMAND_LINE +
                               'earlycon=pl011,mmio32,0xfe201000 ' +
                               'console=ttyAMA0,115200 ' +
                               'panic=-1 noreboot ' +
                               'dwc_otg.fiq_fsm_enable=0')
        self.vm.add_args('-kernel', kernel_path,
                         '-dtb', dtb_path,
                         '-initrd', initrd_path,
                         '-append', kernel_command_line,
                         '-no-reboot')
        self.vm.launch()
        self.wait_for_console_pattern(
            'arch_timer: cp15 timer(s) running at 54.00MHz')
        self.wait_for_console_pattern('Boot successful.')

        exec_command_and_wait_for_pattern(self, 'cat /proc/cpuinfo',
                                                'BCM2835')
        exec_command_and_wait_for_pattern(
            self,
            'grep -qw aes /proc/cpuinfo && echo unexpected || echo no-crypto',
            'no-crypto')
        exec_command_and_wait_for_pattern(self, 'cat /proc/iomem',
                                                'cprman@7e101000')
        exec_command_and_wait_for_pattern(
            self, 'cat /proc/iomem',
            '40000000-7fffffff : System RAM')
        self.assert_external_usb_path()
        self.assert_usb_storage_transfer(usb_storage_digest)
        exec_command_and_wait_for_pattern(self, 'halt', 'reboot: System halted')
        # TODO: Raspberry Pi4 doesn't shut down properly with recent kernels
        # Wait for VM to shut down gracefully
        #self.vm.wait()

    def test_arm_raspi400_initrd(self):
        kernel_path = self.archive_extract(self.ASSET_KERNEL_20230106,
                                           member='boot/kernel8.img')
        dtb_path = self.archive_extract(self.ASSET_KERNEL_20230106,
                                        member='boot/bcm2711-rpi-400.dtb')
        initrd_path = self.uncompress(self.ASSET_INITRD)

        self.set_machine('raspi400')
        self.vm.set_console()
        kernel_command_line = (self.KERNEL_COMMON_COMMAND_LINE +
                               'earlycon=pl011,mmio32,0xfe201000 ' +
                               'console=ttyAMA0,115200 ' +
                               'panic=-1 noreboot ' +
                               'dwc_otg.fiq_fsm_enable=0')
        self.vm.add_args('-kernel', kernel_path,
                         '-dtb', dtb_path,
                         '-initrd', initrd_path,
                         '-append', kernel_command_line,
                         '-no-reboot')
        self.vm.launch()
        self.wait_for_console_pattern('Machine model: Raspberry Pi 400')
        self.wait_for_console_pattern('/4063232K available')
        self.wait_for_console_pattern(
            'arch_timer: cp15 timer(s) running at 54.00MHz')
        self.wait_for_console_pattern('Boot successful.')

        exec_command_and_wait_for_pattern(
            self, 'cat /proc/iomem',
            '40000000-fbffffff : System RAM')
        exec_command_and_wait_for_pattern(
            self, 'cat /proc/cpuinfo', 'BCM2835')
        exec_command_and_wait_for_pattern(
            self,
            'grep -qw aes /proc/cpuinfo && echo unexpected || echo no-crypto',
            'no-crypto')
        self.assert_external_usb_path(pi400=True)
        exec_command_and_wait_for_pattern(self, 'halt', 'reboot: System halted')


if __name__ == '__main__':
    LinuxKernelTest.main()
