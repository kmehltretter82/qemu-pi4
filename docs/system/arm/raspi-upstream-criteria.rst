Raspberry Pi upstream report criteria
=====================================

This is the ``qemu-pi4`` project's local submission gate.  It is deliberately
stricter than QEMU's issue template so that experimental hardware differences
do not become low-quality upstream reports.  It is not an official or
exhaustive statement of what QEMU maintainers must accept.

Apply this gate before classifying a finding as an upstream bug, drafting an
issue, replying to a maintainer, or preparing an upstream patch.  The
:doc:`raspi-upstream` page is an evidence log; an entry there is not proof of
a bug.

Current QEMU rules that are hard gates
--------------------------------------

Check the live upstream rules every time because they can change.

* Reproduce against current QEMU ``master`` where practical.  A problem seen
  only in an old distribution package belongs with the distributor until it
  is reproduced upstream.
* Provide the host and guest environments, exact QEMU version and flavor, the
  smallest complete command line, deterministic reproduction steps, and the
  relevant output.  Auditable source is preferred to an opaque binary.
* Disclose assistance from LLMs, fuzzers, static analyzers, or other automated
  tools in a bug report.  The human reporter must personally run and validate
  the reproducer and every claimed finding.
* Suspected security issues must be made confidential before submission.
* Do not send an external issue, comment, email, or patch without the user's
  explicit approval.

QEMU's current code-provenance policy is especially important for this fork:

* QEMU says it will decline patch contributions believed to include or derive
  from AI-generated content.
* AI may be used for research, static analysis, and debugging only when its
  output is not included in the contribution.
* Consequently, code, tests, documentation, commit text, or patch text
  generated or edited by an AI agent in this fork must not be submitted
  upstream.  Do not disguise or mechanically rewrite that provenance.  A
  person intending to contribute must comply independently with the current
  QEMU policy and the Developer's Certificate of Origin, or first obtain an
  expressly documented exception from the QEMU community.

The bug-report rule and patch rule are different.  QEMU's issue template
expressly permits assisted discovery when it is disclosed and fully validated;
that permission does not make AI-derived patch content acceptable.

What counts as a bug candidate
------------------------------

A difference becomes a bug candidate only when it violates an identifiable
contract.  Record the exact contract before writing the conclusion.  Useful
contracts include:

* a mandatory or guaranteed behavior in the applicable architecture manual,
  including all stated preconditions and usage restrictions;
* a documented register or device behavior for hardware QEMU claims to model;
* QEMU's documented command-line, migration, QMP, image-format, or machine
  interface;
* a regression from a previously supported upstream behavior; or
* host safety and forward progress: guest-controlled input must not crash,
  corrupt, or indefinitely wedge the emulator merely because the guest is
  malformed.

A valid current-master reproducer of a forbidden outcome is enough to remain a
bug candidate even if no production workload is known.  QEMU is also used as a
test oracle by kernel CI, fuzzers, architecture tests, and software bring-up.
An emulator that is too permissive can hide guest bugs; one that is too strict
can create false failures.  Concrete test-oracle impact and production-software
impact raise priority, but they do not define architectural correctness.

What is normally not a bug
--------------------------

Classify the following as an implementation difference, enhancement, known
limitation, or fork-only work unless a separate QEMU contract is violated:

* **IMPLEMENTATION DEFINED behavior.**  This does not mean arbitrary behavior:
  the architecture permits a bounded choice and expects an implementation to
  define and document its choice.  A difference between QEMU and one Cortex
  implementation is not by itself an architecture violation when both choices
  are permitted.  It can still motivate documentation or an explicitly scoped
  named-CPU fidelity enhancement.
* **UNDEFINED, UNPREDICTABLE, or CONSTRAINED UNPREDICTABLE guest sequences.**
  Such a test cannot demand the result selected by one board.  This never gives
  QEMU permission to crash or corrupt the host.
* **Optional or unimplemented devices and features.**  Missing V3D, HDMI, or
  VL805 models are enhancements, not bugs in nonexistent models.
* **Microarchitectural detail outside TCG's contract.**  Pipeline timing,
  speculation, cache latency, and exact PMU event counts are normally fidelity
  projects rather than architectural bugs.
* **A hardware observation without a violated specification.**  Real Pi 400
  results are valuable evidence, but one implementation is not the Arm
  architecture.
* **Fork policy.**  Removing other boards, changing defaults, and optimizing
  for the Pi 4 family are deliberate ``qemu-pi4`` product choices.
* **A stale downstream build, frontend, or vendor integration problem** that
  does not reproduce when QEMU is run directly from supported upstream code.

Known limitations and FIXME comments
------------------------------------

A FIXME is neither proof that a proposed test is valid nor immunity from bug
reports.

Before filing around a known limitation, search QEMU documentation, source,
GitLab, and qemu-devel.  Prefer adding materially new information to the
existing discussion.  A new report should normally contribute at least one of:

* a standards-valid minimal reproducer demonstrating a previously unrecorded
  forbidden outcome;
* a supported real-world workload that fails;
* a kernel test, fuzzer, or conformance suite whose result is masked or made
  false by the limitation;
* a regression or newly severe consequence; or
* a credible fix and focused regression test with acceptable performance.

A synthetic conformance failure can still be real.  For a limitation that is
already explicitly documented and deliberately traded for performance, lack of
new impact usually makes a duplicate issue unhelpful even when the behavior is
not architecturally exact.

Evidence ladder
---------------

Use these levels to describe strength without inflating the claim:

``E0: observation``
  QEMU and hardware differ.  No contract has yet been established.

``E1: constrained proof``
  A one-sided test observes an outcome the cited contract forbids.  All
  preconditions and compiler/language concerns have been audited.

``E2: test-oracle impact``
  The defect masks or invents a failure in an architecture test, kselftest,
  LTP, syzkaller/syzbot workflow, CI system, or realistic kernel-bug test.

``E3: real workload``
  A maintained kernel, firmware, library, runtime, driver, or application
  takes the affected path and fails.

``E4: severe/regression``
  There is data loss, a security consequence, a host crash/hang, or a clear
  regression with a known first-bad commit.

E1 establishes a correctness candidate.  E2--E4 make it more actionable.
E0 alone is not ready to report.

Reproducer quality gate
-----------------------

Before calling a result E1 or higher, answer all of these:

#. Which exact QEMU executable, commit, accelerator, CPU, and machine were
   tested?
#. Does it reproduce on unmodified current upstream ``master``?
#. What exact specification revision, section, and sentence is violated?
#. Are every one of that sentence's memory type, shareability, privilege,
   alignment, instruction-pairing, and ordering preconditions satisfied?
#. Is the tested sequence itself defined?  Check adjacent usage restrictions,
   exceptions, footnotes, and pseudocode rather than quoting one sentence.
#. Is the forbidden outcome deterministic, or is the test one-sided so that
   scheduling and spurious failure cannot produce a false positive?
#. Is concurrency synchronized?  Avoid C data races, ``volatile`` as a
   synchronization substitute, unproven overlap windows, and rate-only claims.
#. Has generated assembly been inspected when inline assembly, compiler
   barriers, access size, or optimization matters?
#. Are controls included and is the smallest source reproducer attached?
#. Has the result been checked against real Pi 400 hardware when that hardware
   is relevant, without treating the board as the architecture specification?
#. Has the source, documentation, issue tracker, mailing list, and recent git
   history been searched for a known limitation, duplicate, or concurrent fix?
#. Can the report state practical impact honestly: none known, test-oracle
   impact, or a named real workload?

For probabilistic tests, retain raw counts and confidence information, but do
not infer a mandatory semantic rule merely from different rates.

Exclusive-monitor case study
----------------------------

QEMU issue #4218 is the model for why this gate exists.  Its test performed,
on the same PE::

  LDXR target
  ordinary store target
  STXR target

Arm ARM DDI 0487M.c section B2.12.1 makes it IMPLEMENTATION DEFINED whether
that ordinary store affects the local monitor.  The forward-progress rules in
B2.12.5 also exclude an explicit intervening memory effect.  The Pi 400's
Cortex-A72 lets the ``STXR`` succeed while QEMU's value-based implementation
fails it forever; both are permitted.  The result is E0 and a useful
implementation fingerprint, not an architectural bug.

The source FIXME about exclusive access ranges and QEMU's documented ABA
weakness are separate.  They do not make the #4218 sequence valid.

A stronger research candidate uses two PEs and Shareable Normal memory:

#. PE0 executes ``LDXR`` on the target.
#. PE1 performs a proven A-to-B-to-A write sequence to the same physical
   marked block and completes the required barriers.
#. PE0 observes completion and then executes ``STXR`` using the same address,
   size, and valid instruction pairing as its ``LDXR``.
#. Treat only ``STXR`` success as evidence: scheduling, interrupts, and the
   handshake can create extra failures, but cannot legalize success after the
   other observer has cleared the global mark.

This avoids the same-PE IMPLEMENTATION DEFINED transition and targets QEMU's
documented compare-exchange/ABA shortcut.  The harness must use real
synchronization with flags outside the reservation granule, avoid C undefined
behavior, prove the interfering writes completed, and be validated on current
master.  It is an E1 candidate, not yet a new issue: QEMU already documents the
ABA limitation, so first look for a concrete real workload, architecture test,
or fuzzer consequence, or bring a viable model and regression test.

Do not substitute either of these ambiguous tests:

* unsynchronized stress rates do not prove that an interfering store occurred
  between a particular load-exclusive and store-exclusive; and
* loading exclusively through one virtual address and storing exclusively
  through another alias can be rejected by the architecture's virtual-address
  monitor check, even when both aliases reach one physical address.

Classification and disposition
------------------------------

Use one of these labels in :doc:`raspi-upstream`:

``bug candidate``
  The evidence is at least E1 and shows a current upstream contract violation.
  State what remains before reporting.

``enhancement candidate``
  New functionality, optional fidelity, a permitted implementation choice, or
  a missing model that could benefit general QEMU.

``known limitation``
  Real nonconformance or approximation already acknowledged upstream.  Record
  new impact and the threshold for reopening discussion.

``needs specification``
  The behavior differs, but the controlling contract or preconditions have not
  been established.  Do not draft an issue yet.

``not a bug``
  The test is invalid, the result is expressly permitted, or the behavior is
  solely fork policy.  Preserve the reason so the claim is not repeated.

Replying after maintainer review
--------------------------------

Treat review as a new verification step.  Re-read the cited source, inspect
the exact submitted test, and independently check the objection.  If the
maintainer is right, acknowledge the precise error and update every local
tracker.  If the maintainer is wrong, reply with the exact contract,
preconditions, and forbidden transition; do not argue from authority, a
hardware percentage, or the mere presence of a FIXME.

Official references
-------------------

* `Reporting a bug <https://www.qemu.org/contribute/report-a-bug/>`__
* `QEMU bug template
  <https://gitlab.com/qemu-project/qemu/-/raw/master/.gitlab/issue_templates/bug.md>`__
* `Code provenance and AI-generated content
  <https://www.qemu.org/docs/master/devel/code-provenance.html#use-of-ai-generated-content>`__
* `Submitting a patch
  <https://www.qemu.org/docs/master/devel/submitting-a-patch.html>`__
* `TCG synchronization primitives and the ABA limitation
  <https://www.qemu.org/docs/master/devel/multi-thread-tcg.html#synchronisation-primitives>`__
* `Arm Architecture Reference Manual DDI 0487M.c
  <https://developer.arm.com/documentation/ddi0487/mc/>`__
