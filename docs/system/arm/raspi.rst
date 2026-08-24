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
 * Both BCM2711 PWM controllers, with FIFO and DMA-paced stereo playback
 * BCM2711 always-on edge-latched L2 interrupt controller, with independently
   masked CPU and PCI banks
 * BCM2711 HVS, HDMI0 pixel valve and HDMI0 transmitter, sufficient for a
   native Linux VC4 DRM scanout to a QEMU display
 * BCM2711 HDMI DVP clock/reset controller and both HDMI DDC I2C controllers,
   with a connected virtual EDID monitor on HDMI0
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
 * Remaining native-display features: HVS scaling, multi-plane composition and
   tiled formats; pixel valves other than HDMI0's; HDMI1; dynamic hotplug, CEC
   and HDMI audio data
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

PWM controllers and audio
-------------------------

BCM2711 has two two-channel PWM controllers.  ``PWM0`` is mapped at
``0xfe20c000`` and drives DMA request 5.  ``PWM1`` is mapped at
``0xfe20c800`` and drives DMA request 1, matching the reset selection of the
BCM2711 DMA request mux.  Both blocks use the CPRMAN PWM clock and have the
BCM2711 64-word shared FIFO.  Reset values, the two FIFO read identifiers and
the 64-word full boundary were checked against the project's Pi 400.  A
single-word timing probe also showed that enabling a FIFO-driven channel
immediately moves its queued word into the channel: the FIFO reports empty
while the channel reports active.  The model follows that observed boundary
and likewise claims a new word immediately when an enabled, idle channel is
waiting for data.

The model implements the control, status, DMA-threshold, range, data and FIFO
registers.  This includes FIFO full and empty state, sticky write and gap
errors, write-one-to-clear status, the one-shot FIFO clear, data-register and
FIFO modes, polarity, silence state, repeat-last behavior, PWM and serialiser
average output, and locked-step FIFO sharing between both channels.  DREQ is
asserted at the programmed FIFO threshold and can pace the BCM2835-compatible
DMA engine.  FIFO contents, current channel data, period phase and output state
migrate with the VM.

Each channel exposes read-only ``freq[0]``, ``freq[1]``, ``duty[0]`` and
``duty[1]`` QOM properties.  Duty uses a scale of zero to one million.  The
same value is emitted on the device's ``duty-gpio-out`` lines, allowing a
board or test fixture to consume the functional average output without
requiring bit-level transitions at the PWM source clock.

On Pi 4-family boards the analogue headphone output is driven by ``PWM1``.
Its synchronized, two-channel FIFO stream can be sent to any normal QEMU
playback backend.  The embedded I2S device must also have a backend when an
explicit PWM backend is selected.  For example, this captures PWM audio while
leaving I2S disconnected::

  -audiodev none,id=i2s \
  -global bcm2835-i2s.audiodev=i2s \
  -audiodev wav,id=pwm,path=pi4-pwm.wav,out.frequency=48000,out.channels=2 \
  -global bcm2835-pwm.audiodev=pwm

The source sample rate is derived from the CPRMAN PWM clock and the common
channel range; QEMU's audio core converts it to the configured backend rate
and format.  Circle 51's PWM driver and cyclic-DMA sound path run on both
``raspi4b`` and ``raspi400``.  With its 48 kHz configuration, the captured
stereo stream contains the expected modulated tone near 440 Hz without
steady-state FIFO gaps.

This is a functional period and average-duty model, not a 125 MHz pin-edge
waveform model.  Host audio currently requires two FIFO-driven PWM-mode
channels with equal nonzero ranges.  Differing ranges in a shared FIFO are
paced at the slower channel, approximating the documented locked-step gaps.
The alternative DSI0 selection for DMA request 1, DMA panic priority, APB
synchronizer bus-error timing and GPIO alternate-function routing are not yet
modeled.  Reading the write-only FIFO returns the observed ``pwm0`` or
``pwm1`` bus identifier; there is no FIFO read-data path.

Always-on HDMI L2 interrupt controller
--------------------------------------

The BCM2711 always-on interrupt controller is mapped at ARM physical address
``0xfef00100`` and is exposed through the upstream device-tree node at GPU bus
address ``0x7ef00100``.  It has twelve edge-latched sources and two register
banks, one for the CPU destination and one for the PCI destination.  Each bank
has independent status, software-set, clear, mask-status, mask-set and
mask-clear registers.  A physical rising edge latches its bit in both banks;
software set and clear operations affect only the selected bank.  An output is
asserted while its bank contains any unmasked pending bit.

The twelve-bit implemented mask, write-one set/clear behavior and read-as-zero
action registers were checked against the project's Pi 400.  A CEC transmit
probe with the corresponding child interrupt masked showed the same physical
edge latched in both the CPU and PCI status banks.  The model exposes all
twelve physical inputs and both bank outputs.  The CPU output is connected to
GIC SPI 96; the PCI output remains available at the device boundary but has no
board-level destination until the corresponding consumer is modeled.

Reset clears both status banks, masks every implemented source and preserves
the externally driven input levels without inventing another edge.  Pending
state, masks and input levels migrate, and destination-side outputs are
reconstructed after loading.  Focused qtests cover physical and software
events, masking, clearing a held-high source, reset and migration.

The node remains present in supplied Pi 4-family device trees, and the pinned
upstream Linux 7.2 image registers its ``irq_brcmstb_l2`` driver on both
``raspi4b`` and ``raspi400``.  HDMI0 uses the controller as the parent for its
hotplug interrupt descriptions, but the transmitter has a fixed connected
state and does not generate connect, disconnect or CEC edges.  The controller
therefore supplies the Linux-visible interrupt topology without claiming
dynamic hotplug or CEC emulation.

Native HDMI0 scanout, DVP clocks and DDC
----------------------------------------

The native display path exposes the HVS at ARM physical address
``0xfe400000``, HDMI0 pixel valve 2 at ``0xfe20a000`` and the HDMI0
transmitter register banks beginning at ``0xfef00200``.  Their upstream
device-tree nodes remain visible, while the other four pixel valves, HDMI1
and V3D stay hidden.

The HVS consumes the channel display list programmed by the Linux VC4 driver
and redirects a supported primary plane to QEMU's existing Raspberry Pi
framebuffer console.  The implemented subset is a top-left, full-screen,
unity-size, linear single plane in RGB565, RGB888 or RGBA8888 format with the
channel orders used by Linux.  The display-list RAM, channel controls and
active-list pointers are guest visible and migrate.  An unsupported primary
plane leaves the previous scanout unchanged.  If more planes follow a
supported primary plane, multi-plane composition is reported as unimplemented
and only that first plane is displayed.

Pixel valve 2 retains its programmed register state and supplies the
write-one-to-clear VFP-start interrupt used by Linux.  While both video-enable
bits are set, it schedules one event every 16,666,667 virtual nanoseconds and
wires the resulting IRQ to GIC SPI 101.  This is a fixed approximately 60 Hz
functional vblank source rather than timing derived from the programmed mode.
The HVS IRQ is wired to GIC SPI 97, although HVS-generated interrupt events
are not yet modeled.

The HDMI0 model exposes all register banks described by the BCM2711 device
tree and implements the hotplug, FIFO, packet-status and scheduler responses
needed by the Linux HDMI driver.  It starts connected and consumes HDMI0's
DVP clock-enable and reset signals.  Most transmitter registers are retained
control state rather than a signal-level HDMI encoder; there is no TMDS,
blanking-interval, audio-packet or physical-monitor model.

The HDMI DVP clock/reset controller is mapped at ARM physical address
``0xfef00000``.  It exposes six software-reset bits and two active-low HDMI
108 MHz clock gates.  Its reset state uses the firmware-configured idle values
captured from the project's Pi 400: control ``0x00000200``, software reset
zero, both clock-disable bits set in miscellaneous configuration, and spare
``0xffff0000``.  Writes to the implemented fields update named reset and
clock-enable outputs so later HDMI devices can consume them without changing
the guest-visible controller contract.

The two HDMI DDC BSC engines are mapped at ``0xfef04500`` and ``0xfef09500``;
their corresponding auto-I2C ownership windows are at ``0xfef00b00`` and
``0xfef05b00``.  The model implements the eight 32-bit input and output data
registers, 32-byte transfer chunks, seven-bit addressed reads and writes,
START, repeated START, no-START and no-STOP sequencing, ACK/NACK status,
ignore-ACK mode, clock-control readback and the BCM2711 ownership-release
operation.  Transfers complete synchronously because the Pi 4 device tree
supplies no DDC interrupt and Linux uses the controller's polling path.

HDMI0 contains QEMU's standard virtual DDC monitor at address ``0x50``;
HDMI1 has no target by default and therefore reports NACK, representing a
disconnected connector.  The virtual EDID is a QEMU display contract, not an
EDID captured from the project's physical monitor.  Linux 7.2 binds
``brcm2711-dvp`` and both ``brcmstb-i2c`` instances on ``raspi4b`` and
``raspi400``.  The acceptance init performs the normal combined pointer-write
and 128-byte read through ``/dev/i2c-*``, which exercises four hardware-sized
chunks and validates the EDID header and checksum.

DVP, HDMI-transmitter, HVS, pixel-valve and DDC register state, the vblank
deadline, an open I2C transaction and the attached EDID cursor migrate.  Reset
closes an active DDC transaction and returns each engine to auto-I2C
ownership.  Focused qtests cover register masks, reset, ownership, ACK/NACK
behavior, malformed-length cleanup, chunked EDID access, vblank IRQ timing and
migration with an active display pipeline.

The pinned upstream Linux 7.2 image binds HVS, HDMI0, TXP and pixel valve 2,
registers ``/dev/dri/card0`` and creates a 1280x800 RGB565 ``/dev/fb0`` on
both machines.  The acceptance init checks the connector, preferred mode and
framebuffer geometry.  A separate end-to-end gate writes deterministic red,
green, blue and white bands, takes a QMP screendump and validates pixels from
each band::

  scripts/pi4/test-display.py --qemu build/qemu-system-aarch64 \
      --machine raspi4b
  scripts/pi4/test-display.py --qemu build/qemu-system-aarch64 \
      --machine raspi400

This is native Linux-programmed scanout, but it remains a deliberately small
display-pipeline subset.  Scaling, arbitrary plane positions, composition,
tiled and compressed formats, mode-derived timings, HDMI1, dynamic HPD, CEC,
HDMI audio data and V3D are not modeled.  The DDC controller also omits the
combined hardware DTF encodings and ten-bit I2C addressing.

Firmware clock and power state
------------------------------

The VideoCore property interface reports the BCM2711 firmware clock inventory
captured through ``/dev/vcio`` on the project's Pi 400.  ``GET_CLOCKS``
discovers IDs 1 through 15; the display clock ID 16 is absent.  Current,
minimum and maximum rates use the captured per-clock profile rather than a
single generic fallback.  ``SET_CLOCK_RATE`` retains the requested rate,
clamps it to that clock's captured range and returns the resulting rate.

The interface also stores the enable state reported by ``GET_CLOCK_STATE``
and changed by ``SET_CLOCK_STATE``.  Known clocks start enabled for guest
compatibility; invalid clock IDs report the firmware's not-present bit.

``GET_DOMAIN_STATE`` and ``SET_DOMAIN_STATE`` similarly track the 23 firmware
power-domain IDs.  The initial enabled set is video scaler, VPU1, USB,
transposer and ARM, matching the state captured from the project's Pi 400
after a normal Linux boot.  ``NOTIFY_REBOOT`` is accepted as an explicit
no-op because QEMU has no VideoCore firmware execution state to quiesce.

Clock rates, clock state and domain state reset with the machine and migrate
with the VM.  A machine reset also discards a property response stalled behind
a full ARM mailbox and lowers its child interrupt, so a request from the new
boot cannot be blocked by the previous one.

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
