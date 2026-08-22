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
:Observed issue: The default CPU advertises crypto extensions absent on Pi 4
  A72s, and the generic timer default is 62.5 MHz instead of 54 MHz.
:Before sending: Compare ID registers and CNTFRQ on a Pi 400, then split the
  CPU-feature and timer corrections.

QP4-UP-012: BCM2711 clock, DMA, OTP, eMMC2, and ASB data are stale
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Observed issue: Hardware comparison found old assumptions for the 54 MHz
  oscillator, DMA mask, OTP rows, eMMC2 capabilities, and ASB identification.
:Before sending: Split into independently tested patches and attach Pi 400
  register dumps with documented BCM2711 definitions.

QP4-UP-013: legacy Pi interrupt outputs are not routed to BCM2711 GIC SPIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: enhancement candidate
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Observed issue: System-timer and SPI0 outputs remain connected only to the
  legacy interrupt controller while Pi 4 Linux uses GIC SPI lines.
:Before sending: Prove each route with a real Pi 400 trace and minimal test.

QP4-UP-014: BCM2711 GENET v5 Ethernet model
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: enhancement candidate
:Fork change: The working tree adds GENET v5 with MDIO PHY, descriptor DMA,
  checksum handling, interrupts, and user-mode networking; Linux 7.2 reaches
  DHCP with error-free RX/TX counters.
:Before sending: Commit and split device/test patches, add migration and
  multi-ring coverage, and compare against a Pi 400.

Known feature gaps, not bug candidates
--------------------------------------

The absence of PCIe/VL805 USB 3, V3D 4.2, RNG200, thermal, and HDMI models is
tracked as implementation scope in :doc:`raspi`.  These are substantial
upstream enhancement opportunities, but should not be described as
correctness bugs merely because the models do not exist.  The fork's GENET v5
model belongs in the same enhancement category; it will receive a candidate
entry here once its implementing commit is recorded.

Fork-only changes
-----------------

Removing legacy Raspberry Pi machines, renaming the build option to
``CONFIG_RASPI4``, and making the focused ``pi4`` device configuration the
project default are intentional product-scope decisions.  They are not
upstream candidates.
