Raspberry Pi upstream candidate tracker
========================================

This page records changes discovered while developing ``qemu-pi4`` that may
belong in QEMU upstream.  It is an evidence log, not a claim that every item
is an upstream bug.  Before submission, each candidate must be rebased onto
current QEMU, reduced to an independent patch, and given a focused reproducer
or regression test.

Every classification and proposed report must first pass the local
:doc:`raspi-upstream-criteria`.  In particular, a real-hardware difference or
source FIXME is not by itself proof of an upstream bug.

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

``known limitation``
  An upstream approximation is already acknowledged; a new report needs
  materially new impact or a viable improvement.

``needs specification``
  An observation is not yet tied to a current-upstream contract violation.

``not a bug``
  The tested behavior is permitted, the test is invalid, or the difference is
  solely project policy.

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

:Classification: bug candidate; E4 host forward-progress failure and E3 Circle
  workload impact
:Fork commit: ``5ce2b80d81f3da738f1d2bf934f77011e9b3db4e``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Contract: Guest-controlled work in an MMIO handler must be bounded so the
  emulator, monitor and unrelated virtual devices retain forward progress.
  The BCM2835 manual also describes independently operating DMA channels and
  linked control blocks executing without software intervention.
:Observed issue: A normal four-byte control block whose next pointer refers to
  itself is executed synchronously in the ``CS.ACTIVE`` write path.  On the
  checked upstream master, that final qtest command never returns, the vCPU
  thread consumes a host core, the monitor stops responding and QEMU requires
  a forced kill.  The transfer is word-sized and does not depend on the older
  length-underflow issue.
:Hardware evidence: A bounded Pi 400 kernel probe reserved DMA channel 4 and
  ran the same self-linked four-byte block for 50.773 ms.  All 34 samples saw
  the channel active with current and next addresses still pointing to the
  block, while the kernel thread slept and woke and SSH remained responsive.
  Linux's normal abort/reset sequence then stopped the channel cleanly.
:Workload evidence: Circle commit ``6177984e30fa`` uses a two-control-block,
  DREQ-paced ring for I2S audio.  Its sound sample wedges the synchronous
  model before it can fill the second block.  With the fork change, the pinned
  AArch64 sample image (SHA-256
  ``772aff938605164b3211fb934bf2470ac4f1449e08ef78b550a58ffbc4aaf041``)
  reaches ``Playing modulated 440 Hz tone`` on both ``raspi4b`` and
  ``raspi400`` and QEMU exits immediately on SIGINT.  Because I2S remains a
  placeholder, this proves forward progress rather than audible output.
:Fork change: Execute at most 256 transfer operations per slice and continue
  on a migratable virtual-clock timer.  Add ACTIVE pause/resume, ABORT,
  channel-enable, DREQ/PERMAP and status behavior, byte-aligned transfers,
  functional wide-memory flags, aligned control-block pointers, bus-error
  handling, reset and migration.  Six focused DMA subtests and an active
  Pi 4 migration test cover the state transitions and deadline.
:Report status: The outer research bundle references QEMU GitLab issue 4221,
  but its current external state was not independently retrievable during this
  documentation update.  No external submission or reply was made.
:Before sending: Verify the live report and current master again, have the
  user personally rerun and inspect the minimal reproducer, and disclose the
  assisted discovery.  This fork's AI-derived code and tests are not eligible
  for upstream submission under QEMU's current provenance policy; any patch
  must be independently implemented by a human.

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

QP4-UP-020: BCM2711 PCIe host and root-port model
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: enhancement candidate
:Fork commit: ``1bf939f2b26d4d2f5300a191d5d3e27f7060a88d``
:Fork follow-up: ``4c0d24ef7728a4050357ebf980d9b3b94be1cd50``
:Fork completion: ``e5ff7235c7ea8b1cacd43337b9200cb410c02f50``
:Observed issue: Upstream QEMU has no BCM2711 PCIe host model, so the
  Raspberry Pi 4 machine must hide the PCIe device-tree node and cannot expose
  the board's VL805 USB path.
:Fork change: Add the controller aperture, a BCM2711 root port, root and
  indirect configuration access, reset/link and minimal MDIO behavior,
  programmable outbound windows, INTx routing, GIC outputs, and migration
  state.  The follow-up adds a private RAM-only DMA address space, BAR2 inbound
  mapping, and the 32-vector MSI status/mask/doorbell path.  The completed fork
  also adds the fixed VL805 endpoint, its firmware-notification behavior,
  active-mapping migration coverage, a VIA hub and the Pi 400 keyboard, then
  exposes the guest DT node.  Pinned unmodified Linux v7.2 passes the full path
  on both board models.
:Before sending: Confirm controller revision and reset semantics with raw Pi
  400 MMIO, split model, SoC wiring and qtests as appropriate, and address
  machine-version compatibility before adding a PCI root bus to an existing
  machine type.  Downstream-device, INTx, private-DMA, MSI, active-migration
  qtests and the Linux boots provide the functional controller evidence.

QP4-UP-021: virtual SGI source is missing from GICV_HPPIR
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate; E1 on the fork, current-master binary
  confirmation pending
:Fork commit: ``8460833e53a91458fd3ba63ff19fb6c4932e8bb4``
:Upstream source checked: ``ae4f3443209ab154b48b706a146e5f557ab147cb``
:Contract: For a software-originated virtual SGI, GICv2 requires the CPUID
  source field in ``GICH_LRn[12:10]`` to be returned in both GICV_HPPIR and
  GICV_IAR.
:Observed issue: With pending LR0 encoding ``0x12000405`` for SGI 5 from
  CPUID 1, the pre-fix fork returned GICV_HPPIR ``0x0005`` instead of
  ``0x0405``.  It returned the source correctly from GICV_IAR.  The frozen H4f
  lab trace records the forbidden result, and the focused fork change passed
  100 fresh processes with two distinct source tags.  See the H4f manifest in
  the separate ``gicv2-lab`` repository at
  ``results/2026-08-22-h4f-qemu-pi4/manifest.md``.
:Before sending: Reproduce with an unmodified current-master binary, search
  current issues and qemu-devel, and have the user manually validate a
  standalone reproducer.  The Codex-assisted fork change and lab must not be
  proposed as an upstream patch or regression test.

QP4-UP-022: first unimplemented GICH List Register is not RAZ/WI
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate; E1 on the fork, current-master binary
  confirmation pending
:Fork source checked: ``8460833e53a91458fd3ba63ff19fb6c4932e8bb4``
:Upstream source checked: ``ae4f3443209ab154b48b706a146e5f557ab147cb``
:Contract: GICv2 specification IHI 0048B.b section 5.3.8 says that
  ``GICH_VTR.ListRegs`` defines the implemented count and every higher-numbered
  List Register is RAZ/WI.
:Observed issue: qemu-pi4 reports ``GICH_VTR=0x90000003``, meaning LR0-LR3 are
  implemented.  A qtest-accelerated MMIO check read LR4 as zero, wrote
  ``0x1200003d`` to LR4 at GICH offset ``0x110``, and read the same nonzero
  value back before clearing it.  A disposable EL2 image independently
  produced the same result.  The current source accepts index
  ``lr_idx == s->num_lrs`` because both GICH LR bounds checks use ``>`` rather
  than ``>=``.  The virtual-delivery loops correctly stop at ``< num_lrs``,
  so the extra stored LR is not delivered, but the too-permissive register
  behavior can hide an off-by-one error in hypervisor software.
:Before sending: Build an unmodified current-master binary, rerun and manually
  validate the minimal qtest transaction, search for duplicates, and add a
  focused human-authored regression test proving that LR4 is read-as-zero and
  write-ignored when VTR advertises four LRs.  The disposable AI-assisted lab
  check is research evidence only and must not be submitted as an upstream
  test or patch.

QP4-UP-023: multi-segment xHCI event-ring support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: enhancement candidate
:Fork commit: ``e5ff7235c7ea8b1cacd43337b9200cb410c02f50``
:Upstream source checked: ``ae4f3443209ab154b48b706a146e5f557ab147cb``
:Contract: ``HCSPARAMS2.ERST Max`` advertises the number of event-ring
  segments a controller accepts.  Generic upstream QEMU advertises exponent
  zero, or one segment, and therefore does not violate its advertised
  contract by rejecting ``ERSTSZ != 1``.
:Fork requirement: The captured VL805 value advertises exponent three, or up
  to eight segments.  Pinned unmodified Linux v7.2 programs two segments and
  otherwise enters Host Controller Error when run against the old
  single-segment engine.
:Fork change: Retain the one-segment generic personality, let the VL805 use up
  to eight validated noncontiguous segments, handle DESI/dequeue and complete
  ring wrap correctly, migrate the active segment and index while accepting
  version-one streams, and add focused active-ring and migration qtests.
:Before sending: Treat this only as a generic xHCI enhancement, decouple it
  from the fork's VL805 personality, decide how a general device opts into a
  larger advertised maximum, test additional guests and migration
  compatibility, and have a human reimplement any proposal under QEMU's
  current code-provenance rules.

QP4-UP-024: repeated qtest system reset can hang on macOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: needs specification
:Fork workaround commit: ``e5ff7235c7ea8b1cacd43337b9200cb410c02f50``
:Observed issue: On macOS 14.8.7 on Apple Silicon, a full default-device
  build repeatedly hung while one ``-accel qtest`` process served several
  ``raspi4b`` test cases separated by QMP ``system_reset``.  The QMP client
  waited for the reset response, QEMU's main thread was in
  ``pause_all_vcpus()``, and one of the four dummy CPU threads was waiting in
  ``sigwait()``.  This was reproducible in the fork test layout and could leave
  orphaned qtest processes.  Giving every test case a fresh QEMU process made
  normal focused and default-device runs reliable, but a three-way parallel
  repetition of the focused suite still wedged two fresh QEMU instances on
  their sole QMP reset; the qtest harness killed them after 30 seconds.  The
  device-reset tests now use the emulated Pi watchdog's guest reset path,
  which preserves reset coverage without depending on the suspect QMP path.
:Why not a bug candidate yet: The observation has not been reduced or
  reproduced on an unmodified current ``master`` build, and no Pi-specific
  device has been excluded as the trigger.  There is therefore no E1 evidence
  or established current-upstream regression.
:Before sending: Build current unmodified upstream on the same host, minimize
  to a generic four-CPU qtest machine and repeated valid QMP resets, search
  GitLab and qemu-devel for Darwin signal/reset duplicates, retain exact
  commands and thread stacks, and have the user manually validate the result.

QP4-UP-025: BCM2711 GPIO event registers and interrupts are unimplemented
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: known limitation; E1 register-contract evidence, with no
  demonstrated E2 or E3 failure
:Fork commit: ``9185e0189123713e70007bad774679b50026ac5e``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Contract: `BCM2711 ARM Peripherals
  <https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf>`__
  section 5.1 defines three bank interrupt lines and a fourth line shared by
  all GPIO event bits.  Section 5.2 tables 76 through 89 define the two
  write-one-to-clear event-status registers and six read/write edge and level
  detector pairs.
:Observed issue: Current upstream explicitly treats all 14 registers as
  unimplemented, exposes no external pin inputs or GPIO interrupt outputs,
  and does not connect GPIO SPIs 113 through 116 to the GIC.  Against the
  unmodified current-master binary, the minimal qtest sequence
  ``writel 0xfe20004c 0x00000001`` followed by
  ``readl 0xfe20004c`` returned ``0`` instead of the written read/write
  ``GPREN0`` bit.  The fork returned ``1``.  A Linux 7.2 boot on the same
  upstream binary logged all 14 unimplemented accesses during normal pinctrl
  initialization; only the unrelated unsupported L2 interrupt-controller DT
  node was disabled so the kernel could reach this probe.
:Why known limitation: The accepted 2024 GPIO implementation deliberately
  marked these cases ``Not implemented``, so this is not a regression.
  `QEMU issue 2591
  <https://gitlab.com/qemu-project/qemu/-/work_items/2591>`__ already records
  the same Linux register-access log.  A new issue with only that evidence
  would be duplicative even though the register behavior violates the
  documented device contract.
:Fork change: Model all 58 pin inputs, output-latch behavior, synchronous and
  asynchronous edge detection, high and low level detection, status and four
  IRQ outputs, GIC routing, reset and migration.  Focused tests cover each
  detector class, interrupt bank and state transition.
:Before sending: Do not open a duplicate issue.  If upstream maintainers want
  the missing functionality, a human must independently implement it under
  QEMU's current provenance policy, reference issue 2591, include focused
  register/IRQ/reset/migration tests, and address migration compatibility for
  the existing machine type.

QP4-UP-026: DWC2 core reset and FIFO flush effects are unimplemented
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: known limitation; E1 register-contract evidence, with normal
  Linux use but no demonstrated E2 or E3 failure
:Fork commit: ``3ac780f5191ee850ab15a6bb4d38328f3cbfcc6e``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Contract: The archived `DWC2 2.94 GRSTCTL register description
  <https://nest-open-source.googlesource.com/manifest_repos/u-boot/+/5e15fb1fe70ac2857e056ad5d238ad8e3373fdb5/drivers/usb/host/dwc_otg_regs_294.h>`__
  says that core soft reset returns the internal state machines to idle,
  terminates AHB and USB transactions, clears interrupt-generating mask bits,
  preserves interrupt status and configuration, flushes the FIFOs and
  self-clears.  It also defines receive and selected/all-transmit FIFO flush
  commands as self-clearing operations.  QEMU advertises revision 2.94a in
  ``GSNPSID``.
:Observed issue: On the unmodified current-master binary, ``GRSTCTL`` initially
  read ``0x80000000``.  After writing ``GAHBCFG=0x00000021``,
  ``GINTMSK=0x04000000`` and ``GRSTCTL=0x80000001``, the reset bit self-cleared
  but ``GINTMSK`` incorrectly remained ``0x04000000``.  The model also left
  host transfer and mask state untouched and logged core reset and receive and
  transmit FIFO flushes as unimplemented.  A pinned Linux 7.2 boot exercised
  two core resets and both FIFO-flush forms during ordinary DWC2 probe, but
  completed successfully because the existing self-clear-on-read behavior
  satisfied its polling loops.
:Why known limitation: The DWC2 model has contained explicit ``TODO`` and
  ``LOG_UNIMP`` cases for these commands since its introduction, so this is
  not a regression.  Searches of QEMU GitLab and qemu-devel on 2026-08-23 did
  not find an exact existing report, but normal Linux use currently has no
  observed failure beyond incomplete state fidelity and diagnostic noise.
:Fork change: Core reset stops modeled transfers, clears global, host and
  channel interrupt masks, resets receive-status and frame state, deasserts
  the IRQ, and preserves configuration and interrupt status.  Receive and
  transmit FIFO commands complete without additional payload work because the
  DMA-only model has no separately modeled FIFO contents.  A focused qtest
  covers the observable register and interrupt contract.
:Before sending: Do not file a standalone issue without new test-oracle or
  workload impact, a regression, or a human-authored viable fix.  Any upstream
  implementation must be produced independently under QEMU's current
  provenance policy and should add focused reset and in-flight-transfer tests.

QP4-UP-027: firmware property child state survives a system reset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate; E1, with no demonstrated E2 or E3 failure
:Fork commit: ``3e05eb0ad0ac5af3abfd7ad6daa8a7e7bec62f01``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Environment: ``qemu-system-aarch64`` 11.1.50
  (``v11.1.0-453-geea8fe61b8``), native arm64 build on macOS 14.8.7,
  ``-machine raspi4b -accel qtest``.  No guest CPU instructions are involved.
:Contract: QEMU's :doc:`../../devel/reset` documentation defines cold reset as
  restoring the initial state corresponding to the start of QEMU.  The
  property child is a sysbus device and already contains a reset helper which
  clears its ``pending`` state, but current upstream calls that helper only
  from ``realize`` and does not register it as a device reset method.
:Observed issue: A direct qtest run filled the 32-entry ARM response mailbox,
  issued one more valid property request so its child response remained
  pending, requested an ordinary guest reset through the emulated Pi watchdog,
  and submitted a valid ``GET_CLOCK_STATE`` request after reset.  Unmodified
  current master returned the empty-mailbox value ``0x0000000f`` instead of
  the expected property response address ``0x00001008``.  The fork returned
  ``0x00001008``.  The stale child ``pending`` flag makes the new request wait
  in the ARM-to-VideoCore mailbox even though the parent mailbox was reset.
:History and impact: The helper and missing class registration date to the
  property's initial 2016 commit ``04f1ab15b9f``; this is not a regression.
  GitLab, qemu-devel, documentation, source-history and recent-commit searches
  on 2026-08-23 found no exact duplicate.  The reproducer deliberately
  saturates the mailbox before reset, and no real workload or external test
  suite is currently known to fail, so the evidence stops at E1.
:Reproducer: ``scripts/pi4/repro-property-reset.py`` speaks the qtest protocol
  directly and has no dependency on fork-only devices.  On 2026-08-23 it
  produced ``0x0000000f`` with current upstream and ``0x00001008`` with the
  fork.  It is an AI-derived local research artifact and is not eligible for
  upstream submission.
:Fork change: Register the existing reset helper, lower the child IRQ, and
  reset all fork-added property state.  A focused qtest proves failure against
  the preserved pre-fix binary, successful service after reset, default-state
  restoration, and migration.
:Before sending: The user must personally inspect and rerun a minimal auditable
  reproducer on current master, validate the source analysis, and disclose the
  automated assistance in any report.  Do not submit this fork's AI-derived
  code or test upstream; any patch must be independently produced by a human
  under QEMU's live provenance policy.

QP4-UP-028: firmware clock and domain state properties are incomplete
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: known limitation; E1 clock-state contract evidence and
  normal Linux use, with no demonstrated E2 or E3 failure
:Fork commit: ``3e05eb0ad0ac5af3abfd7ad6daa8a7e7bec62f01``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Contract: The `Raspberry Pi mailbox property interface
  <https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface>`__
  defines ``GET_CLOCK_STATE`` and ``SET_CLOCK_STATE`` as an ID and state pair,
  with bit 0 selecting on or off and bit 1 reporting a nonexistent clock.
  Linux's firmware clock driver uses that interface.  Its Raspberry Pi power
  driver probes ``GET_DOMAIN_STATE``, and the firmware core sends
  ``NOTIFY_REBOOT`` from its shutdown callback.
:Observed issue: Current upstream always reports every clock as enabled.
  ``SET_CLOCK_STATE`` logs an explicit not-implemented message and does not
  affect the next GET.  ``GET_DOMAIN_STATE`` and ``NOTIFY_REBOOT`` are
  unhandled.  A pinned Linux 7.2 boot takes all three paths on both Pi 4B and
  Pi 400, but still boots successfully; the visible effect is inconsistent
  control-plane state and three diagnostic messages.
:Why known limitation: The source explicitly marks the clock SET operation as
  not implemented and has no cases for the other two optional tags.  This is
  missing model scope rather than a regression, and Linux has no observed
  failure.  BCM2711 devices use the separate direct PM/ASB path for functional
  power control, so merely advertising firmware domain state does not add
  device power gating.
:Hardware evidence: A Pi 400 runtime capture found enabled clocks 2, 4, 9 and
  15 and enabled firmware domains 4, 5, 7, 20 and 23.  A same-state clock SET
  behaved consistently.  A raw V3D domain-off experiment blocked the caller
  until reboot, so the project does not automate domain writes or claim their
  deeper sequencing semantics.
:Fork change: Track valid clock and domain state, report invalid IDs, accept
  reboot notification as an explicit no-op, and reset and migrate the state.
  This is intentionally metadata only: it does not stop vCPUs or gate device
  MMIO.
:Before sending: Do not open a standalone issue without new workload or test
  impact, a regression, or independently human-authored implementation work.
  Any upstream patch must comply with QEMU's live provenance policy and should
  keep the shallow-model limitation explicit.

QP4-UP-029: BCM2835 AUX UART interrupt state survives a system reset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate; E1, with no demonstrated E2 or E3 failure
:Fork commits: ``4f78fe1e54f5ea625a1298821df563f8d2a90b57`` and
  ``5962ba96dbd4200e070427d3b8bccfb09e0cc77c``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Environment: ``qemu-system-aarch64`` 11.1.50
  (``v11.1.0-453-geea8fe61b8``), native arm64 build on macOS 14.8.7,
  ``-machine raspi4b -accel qtest``.  No guest CPU instructions are involved.
:Contract: QEMU's :doc:`../../devel/reset` documentation defines cold reset as
  restoring the initial state corresponding to the start of QEMU.  The AUX
  UART is a sysbus device, but current upstream registers no reset method for
  it.
:Observed issue: A direct qtest run observed upstream startup
  ``AUX_ENABLES=1``, ``IER=0xc0`` and ``AUX_IRQ=0``, enabled the model's
  always-ready transmit interrupt to obtain ``IER=0xc2`` and ``AUX_IRQ=1``,
  and requested an ordinary cold reset through the emulated Pi watchdog.
  Unmodified current master still returned the armed values after reset.  The
  fork resets ``AUX_ENABLES`` to zero, so the gated UART bank reads zero; after
  re-enabling it, ``IER=0``, ``AUX_IRQ=0`` and ``MCR=0`` confirm clean internal
  state and a new interrupt cycle succeeds.
:History and impact: The missing reset method dates to the device's initial
  2016 commit ``97398d900ca`` and is not a regression.  GitLab, qemu-devel,
  source-history and recent-commit searches, refreshed on 2026-08-24, found no
  exact duplicate.  The reproducer is synthetic, and no maintained guest or
  external test suite is currently known to fail, so the evidence stops at E1.
:Reproducer: ``scripts/pi4/repro-aux-reset.py`` speaks the qtest protocol
  directly and has no dependency on fork-only devices.  It explicitly enables
  the UART before comparing internal state, accommodating both upstream's
  always-enabled model and the fork's hardware gate.  On 2026-08-24 it exited
  1 with the current-upstream state above and 0 with the fork.  It is an
  AI-derived local research artifact and is not eligible for upstream
  submission.
:Fork change: Add a device reset which clears FIFO contents and positions,
  interrupt enable and derived state, the IRQ output, shared enable, scratch
  and modem control.  The qtest covers asserted GIC SPI 93 across reset,
  gated and visible register defaults and reuse after reset; migration
  post-load processing also reconstructs the derived IRQ output.
:Before sending: The user must personally inspect and rerun a minimal auditable
  reproducer on current master, validate the source analysis, and disclose the
  automated assistance in any report.  Do not submit this fork's AI-derived
  code or test upstream; any patch must be independently produced by a human
  under QEMU's live provenance policy.

QP4-UP-030: BCM2835 AUX UART modem registers are incomplete
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: known limitation; E1 register-contract evidence and normal
  Linux use, with no demonstrated E2 or E3 failure
:Fork commit: ``4f78fe1e54f5ea625a1298821df563f8d2a90b57``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Contract: `BCM2711 ARM Peripherals
  <https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf>`__
  section 2.2 says unsupported 16550 bits are ignored and read as zero.  Its
  MCR table defines bit 1 as read/write active-low RTS with reset value zero;
  its MSR table defines bit 4 as the inverse of the CTS input with reset value
  one.  The maintained Linux BCM2835 AUX driver identifies RTS as the only MCR
  flag.
:Observed issue: Current upstream explicitly logs MCR reads and writes and MSR
  reads as unimplemented.  A direct qtest write of all ones to MCR read back
  zero instead of the required supported-bit value ``0x2``.  A pinned Linux
  7.2 boot writes MCR during ordinary AUX serial initialization and produces
  one unimplemented-access message on both Pi 4B and Pi 400, but continues to
  boot successfully.
:Why known limitation: The source header has advertised line and modem control
  as unimplemented since 2016.  `QEMU issue 2591
  <https://gitlab.com/qemu-project/qemu/-/work_items/2591>`__ already includes
  the Linux MCR diagnostic.  A `2019 RFC
  <https://patchew.org/QEMU/20190820123417.27930-1-philmd%40redhat.com/>`__
  proposed replacing the device with a generic 16550; review instead favored
  improving the actual mini-UART model.  A new issue containing only the
  missing-register observation would therefore add little.
:Hardware evidence: A read-only Pi 400 runtime snapshot found
  ``AUX_ENABLES=0`` and zero in every sampled mini-UART register, including
  MCR and MSR.  A subsequent controlled probe enabled the block and observed
  the documented ``MCR=0`` and ``MSR=0x10`` defaults.  Writing all ones to MCR
  then read back only ``0x2`` while MSR remained ``0x10``.  The probe restored
  MCR and disabled the block before exiting.
:Fork change: Store only the supported RTS bit, report CTS from a capable
  character backend with the documented reset fallback, propagate RTS through
  the backend's modem-control interface, reset the output and migrate its
  control state.  Automatic flow control and GPIO pin muxing remain outside
  this change.
:Before sending: Do not open a duplicate issue without a new workload failure,
  test-oracle impact or independently human-authored implementation.  Any
  upstream code and tests must be produced independently under QEMU's live
  provenance policy.

QP4-UP-031: BCM2835 AUX enable gate and scratch register are incomplete
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: known limitation; E1 register-contract and hardware
  evidence, with no demonstrated E2 or E3 failure
:Fork commit: ``5962ba96dbd4200e070427d3b8bccfb09e0cc77c``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Environment: ``qemu-system-aarch64`` 11.1.50
  (``v11.1.0-453-geea8fe61b8``), native arm64 build on macOS 14.8.7,
  ``-machine raspi4b -accel qtest``; Raspberry Pi 400 revision 1.0 running
  Linux 6.18.39 for the hardware comparison.
:Contract: `BCM2711 ARM Peripherals
  <https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf>`__
  sections 2.1.1 and 2.2 define a reset-clear, read/write ``AUX_ENABLES``
  register, mini-UART enable bit 0 and an 8-bit read/write scratch register
  with reset value zero.  The document says disabling a module prevents its
  register access.
:Observed issue: Current upstream hardwires ``AUX_ENABLES`` to one, logs and
  ignores attempts to disable the UART or enable SPI, and explicitly logs the
  scratch register as unimplemented.  The direct qtest aid observed startup
  ``EN=1``, all-ones readback ``1`` and no effective disabled state or scratch
  retention.
:Why known limitation: The hardwired-enable comment, explicit unimplemented
  scratch paths and source header make the reduced scope deliberate and date
  to the initial 2016 model.  No maintained guest or external suite is known
  to fail because of it.  Exact GitLab searches for ``AUX_ENABLES``,
  ``AUX_MU_IER_REG`` and mini-UART scratch on 2026-08-24 found no separate
  issue; the broad Raspberry Pi OS report in QEMU issue 2591 does not show an
  enable-gate or scratch failure.
:Hardware evidence: The Pi 400 runtime state had ``AUX_ENABLES=0`` and
  zero-valued reads from the mini-UART bank, consistent with the documented
  reset-clear gate but not itself a reset capture.  All-ones enable readback
  was ``0xff`` on this silicon, though only bits 0:2 are defined.  Writes of
  ``IER=2``, ``MCR=0xffffffff`` and ``SCRATCH=0x1a5``
  while disabled read as zero but were exposed as ``2``, ``2`` and ``0xa5``
  after enabling.  The defined AUX interrupt bit and GIC interrupt ID 125
  remained live while register reads were gated.  This narrows the manual's
  broad no-access statement: implemented control writes are retained on the
  tested silicon.
:Reproducer: ``scripts/pi4/repro-aux-enable.py`` speaks qtest directly.  On
  2026-08-24 it exited 1 with current upstream and 0 with the fork.  It is an
  AI-derived local research artifact and is not eligible for upstream
  submission.
:Fork change: Store the Pi 400-observed low byte of ``AUX_ENABLES``, reset the
  gate clear, return zero for disabled mini-UART-bank reads, pause character
  input and discard output data while disabled, retain implemented control
  writes and interrupt signalling, and implement the 8-bit scratch register.
  Enable and scratch state migrate with the VM.  The two SPI blocks remain
  unimplemented.
:Before sending: Do not open a standalone issue without a concrete workload
  or test-oracle consequence, or independently human-authored implementation
  work.  A future report must distinguish defined register bits from observed
  reserved-bit readback and must follow :doc:`raspi-upstream-criteria`.

QP4-UP-032: BCM2835 AUX IER reports FIFO bits belonging to IIR
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: bug candidate; E1, with no demonstrated E2 or E3 failure
:Fork commit: ``5962ba96dbd4200e070427d3b8bccfb09e0cc77c``
:Upstream source checked: ``eea8fe61b8be8f3016e522e6af24924a0266ca95``
:Environment: ``qemu-system-aarch64`` 11.1.50
  (``v11.1.0-453-geea8fe61b8``), native arm64 build on macOS 14.8.7,
  ``-machine raspi4b -accel qtest``; the Pi 400 environment is as above.
:Contract: `BCM2711 ARM Peripherals
  <https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf>`__
  section 2.2 defines only IER bits 1:0 and reserves bits 7:2.  The FIFO-enable
  status belongs to IIR bits 7:6.  The known documentation error swapping the
  TX and RX IER descriptions does not affect this distinction.
:Observed issue: Current upstream returns ``0xc0 | s->ier`` for IER, with a
  source comment claiming its FIFO enables always read one.  A fresh qtest
  observed ``IER=0xc0`` at startup and ``0xc2`` after enabling TX interrupts.
  With the real mini UART enabled, the Pi 400 returned ``IER=0`` initially and
  ``2`` after the same write; its idle IIR value was ``0xc3``.  The fork now
  returns only the two supported IER bits while retaining the existing IIR
  FIFO status.
:History and impact: The mistaken IER return dates to initial device commit
  ``97398d900ca`` and is not a regression.  The exact GitLab and source-history
  searches above found no duplicate.  The reproducer is synthetic, and no
  maintained guest or external test suite is known to fail, so the evidence
  stops at E1.
:Reproducer: ``scripts/pi4/repro-aux-enable.py`` provides the direct comparison
  and exits 1 with current upstream and 0 with the fork.  It is AI-derived and
  local-only, not an upstream submission artifact.
:Fork change: Return only stored IER bits 1:0 and test IER independently from
  the ``0xc0`` FIFO status in IIR.
:Before sending: The user must personally inspect and rerun a minimal auditable
  reproducer on current master, validate the source and hardware analysis, and
  disclose the automated assistance in any report.  Do not submit this fork's
  AI-derived code or test upstream; any patch must be independently produced
  by a human under QEMU's live provenance policy.

Rejected and research-only findings
------------------------------------

Same-PE ordinary store between ``LDXR`` and ``STXR``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: not a bug
:Upstream issue: `QEMU #4218
  <https://gitlab.com/qemu-project/qemu/-/work_items/4218>`__
:Observed difference: A same-PE ordinary store to the exclusive target lets
  the following ``STXR`` succeed on the tested Cortex-A72, but QEMU's
  value-based implementation makes it fail indefinitely.
:Reason rejected: Arm ARM DDI 0487M.c B2.12.1 makes the ordinary store's
  effect on the local monitor IMPLEMENTATION DEFINED.  B2.12.5 does not
  guarantee forward progress with that intervening explicit memory effect.
  The result is a Cortex-A72/QEMU implementation fingerprint, not an
  architectural violation.  Do not resubmit it or infer validity from an
  adjacent QEMU FIXME.

Other-PE exclusive-monitor ABA behavior
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Classification: known limitation; research only
:Contract candidate: For Shareable Normal memory, another observer's
  completed write to the marked physical block clears the global exclusive
  mark.  A synchronized other-PE A-to-B-to-A sequence followed by an observed
  successful ``STXR`` is therefore a possible one-sided conformance test.
:Why not a new report yet: QEMU already documents that its compare-exchange
  implementation is susceptible to ABA and says typical guests have not used
  problematic patterns.  First build and manually validate a race-free
  harness on current master, then search for a concrete kernel, fuzzer,
  conformance-suite, or production-software consequence, or bring a viable
  monitor model and focused test.  Follow :doc:`raspi-upstream-criteria`.

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

The absence of V3D 4.2 and HDMI models, the unimplemented BCM2711 PCIe
controller-event path, and missing Pi 400 consumer-control event production
are tracked as implementation scope in :doc:`raspi`.  These are upstream
enhancement opportunities, but should not be described as correctness bugs
merely because the models or optional behavior do not exist.  The fork's
GENET v5, BCM2711 PCIe, VL805, RNG200, and AVS thermal models belong in the
same enhancement category and are tracked above or in :doc:`raspi-lab`.
Implementing the former RNG200 and thermal gaps in this fork did not uncover a
separate generic QEMU bug candidate.

The defects found while reviewing the unmerged July 2026 PCIe series are kept
in :doc:`raspi-pcie` and intentionally have no ``QP4-UP`` bug IDs.  They are
patch-review findings, not defects in QEMU master or a released QEMU model.

Fork-only changes
-----------------

Removing legacy Raspberry Pi machines, renaming the build option to
``CONFIG_RASPI4``, and making the focused ``pi4`` device configuration the
project default are intentional product-scope decisions.  They are not
upstream candidates.  Selecting QEMU's standard ``usb-storage`` device in the
focused configuration is likewise a product and regression-test choice, not a
machine dependency: external USB disks are optional guest-configured devices.
The storage acceptance workload found no new generic QEMU USB, SCSI, or block
bug candidate.
