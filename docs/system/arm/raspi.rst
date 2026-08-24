Raspberry Pi 4 family
=====================

``qemu-pi4`` provides two BCM2711 board models in the AArch64 system emulator:

``raspi4b``
  Raspberry Pi 4 Model B revision 1.5 (board revision ``0xb03115``), with
  2 GiB of RAM.

``raspi400``
  Raspberry Pi 400 revision 1.0 (board revision ``0xc03130``), with 4 GiB of
  RAM. This model requires a 64-bit host.

The Pi 400's ARM-visible memory has a VideoCore carveout below 1 GiB and the
BCM2711 peripheral window at ``0xfc000000``. With the default 64 MiB VideoCore
allocation, Linux is therefore presented with 3968 MiB in two ranges rather
than a single contiguous 4 GiB range. This matches the address layout captured
from real hardware; there is no invented RAM relocation above 4 GiB.

Raspberry Pi 5 is not supported: its BCM2712 SoC and RP1 I/O controller
require separate models.

Potential correctness fixes and enhancements suitable for later submission
to QEMU are kept in the evidence-based :doc:`raspi-upstream` tracker.
The implementation evidence and remaining fidelity work for the Pi 4-family
PCIe and external USB path are recorded in :doc:`raspi-pcie`.
The separate :doc:`raspi-gicv2-lab` project will exercise the GICv2
virtualization interface across this fork, Linux KVM, and real Pi 4-family
hardware.  Its first real Pi 400 boot is intentionally deferred until its
minimal EL2 and EL1 paths pass the QEMU safety gate.

Implemented devices
-------------------

 * Four Cortex-A72 CPU cores
 * GIC-400 and legacy VideoCore interrupt controllers
 * DMA controller with bounded asynchronous control-block execution,
   byte-aligned transfers, DREQ pacing, pause/abort, and active migration
 * Clock and reset controller (CPRMAN), with BCM2711 oscillator, PLL and
   firmware-configured clock defaults
 * BCM2835-compatible PCM/I2S controller with playback, DMA and interrupts
 * System Timer
 * GPIO controller, including all 58 input/output lines, edge and level event
   detection, and the three bank interrupts plus the all-bank interrupt
 * Serial ports (BCM2835 AUX - 16550 based - and PL011), including the mini
   UART's supported RTS control and CTS status bits
 * Frame Buffer
 * Arasan eMMC2 SD/MMC host controller and external SD card
 * USB2 host controller (DWC2 and MPHI)
 * Broadcom GENET v5 Gigabit Ethernet controller with an external
   BCM54213PE-compatible PHY
 * MailBox controller (MBOX)
 * VideoCore firmware property interface, including firmware-controlled GPIOs,
   clock and power-domain state, reboot notification, and the Pi 4-family
   VL805 initialization notification
 * Peripheral SPI controller (SPI)
 * Broadcom Serial Controller (I2C)
 * BCM2711 RNG200 random number generator, including its 16-word FIFO,
   four generation rates, status and interrupt registers, soft resets, and
   migratable refill timer and FIFO contents
 * BCM2711 AVS thermal monitor, using the device-tree calibration and a
   migratable, configurable temperature reading
 * BCM2711 PCIe host and root port, including dynamic outbound and inbound
   DMA windows, INTx, and MSI
 * Pi 4-family VIA VL805 PCIe xHCI personality, including the captured PCI and
   xHCI register layout, multi-segment event rings, DMA-backed controller
   events, MSI, PERST, migration state, and a guest-visible PCIe device-tree
   node
 * VIA ``2109:3431`` four-port high-speed USB hub on both boards
 * Raspberry Pi 400 ``04d9:0007`` low-speed integrated keyboard, including
   its keyboard and consumer-control HID interfaces

Missing devices
---------------

 * V3D 4.2 graphics accelerator (its MMIO range is an unimplemented
   placeholder)
 * BCM2711 HDMI/display pipeline and HDMI DDC I2C controllers
 * BCM2711 always-on L2 interrupt controller used by HDMI
 * Pulse Width Modulation (PWM)
 * Remaining BCM2711 PCIe controller-event behavior
 * Consumer-control key-event production for the Pi 400 keyboard's second HID
   interface; its identity, descriptors, enumeration and migration already
   work

Booting Linux from an SD image
------------------------------

The external SD card is connected to the Pi 4 eMMC2 controller.  Attach a raw
card image with ``if=sd``.  QEMU does not emulate the Raspberry Pi boot
firmware, so the kernel and device tree must still be supplied explicitly.
For example::

  qemu-system-aarch64 \
      -machine raspi4b \
      -kernel Image \
      -dtb bcm2711-rpi-4-b.dtb \
      -drive file=raspios.img,if=sd,format=raw,snapshot=on \
      -nic user,model=genet \
      -append 'earlycon=pl011,mmio32,0xfe201000 console=ttyAMA0,115200 root=/dev/mmcblk0p2 rootwait rw' \
      -nographic

With an unpartitioned filesystem image, use ``root=/dev/mmcblk0`` instead.
The SD model requires an image whose size is a valid SD card capacity; a
power-of-two size such as 4 GiB is a convenient choice.  Remove
``snapshot=on`` only when guest writes should persist.

For a Pi 400 guest, change the machine and device tree together::

  -machine raspi400
  -dtb bcm2711-rpi-400.dtb

Supplying a Pi 4 Model B DTB to ``raspi400`` (or the reverse) gives the guest
the wrong board identity and peripherals even though both boards use BCM2711.

Attaching USB storage
---------------------

The focused build includes QEMU's standard USB mass-storage device so that
external-stick workloads can exercise the complete BCM2711 PCIe, VL805 and
VIA-hub path.  For example, attach a raw image to hub port one with safe
snapshot writes::

  -drive file=stick.raw,if=none,id=stick,format=raw,snapshot=on \
  -device usb-storage,drive=stick,bus=vl805.0,port=1.1

Ports ``1.1`` through ``1.3`` are free on both machines.  Port ``1.4`` is also
free on ``raspi4b`` but contains the integrated keyboard on ``raspi400``.
Remove ``snapshot=on`` only when writes to the host image should persist.

Mini UART enable, modem control and reset
-----------------------------------------

The BCM2835 AUX mini UART is 16550-like rather than a complete 16550.  The
model retains the low byte written to ``AUX_ENABLES`` and uses bit 0 as the
mini-UART gate.  The gate resets clear.  While it is clear the mini-UART
register bank reads as zero, outgoing data bytes are discarded and incoming
character-backend data is paused.  Implemented control writes are retained,
and interrupt status and the GIC input remain live, matching the Pi 400
behavior measured by this project.  Enabling the UART exposes the retained
state and resumes backend input.  Enable bits 1 and 2 read back, but the two
auxiliary SPI controllers are not implemented.

``AUX_MU_IER`` exposes only its two supported interrupt-enable bits; the FIFO
status bits are reported by ``AUX_MU_IIR`` instead.  The 8-bit scratch
register is read/write.  The model also implements the device's one
modem-control output and one modem-status input: ``AUX_MU_MCR`` bit 1 controls
active-low RTS, and ``AUX_MU_MSR`` bit 4 reports active-low CTS.  Unsupported
bits are ignored and read as zero.  When the selected QEMU character backend
supports serial modem controls, RTS and CTS are passed through its ``TIOCM``
interface; otherwise CTS retains the documented reset status.

A cold reset discards received FIFO data, clears the interrupt enable and
derived interrupt state, lowers the GIC input, clears ``AUX_ENABLES`` and the
scratch register, resets RTS control, and leaves character input paused until
the UART is enabled again.  The FIFO, enable, interrupt, scratch and
RTS-control state migrates with the VM, and post-load processing reconstructs
the IRQ and external RTS output.

Line control, the baud register, automatic flow control and the two auxiliary
SPI controllers remain unimplemented.  The extra-control register retains the
earlier simplified transmit/receive-enabled behavior; GPIO pin-mux timing is
not modeled.

DWC2 core reset
---------------

The on-SoC USB2 host controller implements the observable effects of a DWC2
core soft reset.  It terminates modeled host transfers, clears the global,
host and channel interrupt masks, resets receive-status and frame state, and
deasserts the interrupt while preserving configuration and interrupt-status
registers.  The ``GRSTCTL`` core-reset and receive/transmit FIFO-flush action
bits self-clear as software expects.

This is a DMA-only host model.  It has no separately observable FIFO payload,
so a FIFO-flush command has no additional buffered data to discard.  Slave
mode FIFO accesses and the DWC2 gadget/device register banks remain
unimplemented.

DMA controller
--------------

The BCM2835-compatible DMA engine executes at most 256 transfer operations in
one slice.  An active channel with more work continues from a virtual-clock
timer, so a cyclic control-block ring remains active without trapping the vCPU
or QEMU event loop in the register write that starts it.  Clearing ``ACTIVE``
pauses the current block, setting it resumes from the retained source,
destination and length, and ``ABORT`` selects the next control block.  Global
channel-disable state also pauses and resumes active work.

Named DREQ inputs implement the control block's ``PERMAP``, source and
destination pacing fields.  A low selected request holds the channel and a
rising request executes one bounded slice immediately.  If the request remains
active after that slice, the channel yields and continues from its timer; DREQ
0 is permanently active as specified.
``CS.DREQ`` and ``CS.ISHELD`` report the resulting state.  The model accepts
the wide-memory flags used by Circle and preserves their guest-visible byte
stream, although QEMU memory regions do not expose individual AXI beat widths.
It also supports the controller's byte-aligned, non-word-multiple transfers
and masks control-block pointers to their required 32-byte alignment.

In-flight control-block state, DREQ levels and the partially elapsed
continuation deadline migrate with the VM.  Reset cancels all pending work,
clears channel and interrupt state, and lowers every channel IRQ.  The 1 us
continuation delay is an emulation scheduling quantum, not a claim about exact
DMA bus throughput.  AXI burst shape, wait-cycle timing, panic priority and
bus arbitration remain approximate.

PCM/I2S audio and BCM2711 clocks
--------------------------------

The PCM/I2S block implements its separate 64-word transmit and receive FIFOs,
channel layouts and sample widths, packed stereo mode, FIFO thresholds and
status, sticky errors, interrupt status, DMA requests, delayed FIFO-clear and
``SYNC`` actions, reset, and migration.  In clock-master mode it is driven by
the CPRMAN PCM output.  The ``raspi4b`` and ``raspi400`` machines use the
BCM2711 firmware clock profile measured on the project's Pi 400: a 54 MHz
oscillator, 3 GHz PLLD and 750 MHz ``plld_per``.  BCM2711 also omits the older
feedback predivider, whose register bits have a different purpose on this SoC.

Transmit samples can be sent to any normal QEMU playback backend.  The backend
must be bound to the embedded device explicitly.  For example, this captures
48 kHz stereo playback to a WAV file::

  -audiodev wav,id=i2s,path=pi4-i2s.wav,out.frequency=48000,out.channels=2 \
  -global bcm2835-i2s.audiodev=i2s

Replace ``wav`` with a supported live backend such as ``coreaudio`` when
audible playback is wanted.  The model derives the source sample rate from the
PCM bit clock and frame length; QEMU's audio core converts it to the configured
backend rate and format.

Realtime TCG can deliver a 48 kHz frame timer late.  The model retains an
absolute hardware-frame deadline, catches up elapsed frames in bounded batches
and gives DREQ-paced DMA a chance to refill between FIFO boundaries.  It does
not silently slow the emulated PCM clock to the host callback rate.  This is a
functional timing model, not bit-level emulation of the serial pins.

Circle 51's ``sample/34-sounddevices`` runs through its cyclic DMA I2S path on
both ``raspi4b`` and ``raspi400``.  The pinned 48 kHz sample produces a clean
host WAV stream at the programmed rate, with its modulated tone measured near
440 Hz and without steady-state FIFO underruns.  This exercises the CPRMAN
PLLD divider, two-channel 24-bit PCM framing, cyclic DMA and DREQ pacing rather
than merely proving emulator forward progress.

Receive currently supplies zero samples; host capture is not connected.  PDM
and gray-code modes expose only shallow control/status behavior.  External PCM
clock and frame-sync pins are not modeled, so slave mode needs an explicit
nominal bit-clock frequency, for example::

  -global bcm2835-i2s.slave-clock-frequency=3072000

This advances frames periodically at the configured rate and does not model
individual external clock or frame-sync edges.  Standby settling time,
bit-exact gray/PDM data paths and channel-slip recovery remain approximate.
The hardware FIFOs, controller deadlines and pending control actions migrate;
samples already staged only in the host audio backend do not.

Firmware clock and power state
------------------------------

The VideoCore property interface stores the enable state reported by
``GET_CLOCK_STATE`` and changed by ``SET_CLOCK_STATE``.  Known clocks start
enabled, preserving the behavior of the earlier stateless implementation;
invalid clock IDs report the firmware's not-present bit.

``GET_DOMAIN_STATE`` and ``SET_DOMAIN_STATE`` similarly track the 23 firmware
power-domain IDs.  The initial enabled set is video scaler, VPU1, USB,
transposer and ARM, matching the state captured from the project's Pi 400
after a normal Linux boot.  ``NOTIFY_REBOOT`` is accepted as an explicit
no-op because QEMU has no VideoCore firmware execution state to quiesce.

Clock and domain state resets with the machine and migrates with the VM.  A
machine reset also discards a property response stalled behind a full ARM
mailbox and lowers its child interrupt, so a request from the new boot cannot
be blocked by the previous one.

This is a control-plane compatibility model, not functional clock or power
gating.  Turning off the ARM clock does not stop a vCPU, and turning off a
domain does not hide, reset or suspend the corresponding QEMU device.  The
captured domain defaults are a useful runtime reference, not a claim about
every Raspberry Pi firmware version or every point during boot.

Ethernet
--------

GENET is both machines' on-board network device. Connect it to
any normal QEMU network backend; for example, unprivileged user-mode
networking is selected with::

  -nic user,model=genet

The model implements the BCM2711 GENET v5 register layout, the external MDIO
PHY, link state, descriptor DMA, interrupts, scatter-gather transmission and
checksum offload.  Upstream Linux can acquire a DHCP lease through it.  MIB
counters, wake-on-LAN and hardware receive filtering are not yet modeled.

GPIO
----

The BCM2711 GPIO model exposes all 58 pins as QEMU input and output lines.
``GPSET`` and ``GPCLR`` update an output latch even while a pin is configured
as an input; the retained value takes effect when the pin becomes an output.
``GPLEV`` reports the external level for inputs and the latch level for
outputs.

The model implements the two ``GPEDS`` event-status registers and all six
rising, falling, high, low, asynchronous-rising and asynchronous-falling
detector pairs.  Event status is write-one-to-clear.  An active high or low
condition immediately restores its status bit, matching the level-detector
contract.  The QEMU GPIO interface conveys logical level transitions rather
than sub-clock pulse widths, so synchronous and asynchronous edge detectors
have the same transition behavior; clock-sampling and glitch-filter timing
are not modeled.

The three bank interrupts cover GPIOs 0--27, 28--45 and 46--57 and are wired
to GIC SPIs 113, 114 and 115.  SPI 116 is asserted whenever any bank has an
event.  Detector configuration, event status and output latches reset to
their hardware defaults, while externally driven input levels persist across
a controller reset.  All GPIO state and asserted interrupts are restored by
live migration.

Random number and thermal sensors
---------------------------------

The upstream Linux ``iproc-rng200`` driver can select the on-SoC RNG and read
``/dev/hwrng``.  RNG data is generated through QEMU's guest-randomness API;
the device's already-produced FIFO data and next refill deadline migrate with
the VM.

The AVS monitor reports 35050 millidegrees Celsius by default, corresponding
to raw code 770 and the BCM2711 device-tree calibration.  Its QOM
``temperature`` property is exposed at
``/machine/soc/peripherals/thermal`` in millidegrees Celsius.  Setting it
through QMP updates the raw ten-bit reading with the sensor's 487-millidegree
quantization.  The upstream ``bcm2711_thermal`` driver exposes the result as
the ``cpu-thermal`` thermal zone.
