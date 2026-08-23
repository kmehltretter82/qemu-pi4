Raspberry Pi 4 hardware comparison lab
======================================

``qemu-pi4`` includes a small lab workflow for running one upstream Linux
kernel on QEMU and real Raspberry Pi 4-family hardware.  The intended first
hardware target is a Raspberry Pi 400.

The kernel source, source digest and corresponding upstream Git commit are
pinned in ``scripts/pi4/linux.lock``.  The build starts from the pinned
kernel's ``arm64`` defconfig and applies ``scripts/pi4/linux.config``.  It
produces one kernel ``Image``, board-specific Pi 4 Model B and Pi 400 device
trees, and a deterministic minimal initramfs.

Build the upstream kernel
-------------------------

On a Debian or Ubuntu x86-64 host, install the normal kernel build tools and
an AArch64 cross compiler.  For example::

  sudo apt-get install bc bison build-essential ca-certificates curl flex \
      gcc-aarch64-linux-gnu libelf-dev libssl-dev python3 xz-utils

Then run::

  scripts/pi4/build-linux.sh --cross-compile aarch64-linux-gnu-

An AArch64 Linux host, including a Pi 400, builds natively when
``--cross-compile`` is omitted.  The build script requires Linux; on macOS or
Windows, run it in a Linux VM or container with the repository mounted.
Downloaded source is verified before it is cached.  Use ``--no-download`` to
require an already populated cache.  The default locations are:

``.cache/pi4-linux``
  Verified source archive and extracted source tree.

``build-pi4-linux/build``
  Kernel build tree.

``build-pi4-linux/artifacts``
  ``Image``, both DTBs, QEMU and physical-test initramfs images, one-shot
  Pi 400 boot templates, final kernel configuration, hashes and build
  provenance.

The source and configuration are pinned, while ``build-provenance.txt``
records the compiler used.  Reuse the same compiler when bit-for-bit artifact
reproduction matters.

This is deliberately a diagnostic kernel based on the upstream ``arm64``
defconfig, not a tiny boot-only kernel.  The first build therefore compiles a
broad set of built-in drivers, but only the Pi 4 Model B and Pi 400 DTBs.

Boot on QEMU
------------

After creating a focused qemu-pi4 build, run::

  scripts/pi4/run-linux.sh \
      --qemu build/qemu-system-aarch64 \
      --artifacts build-pi4-linux/artifacts

To boot the same kernel with the Pi 400 machine and its board-specific DTB,
add::

  --machine raspi400

The runner connects the emulated GENET controller to QEMU user-mode
networking and requests a DHCP lease.  It also creates an 8 MiB sparse raw
image, attaches it as a QEMU ``46f4:0001`` mass-storage device on VIA hub port
one, and deletes it when QEMU exits.  The guest receives no physical disk or
user-supplied disk image; only that temporary file is modified.

The runner captures the complete QEMU console and exits successfully only if
QEMU exits cleanly and the guest prints ``PI4-LAB: upstream Linux boot
successful``.  The guest also verifies that the USB block device can service
a sector read before starting the integrity checks; seeing its ``/dev`` node
alone is not considered sufficient because Linux can publish it before disk
initialization has completed.

The initramfs verifies that upstream Linux selected ``iproc-rng200``, obtains
64 bytes through ``/dev/hwrng``, and reports the modeled 35050-millidegree
reading through its ``cpu-thermal`` zone.  It also verifies the exact BCM2711
root-port and VL805 PCI identities, both xHCI root hubs, the VIA
``2109:3431`` hub, and nonzero MSI activity.  It writes and reads a
deterministic 256 KiB pattern on the disposable USB disk after flushing the
guest block cache.  It then unbinds and rebinds the xHCI driver, requires the
complete USB topology and block device to re-enumerate, checks that the first
pattern survived, and performs a second write/read cycle.  On ``raspi400`` it
also requires the ``04d9:0007`` integrated keyboard and both of its HID
interfaces; ``raspi4b`` correctly omits those checks.  It prints basic kernel
and network state, the assigned IPv4 address, and the marker ``PI4-LAB:
upstream Linux boot successful``, then requests reboot.  The runner uses
``-no-reboot``, so that successful request terminates QEMU.

Linux can print a failed ``SYNCHRONIZE CACHE`` command while the deliberate
driver unbind tears down the USB transport.  The test flushes and closes its
own transfer before unbinding, then proves that the bytes survived after the
device returns; this disconnect-time message does not by itself fail the lab.

The same kernel and unmodified upstream Pi 4 DTB can also mount a normal
Linux root filesystem from the emulated external SD card.  Attach the image
with ``-drive file=IMAGE,if=sd,format=raw`` and use ``root=/dev/mmcblk0`` for
an unpartitioned filesystem or ``root=/dev/mmcblk0pN`` for partition ``N``.
See :doc:`raspi` for a complete command line and safe snapshot mode.

Boot on a Pi 400
----------------

Use a disposable or backed-up boot medium and keep physical reset access.
This project deliberately does not automate writing a boot partition.  Copy
the following files from ``build-pi4-linux/artifacts`` to an existing
Raspberry Pi firmware boot partition under these unique names:

* ``Image`` as ``Image-qemu-pi4``
* ``bcm2711-rpi-400.dtb`` as
  ``bcm2711-rpi-400-qemu-pi4.dtb``
* ``initramfs-hardware.cpio.gz`` as
  ``initramfs-qemu-pi4-hardware.cpio.gz``
* ``cmdline-hardware.txt`` as ``cmdline-qemu-pi4-hardware.txt``

Back up any existing ``tryboot.txt``, then install the generated
``tryboot.txt`` beside ``config.txt``.  Verify every copied file and run
``sync``.  Start the one-shot boot with the tryboot selector passed as one
quoted argument::

  sudo reboot '0 tryboot'

The firmware clears the one-shot flag before launching the test.  A subsequent
reset therefore returns to the normal ``config.txt`` boot.  The test kernel
uses ``panic=10`` for the same reason, while a hard kernel hang still requires
a physical reset or power cycle.  A 3.3 V serial console at 115200 baud is the
definitive diagnostic channel for failures before ``/init``.

The physical-test initramfs writes its report to
``qemu-pi4-hardware-result.txt`` on the FAT boot partition, syncs and unmounts
it, and then reboots into the normal OS.  ``clk_ignore_unused`` keeps the
diagnostic serial-console clock enabled through late kernel initialization.

The Pi firmware files themselves are not distributed by this project. QEMU's
``raspi400`` machine uses the unmodified upstream Pi 400 DTB; the kernel
``Image`` remains identical on both systems, while the two initramfs images
differ only in their test/return behavior.

RNG200 and thermal reference evidence
-------------------------------------

The models use a non-writing Pi 400 hardware capture made on 2026-08-23,
rather than values inferred only from Linux.  With the firmware-configured
RNG enabled, the capture observed control ``0x00007fff``, revision
``0x00040001``, a 16-word full FIFO reported as ``0x40001010``, and the empty
state ``0x80001000`` after 16 data reads.  An additional empty read returned
the stale data latch and set interrupt-status bit 4.  At rate selector 3, the
FIFO progressed from one word immediately to four words after a nominal
10-microsecond sleep, seven after a nominal 100-microsecond sleep, and full
after a one-millisecond sleep.  This is consistent with the documented
one-Mbit/s selector and a 32-microsecond period per 32-bit word once remote
scheduling overhead is considered; the emulated timer uses the exact nominal
rates rather than those coarse SSH measurement intervals.

The same capture read AVS temperature status values ``0x00010701`` and
``0x00010702`` while Linux reported 35537 and 35050 millidegrees Celsius.
Removing valid bits 16 and 10 leaves raw codes 769 and 770; applying the
BCM2711 device-tree formula ``-487 * raw + 410040`` reproduces both Linux
values exactly.  These observations establish the fork model's register
contract, but the previous absence of either device remains an enhancement,
not an upstream QEMU correctness bug.

On 2026-08-23, the pinned upstream Linux 7.2 acceptance image passed on both
``raspi4b`` and ``raspi400`` with the RNG200 and thermal checks enabled.  The
same boots also retained all PCIe, VL805, USB-storage, MSI, GENET, and Pi 400
keyboard acceptance gates.

GPIO reference evidence
-----------------------

The GPIO event model follows `BCM2711 ARM Peripherals
<https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf>`__,
sections 5.1 and 5.2.  The document defines 58 GPIOs in three interrupt banks,
three bank interrupt lines plus a fourth all-bank line, write-one-to-clear
event status, synchronous and asynchronous edge detection, and persistent
high and low level events.

A non-writing Pi 400 capture on 2026-08-23 read the event-status and detector
registers at offsets ``0x40``, ``0x44``, ``0x4c``, ``0x50``, ``0x58``,
``0x5c``, ``0x64``, ``0x68``, ``0x70``, ``0x74``, ``0x7c``, ``0x80``,
``0x88`` and ``0x8c`` from the GPIO base at ``0xfe200000``.  Every register
was zero in the firmware-configured idle state.  This confirms safe access
and the observed idle state only; the specification and qtests establish the
active event semantics.

Before fork commit ``9185e01891``, a pinned Linux 7.2 diagnostic boot with
``-d unimp,guest_errors`` produced 14 ``bcm2838-gpio`` messages for exactly
those event-register accesses.  After the change, the same Pi 4B boot
produced no GPIO unimplemented messages.  The complete 31-subtest Pi 4 qtest
target and the Linux acceptance boots for both ``raspi4b`` and ``raspi400``
then passed.  The qtests cover both edge polarities, both edge-detector modes,
active high and low level reassertion, every interrupt group, reserved bits,
output-latch retention, persistent external input levels across reset and
live migration with asserted events.

Capture and compare a full Linux system
---------------------------------------

For a richer comparison, run the capture tool inside a normal Linux userspace
on each system.  Root is optional, but normally provides complete ``dmesg``
and ``/proc/iomem`` output::

  sudo python3 scripts/pi4/capture-state.py \
      --label pi400 captures/pi400

  sudo python3 scripts/pi4/capture-state.py \
      --label qemu captures/qemu

Copy both directories to one machine and generate a Markdown report::

  python3 scripts/pi4/compare-state.py \
      captures/pi400 captures/qemu \
      --output captures/pi400-vs-qemu.md

The default report compares normalized CPU, interrupt, memory-map, device-tree,
platform, PCI, USB and network inventories.  Use ``--all`` to include raw
data, or ``--fail-on-difference`` when a known subset is later promoted to a
regression gate.  Differences are observations rather than automatic bugs.
Review raw device-tree and ``dmesg`` data for serial numbers, addresses or
other identifying information before publishing a capture.
