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

The absence of V3D 4.2, RNG200, thermal, and HDMI models, the unimplemented
BCM2711 PCIe controller-event path, and missing Pi 400 consumer-control event
production are tracked as implementation scope in :doc:`raspi`.  These are
upstream enhancement opportunities, but should not be described as
correctness bugs merely because the models or optional behavior do not exist.
The fork's GENET v5, BCM2711 PCIe and VL805 models belong in the same
enhancement category and are tracked above.

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
