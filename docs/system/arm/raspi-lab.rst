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
QEMU exits cleanly, the guest prints ``PI4-LAB: upstream Linux boot
successful``, and Linux reports that it registered the BCM2711 AON L2
interrupt controller.  The initramfs also requires the ``brcm2711-dvp`` and
both ``brcmstb-i2c`` platform drivers, finds HDMI0 by its adapter identity,
and validates both blocks of a 256-byte EDID through Linux's ``I2C_RDWR``
path.  The check verifies both checksums and the CTA Basic Audio, stereo LPCM,
speaker-allocation and HDMI vendor blocks.  It also requires VC4 DRM to
register ``card0``, report a connected HDMI-A-1 with the preferred 1280x800
mode, enable scanout and create a 1280x800 RGB565 framebuffer.  The guest
requires two successful target-range sector reads in consecutive polling
intervals before starting the storage integrity checks; seeing the block
device's ``/dev`` node alone is not sufficient because Linux can publish it
while disk initialization is still in progress.

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

The normal runner proves the Linux display-device contracts even though it
uses ``-nographic``.  To prove visible pixels as well, run the dedicated gate
for each board model::

  scripts/pi4/test-display.py \
      --qemu build/qemu-system-aarch64 --machine raspi4b
  scripts/pi4/test-display.py \
      --qemu build/qemu-system-aarch64 --machine raspi400

The guest fills ``/dev/fb0`` with four deterministic RGB565 bands, enables
universal planes through the production DRM UAPI, and attaches an otherwise
unused RGB565 overlay plane to the active CRTC.  Linux programs the HVS to
scale the 320x200 overlay to 640x400 at ``(320, 200)``.  The gate waits for all
Linux acceptance checks, requests a QMP screendump, validates unobscured
primary samples at the top and bottom, and checks every colored quadrant in
the scaled overlay.  This distinguishes a merely bound DRM driver from an HVS
composition path that actually reaches QEMU's display surface.

To validate HDMI0 audio through the production VC4 driver and the host audio
backend, run the dedicated gate for each board model::

  scripts/pi4/test-audio.py \
      --qemu build/qemu-system-aarch64 --machine raspi4b
  scripts/pi4/test-audio.py \
      --qemu build/qemu-system-aarch64 --machine raspi400

The guest opens ``vc4-hdmi-0`` as 48 kHz, two-channel IEC958 PCM and sends one
second of distinct left and right square waves through MAI and DMA.  The host
gate rejects silence, internal gaps, channel swapping, incorrect amplitude,
frequency or duration, and a malformed WAV format.  Pass ``--output PATH`` to
retain the validated capture.

The Pi 400-specific input gate exercises real Linux evdev delivery after the
VL805 rebind::

  scripts/pi4/test-input.py --qemu build/qemu-system-aarch64

It adds a standard QEMU USB mouse on hub port ``1.1``, opens the integrated
``04d9:0007`` keyboard, its separate consumer-control node, and the mouse
``/dev/input/event*`` nodes from the guest.  It sends an ``A`` press/release,
play/pause, volume-up, calculator, relative X/Y motion, and a left-button
press/release through QMP.  The gate accepts only after the guest reports each
event.  ``pi4lab.input_demo=1`` is opt-in and leaves the normal rebooting
acceptance image unchanged; it is also useful with the Cocoa Pi 400 desktop
demo because typed keys and mouse movement are logged on the guest serial
console.

The AUX SPI1 Linux-driver gate uses a test-only device-tree overlay and an
explicit virtual M25P80 device; it leaves the stock Pi DTB unchanged::

  scripts/pi4/test-aux-spi.py \
      --qemu build/qemu-system-aarch64 --machine raspi4b
  scripts/pi4/test-aux-spi.py \
      --qemu build/qemu-system-aarch64 --machine raspi400

It requires ``dtc`` and ``fdtoverlay`` on the host.  The guest must bind
``spi-bcm2835aux`` to ``spi1.0``, bind its generic SPI-NOR child, and read 16
erased bytes from the resulting MTD device.  It is a controller/driver test,
not a claim of physical GPIO wiring.

The same kernel and unmodified upstream Pi 4 DTB can also mount a normal
Linux root filesystem from the emulated external SD card.  Attach the image
with ``-drive file=IMAGE,if=sd,format=raw`` and use ``root=/dev/mmcblk0`` for
an unpartitioned filesystem or ``root=/dev/mmcblk0pN`` for partition ``N``.
See :doc:`raspi` for a complete command line and safe snapshot mode.

The BCM2711 EMMC2 controller at ``0xfe340000`` exposes the Pi 400's
hardware-derived SDHCI CAPABILITIES value ``0x0000a52545ee6432``.  This
advertises the controller's 100 MHz base clock and ADMA2/SDMA support; Linux
therefore reports ``mmc0 ... using ADMA`` during the acceptance boot.  The
legacy BCM2835-compatible controller at ``0xfe300000`` retains its separate
``0x052134b4`` capability value.

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

AON L2 interrupt-controller reference evidence
-----------------------------------------------

Controlled Pi 400 probes on 2026-08-24 established the behavior used by the
BCM2711 AON model.  With Raspberry Pi OS running, the CPU bank reported status
``0x008`` and mask ``0x24c`` while the PCI bank reported status ``0x009`` and
mask ``0xfff``.  The software set and clear registers acted independently in
the two banks, mask-set and mask-clear were write-one operations, action
registers read as zero, and only the low twelve bits were implemented.

A second, cleanup-guarded probe masked child zero, generated a CEC transmit
through the normal Linux CEC interface, and observed bit zero latch in both
status banks.  Clearing the bit and restoring the masks returned the CPU bank
to status ``0x008``/mask ``0x24c`` and the PCI bank to status ``0x008``/mask
``0xfff``.  This supplies physical-source evidence that the two banks are
independent destinations for the same twelve edge-latched inputs; it does not
claim undocumented behavior for the absent HDMI blocks beyond that measured
contract.

On 2026-08-24, the pinned unmodified Linux 7.2 image registered
``irq_brcmstb_l2`` at ``/soc/interrupt-controller@7ef00100`` and completed the
full acceptance run on both ``raspi4b`` and ``raspi400``.  Focused qtests also
proved bank-local software events, dual-bank physical events, mask and clear
behavior, held-high edge handling, watchdog reset and migration with asserted
outputs.

Native display, HDMI DVP and DDC reference evidence
----------------------------------------------------

A read-only Pi 400 capture on 2026-08-24 used Raspberry Pi OS kernel
``6.18.39+rpt-rpi-v8``.  Linux had bound ``brcm2711-dvp`` to
``fef00000.clock``, ``vc4_hdmi`` to both HDMI transmitters, and
``brcmstb-i2c`` to ``fef04500.i2c`` and ``fef09500.i2c``.  The two DDC
adapters appeared as ``i2c-20`` and ``i2c-21``.  Both physical connectors were
reported disconnected.

After normal driver initialization, the DVP words at offsets zero through
``0x0c`` read ``0x00000200``, zero, ``0x00000018`` and ``0xffff0000``.  Both
DDC BSC blocks had chip-address value ``0xa0``, count and IIC-enable zero,
control ``0x90`` for the 97.5 kHz configuration, control-high ``0x40`` for
32-bit data registers, and zero in the data and SCL-parameter registers.  The
auto-I2C release action at offset ``0x26c`` read back as zero.

A cleanup-guarded ``i2c-dev`` probe attempted a normal EDID pointer write and
read at address ``0x50`` on each disconnected adapter.  Both returned
``EREMOTEIO``, and both controllers returned to the same idle register state.
The temporary module was removed afterward.  This establishes the observed
idle readback and disconnected NACK path, but not undocumented HDMI behavior
or every possible BSC transfer encoding.  The fork's attached HDMI0 EDID is a
deliberate virtual-monitor default rather than a claim that the physical Pi
had a monitor connected during capture; HDMI1 retains the captured
disconnected behavior.

On the emulated side, the pinned Linux 7.2 image bound the DVP and both DDC
drivers on ``raspi4b`` and ``raspi400``.  Its production ``I2C_RDWR`` path
successfully performed two pointer writes followed by eight 32-byte read
chunks, validated both EDID checksums and parsed the HDMI stereo-audio CTA
blocks.  Focused qtests also cover DVP and DDC reset, ownership, ACK/NACK,
malformed-command cleanup and live migration with a repeated-start session
left open.

The native scanout milestone intentionally uses the virtual HDMI0 monitor
rather than pretending the disconnected physical capture supplied a display
contract.  On both machine models the same pinned Linux image bound HVS,
HDMI0, TXP and pixel valve 2, registered VC4 DRM, selected the virtual
monitor's 1280x800 mode and created an RGB565 framebuffer.  The first pixel
gate verified the framebuffer's four color bands in a QMP screendump.

The 2026-08-24 composition extension exercised the production VC4 plane path
on both ``raspi4b`` and ``raspi400``.  Linux selected plane 68 on CRTC 67,
created a pitch-640 320x200 RGB565 dumb buffer and programmed a 640x400
destination at ``(320, 200)``.  Host screendumps retained the unobscured
primary bands and showed the expected green, blue, white and black overlay
quadrants.  Focused qtests separately cover two-plane composition, short-list
nearest-neighbor scaling, a Pi 400-referenced Mitchell--Netravali PPF
approximation, a Pi 400-referenced TPZ downscale approximation, RGB888 byte
order, coverage and premultiplied alpha, plane-alpha mixing, horizontal and
vertical reflection, full-surface unity-mode T-tiled RGB565 and RGBA8888
scanout, live display-list updates and visible post-migration reconstruction.

Focused qtests cover HVS display-list memory and channel state, the pixel
valve's masked write-one-to-clear vblank interrupt and stable frame deadline,
HDMI0 hotplug/FIFO/packet/scheduler responses, DVP-driven transmitter reset,
system reset and migration while the pixel-valve timer and DDC repeated-start
session are active.  No raw HVS, pixel-valve or HDMI-transmitter register
writes were made on the physical Pi for this milestone.  The implemented
register subset is justified by the upstream Linux driver path and virtual
display contract.  Short display lists retain nearest-neighbor sampling; a
complete linear-RGB PPF list uses a continuous Mitchell--Netravali
approximation checked against the Pi 400's 4x4-to-8x8 capture, and a complete
TPZ downscale list uses a source-coverage box approximation checked against
Pi 400 2:1, horizontal 2.5:1 and 3:1 one-pixel-checker captures.  Neither test
establishes the HVS's quantized coefficient datapath, full TPZ behavior or
line-buffer-memory implementation as a complete BCM2711 hardware reproduction.
The T-tile qtest uses a synthetic display list built from the upstream Linux
driver's T-tile addressing contract; it verifies the bounded
full-surface layout and safe rejection of scaled T planes, rather than claiming
a physical-Pi display capture for that form.

The same Linux driver registered the ``vc4-hdmi-0`` IEC958 playback device and
completed the MAI/DMA 48 kHz stereo workload on both machines.  The final
acceptance run captured 48,279 active frames on ``raspi4b`` and 48,221 on
``raspi400``;
the measured channels were approximately 1 kHz and 2 kHz with the intended
2.0001 RMS ratio and no internal silent frames.  The functional endpoint is
QEMU's host audio core: no claim is made that the fork generates TMDS or HDMI
audio packets, and the physical Pi 400's disconnected ports supplied no
monitor-derived audio comparison for this milestone.

On 2026-08-24 the complete Pi-focused gate passed all eight qtest binaries and
all 85 subtests: ten display, four CPRMAN, seven DMA, ten I2S, nine PWM,
three I2C, five Pi 400 and 37 Pi 4B tests.  Both boards then passed the full
Linux USB-storage/GENET/DRM acceptance boot and the separate visible-pixel
and HDMI-audio gates.

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

AUX mini-UART reference evidence
--------------------------------

`BCM2711 ARM Peripherals
<https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf>`__,
section 2.2, describes the AUX mini UART as 16550-like but not 16550
compatible.  ``AUX_ENABLES`` resets to zero and its three defined low bits
enable the mini UART and two SPI blocks.  ``IER`` contains only the two low
interrupt-enable bits; its upper bits are reserved, while ``IIR`` bits 7:6
report FIFO status.  The scratch register is 8-bit read/write and resets to
zero.  In the modem registers, ``MCR`` bit 1 is the active-low RTS control
with reset value zero, while ``MSR`` bit 4 is the inverse of the CTS input
with reset value one.  The maintained Linux BCM2835 AUX 8250 driver likewise
documents RTS as the only MCR flag and writes that register during ordinary
serial initialization.  The known BCM2835 documentation error which swaps
the RX and TX IER descriptions is unrelated; QEMU already uses the corrected
bit assignments.

A non-writing Pi 400 capture on 2026-08-23 found that the running Raspberry
Pi OS kernel had disabled the shared block: ``AUX_ENABLES``, ``IER``, ``IIR``,
``MCR``, ``LSR``, ``MSR`` and ``CNTL`` all read zero.  That is useful evidence
of runtime block gating, but is not by itself a power-on-reset oracle.

Controlled, reversible probes on 2026-08-23 and 2026-08-24 then established
the active behavior.  Enabling the mini UART exposed ``IER=0``, ``IIR=0xc3``,
``LCR=3``, ``MCR=0``, ``LSR=0x60``, ``MSR=0x10``, ``SCRATCH=0``, ``CNTL=3``,
``STAT=0x34a`` and ``BAUD=0x21d``.  Writing all ones to ``AUX_ENABLES`` read
back ``0xff`` on this Pi 400; only bits 0:2 are architecturally defined, so
the fork records the low-byte silicon behavior without assigning meaning to
the other retained bits.

The broad manual statement that a disabled module has no register access does
not fully describe this silicon.  With ``AUX_ENABLES=0``, writes of ``IER=2``,
``MCR=0xffffffff`` and ``SCRATCH=0x1a5`` all read back as zero, but enabling
the UART exposed the retained values ``2``, ``2`` and ``0xa5``.  The pending
TX interrupt was not gated: the defined UART bit in ``AUX_IRQ`` was set and
GIC interrupt ID 125 was pending even while the UART bank read zero.  The raw
``AUX_IRQ`` value also contained reserved bit 31; the model deliberately uses
only the defined UART bit.  The probe cleared IER, MCR, scratch and GIC pending
state, restored ``AUX_ENABLES=0``, and verified zero AUX and GIC status before
exiting.

Against unmodified current QEMU master
``eea8fe61b8be8f3016e522e6af24924a0266ca95`` on 2026-08-24,
``scripts/pi4/repro-aux-enable.py`` observed startup ``EN=1, IER=0xc0``, an
all-ones enable readback of ``1``, no effective disable, and no MCR, MSR or
scratch state.  The fork observed startup ``EN=0, IER=0``, low-byte readback
``0xff``, gated reads with live interrupt status, and the retained register
values above.  The aid exits 1 with current upstream and 0 with the fork.

The reset aid explicitly enables the UART before taking its internal
baseline, so ``scripts/pi4/repro-aux-reset.py`` compares both model styles
fairly.  Current upstream started with ``EN=1, IER=0xc0, IRQ=0``, armed
``IER=0xc2, IRQ=1`` and retained that armed state across a watchdog cold
reset.  The fork started with ``EN=0``, exposed ``IER=0, IRQ=0`` after enable,
reset the gate and internal state, and exposed the same clean values after
re-enable.  This aid also exits 1 with upstream and 0 with the fork.

Before fork commit ``4f78fe1e54``, each pinned Linux 7.2 diagnostic boot
logged one unsupported ``AUX_MU_MCR_REG`` write in addition to three PL011
messages.  After the change, both boards retain all PCIe, VL805, MSI,
USB-storage, GENET and Pi 400 keyboard checks, and only the three PL011
messages remain.  The complete focused gate passes 37 Pi 4B qtests, five Pi
400 qtests, all three offline functional boots and all 15 lab-tool tests.
The 2026-08-24 enable-gate, IER and scratch change repeated both pinned Linux
boots with the same result: each diagnostic log contained exactly those three
PL011 messages and no AUX, unimplemented or guest-error message.
The remaining PL011 diagnostics are intentional generic-QEMU compatibility
messages.  `Upstream commit 907b8d56351b
<https://gitlab.com/qemu-project/qemu/-/commit/907b8d56351b1ba6c97953edaca6a08f02fa2048>`__
documents that Linux earlycon relies on firmware having enabled the UART and
that QEMU accepts the bytes while logging the dubious disabled write.  They
are not evidence of a Pi machine failure.

The AUX qtest covers reset-low enable state, observed low-byte readback,
disabled-bank reads and retained writes, IER/IIR separation, scratch masking,
RTS and CTS values, interrupt assertion while disabled, cold-reset
deassertion, reuse after reset, and migration of hidden control state.

AUX SPI1/SPI2 PIO reference evidence
-------------------------------------

The BCM2711 peripheral manual places the two AUX SPI register banks at
offsets ``0x80`` and ``0xc0`` within AUX, with ``CNTL0``, ``CNTL1``, ``STAT``,
``PEEK``, ``IO`` and ``TXHOLD`` at offsets ``0x00``, ``0x04``, ``0x08``,
``0x0c``, ``0x20`` and ``0x30``.  The maintained Linux
``spi-bcm2835aux`` driver uses MSB-first variable-width words containing one
to three bytes, and keeps at most four such words in flight during its PIO
path.  This establishes the bounded controller contract; it does not by
itself establish electrical timing or pin routing.

Controlled, cleanup-guarded Pi 400 probes on 2026-08-26 found both SPI banks
at zero while their AUX enable bits were clear.  With SPI1 enabled, ``CNTL0``
and ``CNTL1`` initially read zero and ``STAT`` read ``0x280`` (empty TX and RX
FIFOs).  Values written while enabled were hidden by a disable/re-enable gate
cycle but retained internally.  Setting SPI1 ``CNTL1`` to ``0xc2`` produced
the defined SPI1 ``AUX_IRQ`` bit; the physical raw value was ``0x80000002``,
whose top bit is reserved.  The defined bit remained live while SPI1's
register bank was gated.  The model represents only defined interrupt bits,
consistent with the existing mini-UART policy.

The focused qtests verify the gate, idle/status and PIO FIFO behavior, shared
SPI1/SPI2 AUX interrupt bits, watchdog reset, migration of hidden gated state,
and repeated JEDEC-ID exchanges with QEMU's standard ``m25p80`` SSI model.
The exchanges verify that ``TXHOLD`` retains and final ``IO`` releases a
virtual SSI chip select; a held JEDEC exchange also resumes after live
migration.  No physical transfer or GPIO-chip-select probe is claimed here;
those remain future hardware work.

DWC2 reset reference evidence
-----------------------------

The emulated controller reports DWC2 revision 2.94a in ``GSNPSID``.  The
archived `DWC2 2.94 GRSTCTL register description
<https://nest-open-source.googlesource.com/manifest_repos/u-boot/+/5e15fb1fe70ac2857e056ad5d238ad8e3373fdb5/drivers/usb/host/dwc_otg_regs_294.h>`__
states that a core soft reset returns the state machines to idle, terminates
AHB and USB transactions, clears interrupt-generating mask bits, preserves
interrupt status and configuration, flushes the FIFOs and self-clears.  The
same description defines receive-FIFO flush and selective or all-transmit-FIFO
flush as self-clearing operations.

Before fork commit ``3ac780f519``, the pinned Linux 7.2 diagnostic boot issued
two core soft resets, one all-transmit-FIFO flush and one receive-FIFO flush
during ordinary DWC2 probe.  QEMU logged all four operations as unimplemented.
After the change, the diagnostic boots for both ``raspi4b`` and ``raspi400``
contained no DWC2 unimplemented message and retained every Linux acceptance
check.  The complete 32-subtest Pi 4 qtest target and five-subtest Pi 400
target also passed.

The focused DWC2 qtest checks mask clearing, IRQ deassertion, host-channel
disable, configuration and status preservation, and immediate completion of
receive and all-transmit FIFO commands.  Raw DWC2 register writes were not
made on the real Pi 400 for this milestone; the revision-matched register
contract and the upstream Linux driver path establish the expected behavior.

Firmware property-state reference evidence
------------------------------------------

A Pi 400 capture on 2026-08-23 used ``/dev/vcio`` to query the VideoCore
property interface.  The board ran Raspberry Pi OS kernel
``6.18.39+rpt-rpi-v8`` with firmware
``288930ab4712b99596f32732664aaaeb881ef1e0`` dated 2026-05-21.  Every
two-word query returned a response header of ``0x80000008`` and echoed the
requested ID.

Clock IDs 2, 4, 9 and 15 reported enabled.  IDs 1, 3, 5 through 8 and 10
through 14 reported disabled.  IDs 0, 16 and 17 returned the not-present bit.
A same-state ``SET_CLOCK_STATE`` request kept V3D clock ID 5 disabled, and a
following query confirmed the state.  These are runtime observations after
Linux boot, not firmware-reset defaults; the fork keeps all known clocks
initially enabled to preserve its prior guest-visible behavior.

A bounded follow-up on 2026-08-24 queried ``GET_CLOCKS`` and the current,
minimum and maximum rate of every candidate ID.  Discovery returned exactly
the parent/ID pairs zero/1 through zero/15; ID 16 was absent.  Rates below are
in MHz and are the raw integer responses divided by one million::

  ID  clock       current  minimum  maximum
   1  EMMC            250      250      250
   2  UART             48        0     1000
   3  ARM            1800      600     1800
   4  CORE            200      200      500
   5  V3D             250      250      500
   6  H264            250      250      500
   7  ISP             250      250      500
   8  SDRAM           400      400      400
   9  PIXEL             0        0     2400
  10  PWM               0        0      500
  11  HEVC            250      250      500
  12  EMMC2             0        0      500
  13  M2MC            120        0      600
  14  PIXEL_BVB        75       75      324
  15  VEC               0        0      108
  16  DISP              0        0        0  (not present)

The fork uses those exact integer values for its per-clock firmware profile.
Clock-rate writes are retained, clamped to the captured range, reset to the
captured current values and migrated.  This makes normal Linux clock
discovery work without converting the runtime snapshot into a claim about
firmware-reset sequencing or functional device clock gating.

Firmware domain IDs 4, 5, 7, 20 and 23 reported enabled; IDs 0 through 24
otherwise reported disabled.  Those enabled IDs correspond to video scaler,
VPU1, USB, transposer and ARM.  An exploratory write of nonzero state to V3D
domain ID 11 was followed by an enabled query, but its direct response changed
the returned ID unexpectedly.  A subsequent attempt to disable that live
domain left the ``/dev/vcio`` caller in uninterruptible sleep until the board
was rebooted.  The board recovered normally.  Raw domain-state writes are
therefore excluded from automated hardware capture, and that anomalous SET
response is not used as a response-layout contract.

Before fork commit ``3e05eb0ad0``, a pinned Linux 7.2 diagnostic boot on each
board model logged ``SET_CLOCK_STATE`` as not implemented and
``GET_DOMAIN_STATE`` and ``NOTIFY_REBOOT`` as unhandled.  With the change,
both Linux boots retained the complete PCIe, VL805, MSI, USB-storage, GENET
and Pi 400 keyboard acceptance checks, and those three property messages
disappeared.  Three unrelated PL011 messages remain in each diagnostic log.
The complete 33-subtest Pi 4B and five-subtest Pi 400 qtest targets passed;
the Pi 4B tests also cover invalid IDs, reset defaults, a response stalled by
a full mailbox, and migration of changed clock and domain state.

The emulated state is intentionally shallow.  Property calls do not power-gate
MMIO devices, stop vCPUs, or reproduce firmware sequencing and latency.  The
hardware capture establishes useful IDs, state encoding and one runtime
snapshot without claiming those deeper effects.

OTP and board-identity reference evidence
-----------------------------------------

On 2026-08-24 a read-only ``vcgencmd otp_dump`` capture on the project's Pi
400 confirmed that row 28 contains the low serial word, row 29 is its exact
ones' complement and row 30 is ``0x00c03130``, the Pi 400 revision code.
``/proc/cpuinfo`` and the device-tree ``serial-number`` reported the same
Linux-visible serial, whose low 32 bits matched row 28.  The unique physical
serial is deliberately omitted from this public evidence log.

The `official Raspberry Pi OTP documentation
<https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#otp-register-and-bit-definitions>`__
assigns those same logical row numbers and assigns customer OTP to rows 36
through 43.  Consequently QEMU's one-based helper, which stores logical row
36 at array index 35, is not off by one.  The earlier outer-workspace B10
hypothesis confused a public row number with its private C-array index and is
rejected.

The same documentation specifies an eight-byte ``GET_BOARD_SERIAL`` response
whose most-significant 32 bits are zero.  The Pi 400's Linux-visible serial
had a nonzero prefix, but that is not the mailbox response contract.  The fork
therefore uses a non-identifying synthetic low word, returns a zero high word,
and does not clone the captured physical identity.  Focused qtests cover both
board revisions and serial responses, persistence across reset, and migration
of a guest-programmed customer OTP row.

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
