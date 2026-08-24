=========
QEMU Pi 4
=========

``qemu-pi4`` is an independent QEMU-derived project focused on improving
system emulation for the Raspberry Pi 4 family, including the Raspberry Pi 4
Model B and Raspberry Pi 400. It is not affiliated with the QEMU Project or
Raspberry Pi Ltd.

Project status
==============

The project started from QEMU 11.1.0, commit
``84f07211cc5b4fc6a371559bf8a5de4fb068e648``. The unchanged starting point is
marked by the ``qemu-pi4-base-v11.1.0`` tag.

The ``raspi4b`` machine models a 2 GiB Raspberry Pi 4 Model B revision 1.5.
The ``raspi400`` machine models a 4 GiB Raspberry Pi 400 revision 1.0 and is
available on 64-bit hosts.  Both use the BCM2711 SoC model; their board
revision, fixed RAM size, device tree and resulting machine identity differ.

The model includes the BCM2711 GENET v5 Ethernet controller and external PHY.
The pinned upstream Linux lab validates link, DHCP, transmit and receive DMA.

The BCM2711 GPIO controller exposes all 58 pin inputs and outputs, implements
edge and level event detection, and routes its three bank interrupts plus the
all-bank interrupt to the GIC.  Qtests cover the event registers, interrupt
grouping, reset and live migration.

The BCM2835 AUX mini UART models the Pi 400-observed shared enable gate and
low-byte readback, separates the IER interrupt bits from the IIR FIFO status,
implements the scratch register, and models its supported RTS control and CTS
status bits.  Disabled-bank reads, retained control writes and interrupt
signalling match hardware observations.  FIFO, enable, interrupt, scratch and
modem-control state reset and migrate with the VM, with modem lines forwarded
to capable character backends.

The on-SoC DWC2 USB controller implements core-soft-reset effects, including
terminating modeled host transfers, clearing interrupt masks while preserving
configuration and status, and completing receive/transmit FIFO flush commands.
The pinned upstream Linux lab exercises those commands during normal probe.

The VideoCore property interface tracks firmware clock and power-domain state,
accepts Linux's reboot notification, and resets pending mailbox responses
cleanly.  The state survives live migration.  Clock and domain values are a
firmware control-plane model only: they do not yet gate CPUs, clocks, or device
MMIO in the emulator.

Both boards expose the BCM2711 PCIe root complex and the fixed VIA VL805 xHCI
controller used for their external USB ports.  The model includes private PCI
DMA, MSI, multi-segment xHCI event rings, the VIA four-port USB 2 hub, and the
Pi 400's dual-interface integrated keyboard.  The pinned upstream Linux lab
also validates PCI identities, USB topology, MSI activity, xHCI
unbind/rebind recovery, and data integrity on a disposable USB mass-storage
device on both machines.

The BCM2711 V3D 4.2 graphics accelerator is not implemented.  The current
display support is a firmware-configured framebuffer, so Raspberry Pi DRM/Mesa
3D acceleration is unavailable.

Raspberry Pi 5 is not supported. It uses a substantially different BCM2712
SoC and RP1 I/O controller, neither of which this project currently models.

Focused build
=============

The ``pi4`` device configuration builds the AArch64 system emulator with the
Pi 4 machine and its required devices, without the normal set of unrelated
AArch64 boards:

.. code-block:: shell

  mkdir build
  cd build
  ../configure \
    --target-list=aarch64-softmmu \
    --without-default-devices \
    --with-devices-aarch64=pi4 \
    --disable-docs \
    --disable-tools
  ninja qemu-system-aarch64
  ./qemu-system-aarch64 -machine help

This fork separates Pi 4 support behind ``CONFIG_RASPI4`` and removes the
legacy Pi 0, Pi 1, Pi 2, and Pi 3 machine and SoC implementations. The focused
binary exposes only ``none``, ``raspi400`` and ``raspi4b`` as machine types on
a 64-bit host. Device models whose names start with ``bcm2835`` remain where
the Pi 4 still uses those compatible peripheral blocks.

Pi 4 regression gate
====================

The focused regression gate builds the emulator, verifies that its public
machine list contains only ``none``, ``raspi400`` and ``raspi4b``, runs the Pi
4 qtests, and boots a SHA-256-pinned Raspberry Pi Linux kernel on both board
models.  The boots check machine identity, usable physical RAM, PCIe/VL805,
the board-specific USB topology, an external USB-storage transfer, and GENET
DHCP.  The qtests also exercise GPIO event delivery and migration.  The
separate Linux 7.2 lab adds xHCI MSI and rebind recovery checks.

Asset download is deliberately separate from test execution.  From a
configured focused build directory, populate the content-addressed cache once
and then run the offline gate:

.. code-block:: shell

  make precache-pi4
  make check-pi4

``precache-pi4`` is the only step that accesses the network.  ``check-pi4``
sets ``QEMU_TEST_NO_DOWNLOAD=1`` and treats a missing or corrupt cache entry as
a failure rather than a skipped test.  The cache defaults to
``build/pi4-test-cache``; set ``PI4_TEST_CACHE_DIR`` to an absolute path to
share it between build directories.

The same two-stage path runs in GitHub Actions for every push and pull request.

Pi 4 hardware comparison lab
============================

``scripts/pi4/build-linux.sh`` builds a kernel.org source archive pinned by
version, upstream Git commit and SHA-256.  It produces one upstream Linux
``Image``, Pi 4 Model B and Pi 400 DTBs, a deterministic smoke-test initramfs,
and an artifact hash manifest.  ``scripts/pi4/capture-state.py`` records
read-only Linux hardware state and ``scripts/pi4/compare-state.py`` generates
a normalized Pi 400-versus-QEMU report.

See ``docs/system/arm/raspi-lab.rst`` for build, boot and comparison
instructions.

Licensing
=========

This project retains QEMU's existing licenses and per-file license notices.
QEMU as a whole is licensed under GPL-2.0; see ``LICENSE``, ``COPYING``, and
``COPYING.LIB`` for the complete licensing information. No replacement license
was added when the repository was created.

Upstream QEMU README
====================

QEMU is a generic and open source machine & userspace emulator and
virtualizer.

QEMU is capable of emulating a complete machine in software without any
need for hardware virtualization support. By using dynamic translation,
it achieves very good performance. QEMU can also integrate with the Xen
and KVM hypervisors to provide emulated hardware while allowing the
hypervisor to manage the CPU. With hypervisor support, QEMU can achieve
near native performance for CPUs. When QEMU emulates CPUs directly it is
capable of running operating systems made for one machine (e.g. an ARMv7
board) on a different machine (e.g. an x86_64 PC board).

QEMU is also capable of providing userspace API virtualization for Linux
and BSD kernel interfaces. This allows binaries compiled against one
architecture ABI (e.g. the Linux PPC64 ABI) to be run on a host using a
different architecture ABI (e.g. the Linux x86_64 ABI). This does not
involve any hardware emulation, simply CPU and syscall emulation.

QEMU aims to fit into a variety of use cases. It can be invoked directly
by users wishing to have full control over its behaviour and settings.
It also aims to facilitate integration into higher level management
layers, by providing a stable command line interface and monitor API.
It is commonly invoked indirectly via the libvirt library when using
open source applications such as oVirt, OpenStack and virt-manager.

QEMU as a whole is released under the GNU General Public License,
version 2. For full licensing details, consult the LICENSE file.


Documentation
=============

Documentation can be found hosted online at
`<https://www.qemu.org/documentation/>`_. The documentation for the
current development version that is available at
`<https://www.qemu.org/docs/master/>`_ is generated from the ``docs/``
folder in the source tree, and is built by `Sphinx
<https://www.sphinx-doc.org/en/master/>`_.


Building
========

QEMU is multi-platform software intended to be buildable on all modern
Linux platforms, OS-X, Win32 (via the Mingw64 toolchain) and a variety
of other UNIX targets. The simple steps to build QEMU are:


.. code-block:: shell

  mkdir build
  cd build
  ../configure
  make

Additional information can also be found online via the QEMU website:

* `<https://wiki.qemu.org/Hosts/Linux>`_
* `<https://wiki.qemu.org/Hosts/Mac>`_
* `<https://wiki.qemu.org/Hosts/W32>`_


Submitting patches
==================

The QEMU source code is maintained under the GIT version control system.

.. code-block:: shell

   git clone https://gitlab.com/qemu-project/qemu.git

When submitting patches, one common approach is to use 'git
format-patch' and/or 'git send-email' to format & send the mail to the
qemu-devel@nongnu.org mailing list. All patches submitted must contain
a 'Signed-off-by' line from the author. Patches should follow the
guidelines set out in the `style section
<https://www.qemu.org/docs/master/devel/style.html>`_ of
the Developers Guide.

Additional information on submitting patches can be found online via
the QEMU website:

* `<https://wiki.qemu.org/Contribute/SubmitAPatch>`_
* `<https://wiki.qemu.org/Contribute/TrivialPatches>`_

The QEMU website is also maintained under source control.

.. code-block:: shell

  git clone https://gitlab.com/qemu-project/qemu-web.git

* `<https://www.qemu.org/2017/02/04/the-new-qemu-website-is-up/>`_

A 'git-publish' utility was created to make above process less
cumbersome, and is highly recommended for making regular contributions,
or even just for sending consecutive patch series revisions. It also
requires a working 'git send-email' setup, and by default doesn't
automate everything, so you may want to go through the above steps
manually for once.

For installation instructions, please go to:

*  `<https://github.com/stefanha/git-publish>`_

The workflow with 'git-publish' is:

.. code-block:: shell

  $ git checkout master -b my-feature
  $ # work on new commits, add your 'Signed-off-by' lines to each
  $ git publish

Your patch series will be sent and tagged as my-feature-v1 if you need to refer
back to it in the future.

Sending v2:

.. code-block:: shell

  $ git checkout my-feature # same topic branch
  $ # making changes to the commits (using 'git rebase', for example)
  $ git publish

Your patch series will be sent with 'v2' tag in the subject and the git tip
will be tagged as my-feature-v2.

Bug reporting
=============

The QEMU project uses GitLab issues to track bugs. Bugs
found when running code built from QEMU git or upstream released sources
should be reported via:

* `<https://gitlab.com/qemu-project/qemu/-/issues>`_

If using QEMU via an operating system vendor pre-built binary package, it
is preferable to report bugs to the vendor's own bug tracker first. If
the bug is also known to affect latest upstream code, it can also be
reported via GitLab.

For additional information on bug reporting consult:

* `<https://wiki.qemu.org/Contribute/ReportABug>`_


ChangeLog
=========

For version history and release notes, please visit
`<https://wiki.qemu.org/ChangeLog/>`_ or look at the git history for
more detailed information.


Contact
=======

The QEMU community can be contacted in a number of ways, with the two
main methods being email and IRC:

* `<mailto:qemu-devel@nongnu.org>`_
* `<https://lists.nongnu.org/mailman/listinfo/qemu-devel>`_
* #qemu on irc.oftc.net

Information on additional methods of contacting the community can be
found online via the QEMU website:

* `<https://wiki.qemu.org/Contribute/StartHere>`_
