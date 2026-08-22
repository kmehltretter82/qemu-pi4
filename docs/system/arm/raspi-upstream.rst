Raspberry Pi upstream candidate tracker
========================================

This page records changes discovered while developing ``qemu-pi4`` that may
belong in QEMU upstream.  It is an evidence log, not a claim that every item
is an upstream bug.  Before submission, each candidate must be rebased onto
current QEMU, reduced to an independent patch, and given a focused reproducer
or regression test.

The original comparison point is QEMU 11.1.0, commit
``84f07211cc5b4fc6a371559bf8a5de4fb068e648`` (tagged
``qemu-pi4-base-v11.1.0`` in this repository).

Classification
--------------

``bug candidate``
  Existing upstream behavior appears incorrect independently of this fork.

``enhancement candidate``
  Missing behavior may be useful upstream, but its absence is not itself a
  regression or implementation error.

``fork-only``
  The change serves this project's deliberately narrow scope and should not
  be proposed to general-purpose QEMU.

Candidates
----------

QP4-UP-001: GICv5 sources in configurations without GICv5
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Fork commit: ``c80f7299116ae28b27b26483c63edd7b16363442``
:Observed issue: QEMU 11.1.0 adds ``target/arm/tcg/gicv5-cpuif.c`` to the
  common system source set unconditionally and again under
  ``CONFIG_ARM_GICV5``.  The non-GICv5 stub aborts if CPU register setup calls
  it.
:Fork change: Build the implementation only with ``CONFIG_ARM_GICV5`` and
  make the unsupported-configuration stub a no-op.
:Before sending: Reproduce with a minimal upstream ARM system configuration,
  determine whether the unconditional source entry or the aborting stub is
  the independently correct fix, and add a build/startup regression test.

QP4-UP-002: incomplete Raspberry Pi Kconfig dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Fork commit: ``3773e1a74ddc8b09392e3606f559164d918c9600``
:Observed issue: Selecting the upstream Raspberry Pi machine in a
  ``--without-default-devices`` build does not select all device models used
  by the machine: ``ARM_GIC``, ``OR_IRQ``, ``SPLIT_IRQ``, and ``UNIMP``.
:Fork change: Add the missing ``select`` clauses.
:Before sending: Reproduce each missing dependency independently on current
  upstream and split unrelated Kconfig fixes if necessary.  The focused Pi 4
  configure-and-build gate is broader evidence, not yet a minimal test.

QP4-UP-003: BCM2835 I2C interrupt remains asserted after status clear
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Fork commit: ``da9bb396578ff5fa846c20159fe9abac84b70724``
:Observed issue: Writing one to clear ``DONE``, ``ERR``, or ``CLKT`` updates
  the emulated status register but does not recompute the IRQ output.  A guest
  can therefore observe an asserted interrupt after clearing its cause.
:Fork change: Call ``bcm2835_i2c_update_interrupt()`` after the write-one-to-
  clear operation.
:Before sending: Add a focused qtest that raises ``DONE``, observes the IRQ,
  clears the status bit, and observes IRQ deassertion.  Verify the behavior
  against the BCM2711 documentation or real hardware.

QP4-UP-004: unimplemented BCM2711 DT nodes can abort recent Linux guests
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Fork commit: ``758aff349f1fce539ba5a2542bc9ce2ac11e94ff``
:Observed issue: The ``raspi4b`` machine passes through enabled nodes for the
  BCM2711 always-on L2 interrupt controller, HDMI DDC I2C controllers, and
  V3D even though QEMU does not model their MMIO blocks.  Recent upstream
  Linux probes these nodes early and can take synchronous external aborts.
  The original compatible-node loop can also miss repeated compatibles after
  nopping a node.
:Fork change: Hide every matching unimplemented node and restart each libfdt
  search after a successful ``fdt_nop_node()``.
:Before sending: Capture a short failing Linux console log on unmodified QEMU
  11.1.0, identify the smallest necessary node set, and decide with Raspberry
  Pi maintainers whether setting ``status = "disabled"`` is preferable to
  nopping nodes.  The compatible-iteration fix may deserve a separate patch.

QP4-UP-005: Pi 4 external SD card is attached to the wrong controller
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Fork commit: ``abc8df5b682c17ce51e105f2c7de004be83d4bc2``
:Observed issue: Upstream exposes the machine's external SD card through the
  legacy GPIO-multiplexed SD bus.  BCM2711-based Pi 4 boards wire the external
  slot to eMMC2 at ``fe340000``.  Consequently, an unmodified Pi 4 device tree
  cannot find a QEMU ``if=sd`` card at the hardware-correct controller.
:Fork change: Point the machine ``sd-bus`` alias at eMMC2.  A qtest proves
  that the card is present on eMMC2 and absent from the legacy SDHCI bus; the
  pinned upstream Linux lab kernel also discovers and mounts the card.
:Before sending: Rebase the card-routing change independently of the firmware
  GPIO work, preserve compatibility intentionally if upstream requires it,
  and include the focused qtest plus Linux boot evidence.

QP4-UP-006: firmware GPIO property tags needed by Pi 4 SD regulators
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: enhancement candidate
:Fork commit: ``abc8df5b682c17ce51e105f2c7de004be83d4bc2``
:Observed issue: The firmware property device does not implement get/set GPIO
  configuration and state tags for firmware-controlled GPIOs 128 through
  135.  The Pi 4 SD regulator path uses these requests during normal Linux
  initialization.
:Fork change: Implement the four mailbox tags, reset semantics, migration
  state, and qtest round trips.
:Before sending: Confirm the response/error semantics against Raspberry Pi
  firmware for valid and invalid GPIO numbers, then submit separately from
  the eMMC2 wiring correction.

QP4-UP-007: self-linked BCM2835 DMA control blocks can hang the emulator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Observed issue: A control block whose next pointer refers to itself is
  executed synchronously in the DMA write path.  A malformed guest can keep
  the QEMU vCPU thread in the device model indefinitely.
:Before sending: Confirm the intended hardware behavior with an asynchronous
  DMA test on a Pi 4, then add a bounded-loop or completion test.

QP4-UP-008: malformed property-mailbox tags can make the parser lose progress
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Observed issue: The parser advances by a guest-supplied value-buffer size;
  crafted zero or overflowing sizes may wrap or fail to advance.
:Before sending: Add a bounded-memory qtest proving termination and safe
  rejection of malformed requests.

QP4-UP-009: framebuffer palette tag has an interval bounds-check gap
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Observed issue: Palette start and end ranges are checked independently,
  rather than validating the complete interval before copying entries.
:Before sending: Confirm firmware reachability and add a malformed-tag test.

QP4-UP-010: framebuffer migration restores geometry without post-load checks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Observed issue: Migrated dimensions are restored without reallocating or
  validating the DisplaySurface, risking incompatible surface indexing.
:Before sending: Add a save/load test that changes framebuffer geometry.

QP4-UP-011: BCM2711 CPU feature and timer defaults do not match Pi 4 silicon
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Fork commit: ``2c0a3a3579e6614656c277738f999a4eea933fee``
:Observed issue: The default CPU advertises crypto extensions absent on Pi 4
  A72s, and the generic timer default is 62.5 MHz instead of 54 MHz.
:Fork change: Clear the three AArch64 crypto ID fields only for BCM2711 CPUs
  and set their architectural timer to 54 MHz.  The qtest checks ``cntfrq``;
  two Linux guests confirm the timer log and absence of crypto features.
:Before sending: Split the CPU-feature and timer corrections and address
  machine-version compatibility explicitly.

QP4-UP-012: BCM2711 clock, OTP, and eMMC2 data are stale
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Observed issue: Hardware comparison found old assumptions for the 54 MHz
  oscillator, OTP rows, and eMMC2 capabilities.
:Before sending: Split into independently tested patches and attach Pi 400
  register dumps with documented BCM2711 definitions.

QP4-UP-013: legacy Pi interrupt outputs are not routed to BCM2711 GIC SPIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Fork commit: ``48857360b4550cf101b9332a27862517a10260b1``
:Observed issue: System-timer and SPI0 outputs remain connected only to the
  legacy interrupt controller while Pi 4 Linux uses GIC SPI lines.
:Fork change: Route the four timer outputs to GIC SPIs 64 through 67 and SPI0
  to SPI 118.  Qtests raise and acknowledge every line.
:Before sending: Split timer and SPI routing if requested and add reset tests
  for already-asserted outputs.

QP4-UP-014: BCM2711 GENET v5 Ethernet model
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: enhancement candidate
:Fork commit: ``f0031db878bf2ac32f17b4d3552cf1a064b25c5f``
:Fork change: Add GENET v5 with MDIO PHY, descriptor DMA,
  checksum handling, interrupts, and user-mode networking; Linux 7.2 reaches
  DHCP with error-free RX/TX counters.
:Before sending: Split device/test patches, add migration and multi-ring
  coverage, and compare against a Pi 400.

QP4-UP-015: Pi 4 firmware property interface reports a legacy DMA mask
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Fork commit: ``79c5578a5c440d9c62caebb5deb2975e075a5ea5``
:Observed issue: ``GET_DMA_CHANNELS`` is hard-coded to legacy channels 2
  through 5, while the BCM2711 device tree and Pi 400 firmware report
  ``0x07f5``.
:Fork change: Make the mask a device property with the legacy default and set
  it to ``0x07f5`` for BCM2711.  A mailbox qtest checks the response.
:Before sending: Attach the real firmware-property response and retain the
  legacy default for older machine types.

QP4-UP-016: BCM2711 ASB bridge stubs prevent power-domain registration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Fork commit: ``0a6db1188300298e35ad3a91f254f8fa32382a15``
:Observed issue: Both ASB windows read as zero, but Linux requires the hardware
  bridge ID ``0x62726467`` at offset ``0x20`` before registering BCM2835 power
  domains.
:Fork change: Replace the stubs with minimal ASB regions returning the real
  ID.  Qtests cover both windows, and Linux 7.2 registers the power domains.
:Before sending: Include the Pi 400 register dump and keep control-register
  behavior explicitly documented as unimplemented.

QP4-UP-017: BCM2835 timer and SPI outputs remain asserted across reset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Fork commit: ``8e867d903779ab3b3a8742189beeac018a449375``
:Observed issue: Resetting the system timer clears its registers without
  deleting pending host timers or lowering its four IRQ outputs. The SPI reset
  similarly clears controller state without lowering its output IRQ. A guest
  can therefore start after reset with a stale interrupt or receive a callback
  from the previous boot.
:Fork change: Delete all pending system-timer callbacks, lower the timer and
  SPI outputs, and extend qtests to prove assertion, reset deassertion, and the
  absence of a delayed timer reassertion.
:Before sending: Split the timer and SPI fixes, retain the focused reset tests,
  and check whether each device should move to the three-phase reset API in a
  separate cleanup.

QP4-UP-018: Pi 4 upper-memory device-tree construction is incorrect
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Fork commit: ``5a8a2bf70784b5d6aaa6c98ce5804b114f2c5ed9``
:Observed issue: ``raspi4_modify_dtb()`` decides whether to add upper memory by
  testing ``info->ram_size``. That value is the boot-visible low range, capped
  below 1 GiB by construction, so a 2 GiB ``raspi4b`` guest is told it has only
  960 MiB. Simply correcting the condition would make a future 4 GiB board
  describe the live ``0xfc000000`` BCM2711 peripheral window as RAM.
:Fork change: Test the RAM size decoded from the board revision and cap the
  upper range at ``BCM2838_PERI_LOW_BASE``. The pinned Linux tests now see
  ``[0x40000000, 0x80000000)`` on ``raspi4b`` and
  ``[0x40000000, 0xfc000000)`` on ``raspi400``.
:Before sending: Split the dead-condition correction from the 4 GiB range cap
  if requested, add a small generated-DTB unit test, and include the existing
  Linux boot evidence for both memory sizes.

QP4-UP-019: distinct Raspberry Pi 400 machine
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: enhancement candidate
:Fork commit: ``5a8a2bf70784b5d6aaa6c98ce5804b114f2c5ed9``
:Fork change: Add a 64-bit-host ``raspi400`` machine with board revision
  ``0xc03130``, fixed 4 GiB RAM, the common BCM2711 SoC, and its board-specific
  device tree. Qtests cover default RAM, invalid RAM rejection, upper-memory
  access and the firmware board-revision response. A pinned Linux boot checks
  the Pi 400 machine identity and 3968 MiB physical layout.
:Before sending: Submit after the upper-memory fix, decide whether upstream
  wants the 32-bit-host omission to be user-visible in documentation, and keep
  the Pi 400 DTB boot test with the machine patch.

Hardware-derived Pi 400 memory-map correction
----------------------------------------------

A read-only capture from the project's real Pi 400 on 2026-08-22 reports one
``memory@0/reg`` property with the two ranges ``[0, 0x3b400000)`` and
``[0x40000000, 0xfc000000)``. ``/proc/iomem`` likewise ends System RAM at
``0xfbffffff`` and exposes no RAM above 4 GiB. The smaller first range reflects
that system's firmware reservation; QEMU's default 64 MiB VideoCore carveout
produces ``[0, 0x3c000000)`` while preserving the same address topology.

This evidence supersedes an earlier local proposal to alias the obscured top
64 MiB of SDRAM at ``0x100000000``. Neither the hardware DT nor the BCM2711
address-map documentation supports that relocation, so ``qemu-pi4`` does not
invent it.

Known feature gaps, not bug candidates
--------------------------------------

The absence of PCIe/VL805 USB 3, V3D 4.2, RNG200, thermal, and HDMI models is
tracked as implementation scope in :doc:`raspi`.  These are substantial
upstream enhancement opportunities, but should not be described as
correctness bugs merely because the models do not exist.  The fork's GENET v5
model belongs in the same enhancement category and is tracked as QP4-UP-014.

Fork-only changes
-----------------

Removing legacy Raspberry Pi machines, renaming the build option to
``CONFIG_RASPI4``, and making the focused ``pi4`` device configuration the
project default are intentional product-scope decisions.  They are not
upstream candidates.
