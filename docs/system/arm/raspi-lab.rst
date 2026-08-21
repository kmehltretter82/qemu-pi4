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
  ``Image``, both DTBs, the initramfs, final kernel configuration, hashes and
  build provenance.

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

The initramfs prints basic kernel hardware state and the marker
``PI4-LAB: upstream Linux boot successful``, then requests poweroff.

Boot on a Pi 400
----------------

Use a disposable or backed-up boot medium.  This project deliberately does
not automate writing a boot partition.  Copy the following artifacts to an
existing Raspberry Pi firmware boot partition, using unique filenames:

* ``Image``
* ``bcm2711-rpi-400.dtb``
* ``initramfs.cpio.gz``

Select them in ``config.txt``::

  arm_64bit=1
  enable_uart=1
  kernel=Image-qemu-pi4
  device_tree=bcm2711-rpi-400-qemu-pi4.dtb
  initramfs initramfs-qemu-pi4.cpio.gz followkernel

Keep ``cmdline.txt`` on one line and append the smoke-test arguments::

  console=serial0,115200 rdinit=/init panic=-1 clk_ignore_unused

``clk_ignore_unused`` keeps the diagnostic serial console clock enabled through
late kernel initialization.

The Pi firmware files themselves are not distributed by this project.  The
minimal test initramfs powers the machine off after printing its report.
QEMU uses the Pi 4 Model B DTB until a separate ``raspi400`` machine exists;
the kernel ``Image`` and initramfs remain identical on both systems.

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
