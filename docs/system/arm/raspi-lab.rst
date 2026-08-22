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
networking and requests a DHCP lease.  The initramfs verifies the exact
BCM2711 root-port and VL805 PCI identities, both xHCI root hubs, the VIA
``2109:3431`` hub, nonzero MSI activity, and a successful xHCI driver
unbind/rebind followed by re-enumeration and fresh MSI activity.  On
``raspi400`` it also requires the ``04d9:0007`` integrated keyboard and both
of its HID interfaces; ``raspi4b`` correctly omits those checks.  It prints
basic kernel and network state, the assigned IPv4 address, and the marker
``PI4-LAB: upstream Linux boot successful``, then requests reboot.  The runner
uses ``-no-reboot``, so that successful request terminates QEMU.

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
