GICv2 virtualization lab plan
=============================

This is a fork-development plan, not documentation proposed for QEMU
upstream.  Hypervisor and test-harness source belongs in a separate repository
named ``gicv2-lab``.  Keeping it separate lets the same laboratory test
unmodified upstream QEMU, ``qemu-pi4``, Linux KVM, and physical hardware
without making the test oracle part of the emulator under test.

The project is a deterministic AArch64 EL2 laboratory for differential GICv2
virtualization testing.  It is not intended to become a production or
general-purpose hypervisor.

Current status
--------------

The separate `gicv2-lab repository
<https://github.com/kmehltretter82/gicv2-lab>`__ has completed its current
QEMU-only H1-H4i sequence.  The final paused virtual-interface save/restore
scenario passed 100 consecutive fresh qemu-pi4 processes at lab evidence
commit ``72865aa15a061469b7728af69d2089202d0eae67`` on 2026-08-22.  No physical
Pi 400 or Linux KVM run has been performed.

The milestone list below is the original, intentionally broad research plan.
The separate repository's ``ROADMAP.md`` and result manifests are
authoritative for implemented scenarios and their exact test revisions.

Why ``gicv2-lab``
------------------

The Pi 4 family contains an Arm GIC-400, which is one implementation of the
GICv2 architecture.  QEMU provides a generic GICv2 model, and Linux KVM
provides a VGICv2 userspace interface.  Naming the project after GICv2
therefore describes all three backends; ``gic400-lab`` would incorrectly
suggest that every backend models that particular Arm product.

Suggested repository description::

  Deterministic AArch64 EL2 lab for differential GICv2 virtualization
  testing on Raspberry Pi 4/400, QEMU, and Linux KVM.

Do not add the laboratory to ``qemu-pi4`` as a Git submodule initially.
Record the tested QEMU and laboratory commit IDs in result manifests and CI
jobs instead.  Choose and record the laboratory's license and contribution
provenance policy before adding source code.  Content created for this lab
must not be repackaged as a QEMU upstream patch contrary to QEMU's current
code-provenance policy.

Success criterion
-----------------

The useful product is not merely a guest that boots.  A successful laboratory
can run the same small, deterministic interrupt scenario on three backends,
capture comparable architectural state transitions, identify a one-sided
forbidden result, and reduce that result to a small reproducer:

* ``qemu-pi4`` or unmodified QEMU under TCG;
* the Pi 400's physical GIC-400 virtualization interface at EL2; and
* Linux KVM with ``KVM_DEV_TYPE_ARM_VGIC_V2``.

The first Linux guest is deliberately late in the plan.  Small EL1 payloads
make incorrect interrupt state much easier to observe and minimize.

Repository layout
-----------------

The initial repository should remain freestanding and allocation-free::

  arch/arm64/       entry, exception vectors, sysregs, stage-2 translation
  hyp/              vCPU state, trap handling, run loop, virtual interrupts
  platform/pi400/   boot protocol, UART, memory map, GIC-400, SMP bring-up
  guest/tests/      tiny EL1 payloads and deterministic scenarios
  include/          shared register definitions and trace format
  tools/            trace comparison, replay, and reduction
  docs/             design, hardware-run procedure, and result manifests

Start with a small amount of AArch64 assembly and freestanding C.  Avoid a
runtime library, heap, device passthrough, filesystem, network stack, and
general VM configuration until a test demonstrably needs them.  Every build
should retain its ELF image, raw image, linker map, disassembly, configuration,
source commit, toolchain version, and image hash.

Milestones
----------

H0: contract and safety boundary
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* State explicitly that this is a single-guest GICv2 test instrument.
* Select the license, toolchain, coding rules, and contribution-provenance
  policy before source is accepted.
* Define a versioned, append-only trace format before implementing complex
  virtual interrupt behavior.
* Reserve separate physical ranges for the monitor, guest, shared trace
  buffer, stacks, and page tables.
* Keep the real Pi 400 run disabled in normal development and CI.

Exit condition: the repository contains the scope, memory map, build contract,
trace schema, and physical-hardware safety checklist.

H1: EL2 monitor under ``qemu-pi4``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Add reset entry, per-CPU stacks, BSS initialization, linker script, PL011
  output, panic handling, and EL2 exception vectors.
* Print ``CurrentEL``, ``MIDR_EL1``, ``MPIDR_EL1``, ``CNTFRQ_EL0``, selected
  virtualization ID registers, and ``GICH_VTR``.
* On every exception, print at least ``ESR_EL2``, ``FAR_EL2``, ``HPFAR_EL2``,
  ``ELR_EL2``, and ``SPSR_EL2`` before halting.
* Handle one deliberately triggered synchronous exception and one EL2 timer
  interrupt without losing diagnostic state.

Exit condition: 100 consecutive cold QEMU boots produce identical normalized
capability and exception traces and no unexplained reset or hang.

H2: one tiny EL1 guest
~~~~~~~~~~~~~~~~~~~~~~

* Configure ``HCR_EL2`` for AArch64 EL1 and enter a one-vCPU payload with
  ``ERET``.
* Build minimal stage-2 tables that map only the guest RAM and explicitly
  protect monitor memory, stacks, page tables, and trace buffers.
* Implement ``HVC`` report, success, failure, and shutdown calls.
* Prove both a valid guest memory access and a deliberate stage-2 fault.
* Keep the first guest single-core, MMU-off, and independent of a device tree.

Exit condition: the payload enters EL1, reports through ``HVC``, exercises a
stage-2 fault with the expected syndrome, and exits deterministically.

H3: first GICv2 virtual interrupt
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Read the implemented List Register count from ``GICH_VTR``; never assume
  that every backend has four LRs.
* Initialize the physical GIC and the GIC virtualization control interface,
  including ``GICH_HCR`` and ``GICH_VMCR``.
* Inject one virtual interrupt through a List Register.
* Let the EL1 guest acknowledge it through ``GICV_IAR``, record the virtual
  INTID, write ``GICV_EOIR``, and return through ``HVC``.
* Capture and acknowledge the GIC maintenance interrupt, PPI 25 on the
  Pi 400, and record the relevant MISR/EISR/ELRSR/LR state.

Exit condition: a single virtual interrupt completes exactly once with a
fully explained trace on ``qemu-pi4``.

H4: deterministic GICv2 state-machine suite
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add small scenarios one at a time, each with an explicit initial state,
stimulus, allowed outcomes, timeout, and final-state assertions:

* invalid, pending, active, and pending-plus-active LR states;
* underflow, no-pending, EOI, and LR-entry-not-present maintenance events;
* priority masking, preemption, binary point, APR, and VMCR behavior;
* EOImode 0 and split priority-drop/deactivation behavior;
* edge delivery and level reassertion/resampling;
* SGIs, including multiple source PEs;
* more runnable interrupts than available LRs;
* WFI wakeup and interrupt-arrival boundary races; and
* save/restore and migration of a paused virtual CPU's complete GIC state.

Trace records need a monotonically increasing sequence number, physical and
virtual CPU IDs, scenario step, operation, register or INTID, value, and
observed result.  Timing may help diagnose a failure, but it must not be the
correctness oracle.

H5: differential runner and reducer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Run identical payload bytes and scenario inputs against QEMU and physical
  EL2 backends.
* Normalize implementation-specific capability fields before comparing
  traces.
* Encode architectural sets of allowed results rather than requiring QEMU to
  copy every GIC-400 implementation choice.
* Treat a forbidden one-sided outcome as the primary signal; do not infer a
  bug merely from different timing or interrupt latency.
* Add deterministic replay and prefix/delta reduction for a failing sequence.
* Preserve raw traces and complete build manifests for every claimed
  difference.

Only after the bare-metal comparison is useful, add a small Linux userspace
runner using ``/dev/kvm`` and ``KVM_DEV_TYPE_ARM_VGIC_V2``.  Run existing
KVM GICv2 unit tests as a baseline before adding lab-specific API sequences,
irqfd/resample tests, or vCPU save/restore tests.

H6: first real Pi 400 boot -- deliberately scheduled later
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The physical boot is gated by H1 through H3 passing under ``qemu-pi4``.  It is
not part of initial repository bring-up.  Before the first attempt:

* create a clean signed or tagged source checkpoint and retain reproducible
  image hashes, linker map, and disassembly;
* use a spare or otherwise recoverable boot medium with a known-good fallback;
* have UART capture and a reliable power-cycle/recovery path available;
* make the loaded monitor perform no runtime block, filesystem, OTP, firmware,
  PCIe, USB, or network writes;
* restrict MMIO to PL011, the architectural timer, and documented GIC
  regions; and
* boot one physical core and halt on any unexpected exception or timeout.

The first hardware image does only four things:

#. print the EL2 and GIC capability registers;
#. enter the tiny EL1 guest;
#. handle one ``HVC`` round trip and one virtual interrupt; and
#. print the final trace and halt.

The boot partition may have to be prepared to load the image; the safety
promise is that the monitor makes no storage writes after firmware loads it.
Do not enable SMP, random sequences, repeated resets, guest-controlled MMIO,
or stage-2 fuzzing during this first hardware run.

Exit condition: the captured serial trace is complete, the board halts as
designed, and the boot medium remains recoverable.  Only then enable individual
H4 scenarios on hardware, initially one per boot with strict timeouts.

H7: Linux guest, if it adds evidence
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Linux is a workload, not the first oracle.  Add only the minimum support it
needs: PSCI CPU control, virtual timer, GICv2 device tree, console, and an
initramfs.  Keep emulated devices out of scope unless a particular KVM/QEMU
bug hypothesis requires one.

Scope limits of the Pi 400
--------------------------

The Pi 400 is valuable precisely because it provides real GICv2 virtualization
hardware that is now less common in current systems.  It cannot validate
GICv3 or GICv4, ITS/MSI translation, VHE, Arm nested-virtualization extensions,
or protected KVM support.  Findings must be described as GICv2 results, not as
proof about every Arm interrupt controller.

First implementation slice
--------------------------

After the new repository exists, implement only H1 first: a linked image that
prints an EL2 banner and capability dump under ``qemu-pi4``, deliberately
takes one exception, prints its syndrome, and halts.  Do not begin the real
Pi 400 boot or Linux/KVM runner in that slice.
