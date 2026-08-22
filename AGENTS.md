# Repository instructions

Before classifying, drafting, filing, or replying to a proposed QEMU upstream
bug, read and apply
`docs/system/arm/raspi-upstream-criteria.rst`. Treat
`docs/system/arm/raspi-upstream.rst` as an evidence log, not proof that an item
is a bug.

In particular:

- Do not describe an implementation-defined choice, an invalid or
  unpredictable guest sequence, a documented missing model, microarchitectural
  timing, or fork-only scope as an architecture bug without an independently
  violated QEMU or architecture contract.
- Reproduce on unmodified current upstream master, validate every
  specification precondition, search for prior reports and documented
  limitations, and classify real-workload and test-oracle impact separately.
- Generated reproducers and findings must be manually run and validated by the
  user. Disclose LLM, fuzzer, or other automated assistance as required by the
  current QEMU bug template.
- QEMU currently declines patch contributions believed to include or derive
  from AI-generated content. Never propose or prepare Codex-authored or
  AI-derived repository content for upstream submission. Re-check the live
  QEMU code-provenance policy before any upstream work.
- Never file an issue, post a comment, send email, or submit a patch externally
  without the user's explicit approval.
