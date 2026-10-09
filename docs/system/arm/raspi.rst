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
hardware.  Its minimal EL2 and EL1 paths have now passed the QEMU safety gate,
so its first real Pi 400 boot is gated only on the physical-run checklist.

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
 * BCM2711 HVS, HDMI0/HDMI1 pixel valves and transmitters, including RGB and
   linear YUV multi-plane composition, coefficient-driven PPF/TPZ scaling,
   256-byte-column RGB addressing and native Linux VC4 DRM scanout
 * BCM2711 HDMI DVP clock/reset controller and both HDMI DDC I2C controllers,
   with virtual EDID monitors gated by each transmitter's runtime connection
   state; HDMI0 starts connected and HDMI1 disconnected
 * BCM2711 HDMI0 MAI audio, with a 64-word FIFO, DMA DREQ pacing and PCM
   playback through a QEMU audio backend
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
   coherent OTP-backed board identity, clock and power-domain state, reboot
   notification, and the Pi 4-family VL805 initialization notification
 * Peripheral SPI controller (SPI) and bounded PIO AUX SPI1/SPI2 controllers
 * Broadcom Serial Controller (I2C)
 * BCM2711 RNG200 random number generator, including its 16-word FIFO,
   four generation rates, status and interrupt registers, soft resets, and
   migratable refill timer and FIFO contents
 * BCM2711 AVS thermal monitor, using the device-tree calibration and a
   migratable, configurable temperature reading
 * BCM2711 PCIe host and root port, including dynamic outbound and inbound
   DMA windows, INTx, root-port service events, and MSI
 * Pi 4-family VIA VL805 PCIe xHCI personality, including the captured PCI and
   xHCI register layout, multi-segment event rings, DMA-backed controller
   events, MSI, PERST, migration state, and a guest-visible PCIe device-tree
   node
 * VIA ``2109:3431`` four-port high-speed USB hub on both boards
 * Raspberry Pi 400 ``04d9:0007`` low-speed integrated keyboard, including
   functional keyboard and consumer-control HID interfaces

Missing devices
---------------

 * V3D 4.2 graphics accelerator.  Its Pi 400-identifying hub/core register
   substrate and shared interrupt are modeled, but the device-tree node stays
   disabled because command-list execution and Mesa acceleration are not yet
   modeled.
 * Remaining native-display features: compressed and column-addressed YUV
   formats; exact TPZ and blend rounding in some captured cases; physical
   LBM/FIFO behavior and full scanline events; pixel valves 0, 1 and 3;
   interlaced and deep-colour timing; HDMI1 audio; HPD interrupt edges, CEC,
   signal-level TMDS and HDMI audio-packet transport
 * Physical BCM2711 PCIe power-management and link-training event behavior
 * AUX SPI DMA, fixed-width or LSB-first framing, clock timing, GPIO pin-mux
   and native chip-select wiring

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

Pi 400 desktop view on macOS
----------------------------

With ``-machine raspi400 -display cocoa``, one Cocoa window contains an HDMI
monitor bezel and the Pi 400 keyboard.  Host typing and clicking a displayed
key use the board's built-in ``04d9:0007`` USB HID keyboard; the same input
event both reaches the guest and highlights its physical key on screen.  The
displayed key printing matches the local German Pi 400 keyboard, including
``PrtScn/SysRq`` and the physical navigation cluster.

The boxed ``1`` (Num Lock) and ``A`` (Caps Lock) indicators reflect the
guest's HID output reports.  The third, power-symbol LED is lit while QEMU is
running; Pi 400 has no Scroll Lock LED.  To add an external, standard USB
mouse on a spare Pi 400 hub port, for example, use::

  -device usb-mouse,bus=vl805.0,port=1.1

The Cocoa frontend forwards its movement only while the host pointer is over
the HDMI panel, so it remains available for normal macOS use outside it.
When the Pi 400 window is active, macOS volume, mute, play/pause,
next/previous-track and eject keys drive the keyboard's separate
consumer-control HID interface.

Mini UART enable, modem control and reset
-----------------------------------------

The BCM2835 AUX mini UART is 16550-like rather than a complete 16550.  The
model retains the low byte written to ``AUX_ENABLES`` and uses bit 0 as the
mini-UART gate.  The gate resets clear.  While it is clear the mini-UART
register bank reads as zero, outgoing data bytes are discarded and incoming
character-backend data is paused.  Implemented control writes are retained,
and interrupt status and the GIC input remain live, matching the Pi 400
behavior measured by this project.  Enabling the UART exposes the retained
state and resumes backend input.

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

Line control, the baud register and automatic flow control remain
unimplemented.  The extra-control register retains the earlier simplified
transmit/receive-enabled behavior; GPIO pin-mux timing is not modeled.

AUX SPI1/SPI2 PIO
-----------------

``AUX_ENABLES`` bits 1 and 2 gate the SPI1 and SPI2 register banks.  A gated
bank reads as zero and ignores writes; state written while the controller is
enabled is visible again after re-enabling it.  As on the measured Pi 400,
the defined shared-AUX interrupt bit can remain asserted while the register
bank is gated.  The model deliberately omits the hardware's observed reserved
bit 31 from ``AUX_IRQ``.

The bounded PIO subset implements ``CNTL0``, ``CNTL1``, ``STAT``, ``PEEK``,
``IO`` and ``TXHOLD`` at the documented SPI1/SPI2 offsets.  It accepts the
one-, two- and three-byte MSB-first variable-width transfers used by Linux's
``spi-bcm2835aux`` driver, synchronously exchanges them with a QEMU SSI bus,
and provides the driver's twelve-byte receive FIFO, status bits, shared
interrupt, cold reset and migration behavior.  A word is sent from the most
significant bits but received into the least significant ones, so an n-byte
result is returned right-aligned, as ``spi-bcm2835aux`` reads it back.  The
receive FIFO holds bytes rather than words, so a read drains up to three of
them irrespective of the widths that produced them; the driver only ever
shortens its final word, so its own ``min(pending, 3)`` accounting matches.  QEMU exposes the buses as
``spi1`` and ``spi2``.  For a lab-only serial-flash transaction, for example,
an explicitly requested standard QEMU device can be attached with::

  -device m25p80,id=spi1flash,bus=spi1

For such an explicit virtual SSI device, ``TXHOLD`` keeps the selected virtual
chip select asserted and the final ``IO`` word deasserts it.  This follows the
transaction boundary used by the Linux PIO driver; it does not model physical
GPIO routing.  The controller state, virtual chip-select state and a held
standard-M25P80 transaction migrate together.

That boundary is one ``spi_transfer``, not one SPI message, because
``spi-bcm2835aux`` writes the last word of every transfer to ``IO``.  A guest
message built from separate command and data transfers therefore drops the
native chip select in between, and a SPI-NOR child bound to this controller
cannot complete a JEDEC or read command.  This matches the hardware the driver
describes, which is why it asks for ``cs-gpios``; a single full-duplex
transfer is what the native chip select can span.

The upstream Pi 4 device-tree nodes remain disabled by default, as on the
board, so a guest must use its normal device-tree overlay or other DT change
to enable a controller.  This is not a pin-level model: SPI DMA, other
framing modes, wire timing, GPIO function selection and physical chip-select
routing remain unimplemented.  An attached QEMU SSI device is therefore a
bounded test/lab peripheral rather than a claim of native GPIO wiring.

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
hotplug interrupt descriptions.  The transmitter's hot-plug state is settable
at run time, but detection is polled: no connect, disconnect or CEC edge is
delivered through this controller.  It therefore supplies the Linux-visible
interrupt topology without claiming HPD interrupt or CEC emulation.

Native HDMI scanout, DVP clocks and DDC
--------------------------------------

The native display path exposes the HVS at ARM physical address
``0xfe400000``, HDMI0 pixel valve 2 at ``0xfe20a000``, HDMI1 pixel valve 4 at
``0xfe216000`` and HDMI transmitter register banks beginning at
``0xfef00200`` and ``0xfef05700`` respectively.  Their device-tree nodes
remain visible, while pixel valves 0, 1 and 3 and V3D stay hidden.
A non-executing V3D 4.2 hub/core register substrate is
present at its real addresses, but its device-tree node remains disabled until
the command-list engine can be modeled faithfully.

V3D developer probe
-------------------

For driver-facing regression testing only, the V3D node can be explicitly
retained with ``-global bcm2711-v3d.enable-probe-dtb=true``.  The pinned Linux
driver then reads the Pi 400-identifying hub/core registers, initializes its
MMU and interrupt state, and registers its DRM device.  The stateful ASB
bridge model supplies the stop/acknowledge handshakes used by the Linux V3D
power-domain driver.  Both bridge blocks reset to the per-domain control
values read from a real Pi 400, and a request to stop is acknowledged
immediately because no transaction is ever in flight.  Note that the two
blocks disagree about V3D: on BCM2711 the power driver reaches the V3D
bridges through the RPiVid block, so the main block's V3D words are
firmware-left state that Linux never writes on this SoC.

The option does **not** enable usable 3D graphics: command-list execution,
memory accesses by GPU jobs, fences and rendering are still absent.  Do not
run Mesa or submit V3D work with it; use it only to exercise the driver-probe
path.  The following headless test verifies that bounded contract on both Pi 4
machine types without submitting a GPU job::

  scripts/pi4/test-v3d-probe.py --qemu build/qemu-system-aarch64 \\
      --machine raspi4b
  scripts/pi4/test-v3d-probe.py --qemu build/qemu-system-aarch64 \\
      --machine raspi400

The HVS consumes the Linux VC4 driver's channel display lists.  Output 4's
``DISPEOLN`` mux selects the channel for HDMI0; output 5's ``DISPDITHER`` mux
selects HDMI1, with value 3 disabling either output.  The primary console is
the existing Raspberry Pi framebuffer; HDMI1 has a separate console named
``hdmi1-fb``, selectable with the QMP ``screendump`` command's ``device``
argument.  The implemented HVS5 subset composites up to sixteen planes in
display-list order with output clipping and nonnegative destination positions.
Source and output dimensions are bounded at 3840x2560.

All sixteen RGB formats exercised by the Pi 400 captures are supported,
including RGB332, RGB/BGR565, 5551, 888, 8888 and 1010102 layouts and the
driver's fixed-alpha variants.  Linear NV12/NV21, NV16/NV61, YUV/YVU420,
422 and 444 are supported through their luma/chroma pointers, pitches,
independent filters and programmed CSC words.  The captured BT.601, BT.709
and BT.2020 limited/full-range matrices match.  Components remain at twelve
bits through filtering, colour conversion and blending, then reduce to eight
bits for console output.  Fixed and per-pixel alpha, coverage, premultiplied
alpha and plane-alpha mixing are implemented.

PPF scaling reads the guest's signed nine-bit coefficient tables, programmed
scale and fractional phase, coefficient interpolation and gain correction.
TPZ downscaling uses the programmed fixed-point scale and reciprocal.  Both
filters clamp and round between axes; horizontal filtering runs first when it
does not enlarge the image, otherwise vertical filtering runs first.  Vertical
reflection reads source lines upward from the driver-adjusted pointer;
horizontal reflection mirrors the filtered output.  Fractional crops are
represented by the source pointer and phase.  The project's old short RGB
test lists retain their nearest-neighbour fallback.

Pi 400 captures with the HVS tiling field set to 3 show 256-byte-column RGB
addressing rather than the T-format memory layout described by the driver's
modifier name.  The renderer follows those observed addresses, including
column crossings, source-pointer crops, reflection and scaling.  The low
``PITCH0`` halfword supplies the column stride in rows; bits 22:16 select the
additional row step.  That row-step formula has only been observed for field
values 0, 1 and 2.  Column-addressed YUV and compressed formats are not
implemented.  These are constrained hardware observations, not a complete
specification of all tiling encodings.

Display-list RAM, controls and active-list pointers migrate.  The destination
reconstructs filter state and scanout from these registers and migrated guest
RAM, including the image and EOF state of a completed one-shot channel.
A list containing an unsupported format, tiling encoding or alpha mode
leaves the previous scanout configuration unchanged.

Pixel valves 2 and 4 supply sticky, write-one-to-clear VFP-start interrupts on
GIC SPIs 101 and 110.  Their frame periods derive from the programmed
horizontal and vertical totals and each HDMI PHY/RM pixel clock.  The rate
calculation follows Linux's 54 MHz reference, RM offset and VCO divider for
eight-bit TMDS; both pixel valves consume two pixels per clock.  Supported
frame periods are bounded between 1 ms and 1 s.  Disabling
the HDMI clock stops a mode with programmed timings.  Register-only tests
without timing totals retain a 16,666,667 ns fallback.  Events continue while
the interrupt remains pending, and acknowledgement preserves the next frame's
deadline.  Interlace, deep colour and physical scanline timing remain outside
this functional model.

Each pixel valve's frame event drives its routed HVS channel.  HVS EOF and
short-frame status, per-channel interrupt masks and summaries, the global
IRQ enables, write-one-to-clear status and one-shot channel completion are
implemented on GIC SPI 97.  Physical FIFO underrun, LBM allocation and the
remaining scanline events are not simulated.

Both HDMI transmitters expose their independent register banks and consume
their own DVP clock-enable and reset signals.  HDMI0 starts connected;
HDMI1 starts disconnected.  Each accepts a runtime QOM ``connected``
property change at ``/machine/soc/peripherals/hdmi0`` or ``hdmi1``, which a
polled guest observes through ``HDMI_HOTPLUG``.  DDC transfers NACK when the
corresponding connector is disconnected.  Most transmitter registers retain
control state rather than encoding an HDMI signal; TMDS, blanking-interval
packets and a physical monitor are not modeled.  The shared HD/MAI window
belongs to HDMI0, whose separate audio model is described below.  HDMI1's
virtual monitor does not advertise audio.

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

Each connector contains QEMU's standard virtual DDC monitor at address
``0x50``, gated by the transmitter's connection state.  The virtual EDIDs are
QEMU display contracts, not captures of the project's physical monitor.
HDMI0's 256 bytes contain a CTA-861 revision 3 extension with Basic Audio,
a two-channel LPCM short audio
descriptor for 32, 44.1 and 48 kHz at 16, 20 and 24 bits, front-left/right
speaker allocation and an HDMI vendor data block.  Linux 7.2 binds
``brcm2711-dvp`` and both ``brcmstb-i2c`` instances on ``raspi4b`` and
``raspi400``.  The acceptance init performs the normal combined pointer-write
and two 128-byte reads through ``/dev/i2c-*``, exercises eight hardware-sized
chunks, validates both checksums and parses the HDMI and audio capabilities.

DVP, both HDMI transmitters, HVS, pixel-valve and DDC register state, pixel
clocks, vblank deadlines, open I2C transactions and EDID cursors migrate.  Reset
closes an active DDC transaction and returns each engine to auto-I2C
ownership.  Focused qtests cover register masks, reset, ownership, ACK/NACK
behavior, malformed-length cleanup, chunked EDID access, independent HDMI1
scanout, coefficient-driven filtering, HVS frame events, mode-derived IRQ
timing and migration with an active display pipeline.  Fifty-five fixtures
contain source buffers and sampled golden output pixels from Pi 400 writeback
captures; these compare directly with hardware data at zero tolerance.

The pinned upstream Linux 7.2 image binds HVS, HDMI0/HDMI1, TXP and pixel
valves 2 and 4, registers ``/dev/dri/card0`` and creates a 1280x800 RGB565
``/dev/fb0`` on
both machines.  The acceptance init checks the connector, preferred mode and
framebuffer geometry.  A separate end-to-end gate writes deterministic red,
green, blue and white bands to the primary framebuffer, uses the production
DRM UAPI to attach a second RGB565 plane, and asks VC4 to scale that 320x200
source to 640x400 at ``(320, 200)``.  It takes a QMP screendump and validates
unobscured primary samples plus green, blue, white and black overlay
quadrants::

  scripts/pi4/test-display.py --qemu build/qemu-system-aarch64 \
      --machine raspi4b
  scripts/pi4/test-display.py --qemu build/qemu-system-aarch64 \
      --machine raspi400

Add ``--hdmi1`` to connect the second virtual monitor before Linux boots.  The
same gate then checks HDMI1's independent 1280x800 console, including primary
pixels in regions covered by HDMI0's overlay.  With ``--output`` it retains a
second screendump with ``-hdmi1`` appended to the filename stem.

This is native Linux-programmed scanout within the bounded display subset
described above.  The October 9, 2026 replay of 334 Pi 400 writeback captures
matches 301 images exactly.  Twenty-four captures retain one-count TPZ or blend
differences.  Nine column-layout captures require addresses outside their
saved source buffers under the inferred layout, so their remaining differences
require controlled recapture before they can validate the addressing model.
Those counts describe this capture set, not every possible guest configuration.
The local replay tool is
``scripts/pi4/hvs-diff.py replay``; per-capture results and SHA256 identifiers
are saved in the outer workspace's
``harnesses/hvs-diff-20261009/validation`` directory.

Negative destination coordinates, HPD interrupt edges, CEC, signal-level TMDS
and audio-packet transport, and V3D command execution remain unmodeled.  The
DDC controller also omits the combined hardware DTF encodings and ten-bit
I2C addressing.

HDMI0 MAI audio
---------------

The HDMI0 transmitter's shared HD register bank contains a functional MAI
audio subset.  It implements the control, threshold, format and data
registers, a 64-word FIFO, busy/empty/full status, sticky underflow and
overflow status, write-one-to-clear errors, and reset and flush commands.  A
running stream consumes complete frames at the selected virtual-time rate.
Low and high FIFO thresholds provide hysteretic DMA DREQ 10 pacing to the
BCM2835-compatible DMA engine, so the production Linux driver exercises the
same MAI/DMA path rather than an acceptance-only shortcut.

All fifteen MAI rate selectors from 8 kHz through 192 kHz and one through
eight interleaved channels are accepted.  IEC958 validity-marked subframes
produce silence.  Mono is duplicated to the host stereo output; for wider
streams the first two channels reach the host and the remaining words are
consumed to preserve FIFO and DMA pacing.  PCM reaches QEMU's audio core as
signed 32-bit stereo.  Subframe preamble and parity checking, channel-status
interpretation, non-PCM/HBR decoding and an HDMI packet or link model remain
unimplemented.

Select an explicit backend for HDMI audio when other Pi audio devices are
also present.  For example, this captures HDMI0 and assigns the unrelated I2S
and PWM devices to a silent backend::

  -audiodev wav,id=hdmi,path=pi4-hdmi.wav,out.frequency=48000,out.channels=2 \
  -audiodev none,id=silent \
  -global bcm2711-hdmi.audiodev=hdmi \
  -global bcm2835-i2s.audiodev=silent \
  -global bcm2835-pwm.audiodev=silent

The pinned Linux 7.2 image sees the CTA audio advertisement, registers
``vc4-hdmi-0`` and opens its IEC958 PCM device at 48 kHz stereo.  Its
one-second test stream uses different square-wave frequencies and amplitudes
on the two channels.  The dedicated host gate requires production-driver
playback to complete, then verifies the WAV format, duration, continuity,
channel frequencies, amplitudes and RMS ratio on both board models::

  scripts/pi4/test-audio.py --qemu build/qemu-system-aarch64 \
      --machine raspi4b
  scripts/pi4/test-audio.py --qemu build/qemu-system-aarch64 \
      --machine raspi400

MAI register state, FIFO contents, the fractional-rate accumulator and the
next pacing deadline migrate.  Post-load re-establishes the derived DREQ and
host voice state.  Samples already accepted only by the host audio backend do
not migrate.  Focused qtests cover thresholds, FIFO status and errors,
bounded timer catch-up, DVP and MAI resets, DMA completion and migration of an
active stream.

Firmware board identity and OTP
-------------------------------

The Pi 4 firmware property device initializes the factory identity rows of
the BCM2835-compatible OTP model.  Row 28 contains the lower 32-bit serial,
row 29 its ones' complement and row 30 the machine's board revision.  Both
``GET_BOARD_REVISION`` and ``GET_BOARD_SERIAL`` read those rows, so mailbox
and OTP-backed identity cannot disagree.  The serial response is 64 bits with
an upper word of zero, matching the documented firmware property response.

The default serial is the deterministic synthetic value ``0x51454d55``
(``QEMU`` in ASCII).  Give separate virtual boards distinct identities when
software relies on uniqueness, for example::

  -global bcm2835-property.board-serial=0x12345678

Do not copy the serial of a physical board unless deliberately cloning that
identity.  Factory and customer OTP rows retain e-fuse semantics: writes can
only set bits, survive a system reset and migrate with the VM.  The raw OTP
controller register interface remains unimplemented because its programming
protocol is not publicly documented; this functional identity support does
not claim a Linux nvmem-accessible OTP controller.

The firmware ``GET_BOARD_MODEL`` tag returns zero on the Pi 4 family; the
``GET_BOARD_REVISION`` value is the useful board discriminator.  This matches
the Pi 400 capture and avoids inventing a model number that the firmware does
not provide.

Firmware property buffers
-------------------------

The property mailbox parser validates the complete top-level header, every tag
header, four-byte padding and the terminating end tag before dispatching a
request.  It rejects malformed requests with the documented partial-response
code ``0x80000001`` and bounds the total request below 1 MiB so a guest cannot
turn a property MMIO write into an unbounded host walk.  Supported responses
report their full desired length while writes are truncated to the guest's
declared value buffer.  Unknown and deliberately unimplemented tags leave the
tag response bit clear.  The command-line tag keeps the Pi firmware's
all-or-nothing short-buffer behavior as a documented compatibility exception.

Framebuffer palette Test/Set requests require the published 24..1032-byte
value area, validate ``offset + length`` as one interval within 256 entries,
and stage all colors until tag dispatch completes and the palette request is
accepted.  Invalid or truncated requests therefore do not partially modify
palette memory.  A Set whose palette destination directly overlaps its own
property message is rejected because the update and response cannot both
occupy those bytes.

Framebuffer operation handling implements the published ordering and
tag-combination rules for modeled configuration fields and palette state.  All
modeled state-changing Set inputs are staged before any framebuffer Get
response, regardless of their order in the request, so a Get observes the
resulting configuration and palette.  The generic tag walker still dispatches
tags in guest order.  Mixing framebuffer Test tags with Get or Set tags is
rejected with response code ``0x80000001`` without applying or returning any
tag, as are duplicate framebuffer tag IDs.  Modeled Test/Set fields are
collected into one temporary configuration and passed together through the
existing configuration validator.  As a fork hardening policy, a later
ordinary-tag input-validation or memory-access failure also discards staged
framebuffer state; this is not claimed as a firmware guarantee.  Ordinary
property groups such as VCHIQ retain normal in-order processing.  Grouping a
framebuffer/display tag does not make an otherwise unhandled tag implemented,
and this work does not remove existing configuration-validation or allocation
limitations.

The project's Pi 400 probes show that short responses are tag-specific in the
real firmware: some tags truncate, some preserve the request, and some return
an error without touching the buffer.  The fork follows the published safe
mailbox contract rather than reproducing firmware padding overruns.  The
corresponding current-upstream framebuffer ordering mismatch remains recorded
as :doc:`raspi-upstream` item ``QP4-UP-037``.

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
