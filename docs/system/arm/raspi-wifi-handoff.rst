Pi 400 and Pi 500+ Wi-Fi development handoff
============================================

Checkpoint: 2026-10-16. This is AI-assisted work for Karl's downstream
forks, not a QEMU upstream submission.

The later ``wifi-next-20261017`` experimental branch adds shared radio
regressions and records the Pi 500 source integration. Its pending-scan
migration test currently fails, and development is paused. See
:doc:`raspi-wifi-next-checkpoint`; the historical results below must not be
treated as qualification of that branch.

Repositories and tested state
-----------------------------

Pi 400 development now belongs in
https://github.com/kmehltretter82/qemu-pi4 on branch
``pi400-wifi-integration-20261012``. The integration imports the earlier
Pi 400 SDIO model onto main ``c1325f438746428317bbd6b2e9c2aee702147d78``;
it retains that main's display, board and storage changes. Unrelated
ExactCPU experiments were not imported. The endpoint stays disabled by
default. See :doc:`raspi` for its options and supported control profile.

Tested code commit: ``769485bc045e5270a0930a34dc6c2b9e2008339c``.
The documentation commit following it does not change the tested code.
Fresh Linux-host results:

* Pi 4 qtests: 73/73; Pi 400 board: 5/5; display: 27/27. No skips.
* One Linux PREEMPT_RT report-mode boot: 20/20 markers and 200/200 stream
  records, ordered without duplicates. All 226 netconsole UDP datagrams
  match the recorded Ethernet pcap byte-for-byte and in order.
* Linux attached brcmfmac through ``fe300000.mmc`` using PIO. Lockdep
  remained active; no measurement diagnostics or TX errors/drops occurred.

Local main on 2026-10-16 additionally completed firmware upload/readback,
country state and eMMC1 DMA request wiring, then added the bounded 5 GHz
channel-36/20-MHz profile described below.  A fresh macOS build passes the
79-test Pi 4 suite (including focused 5 GHz and scheduled-scan migration
coverage), all 5 Pi 400 board tests and all 27 display tests.  The stock
6.18.39 Raspberry Pi kernel and unmodified Pi 4 DT also completed five
consecutive Pi-on-Pi station boots with the channel-36 profile.  The real
``brcmfmac`` driver advertised 5180 MHz through nl80211, ``iw`` received the
synthetic BSS, and carrier reached one before each clean power-off.  This is
end-to-end acceptance of the bounded synthetic station path, not RF or AP-mode
validation.

The earlier 2.4 GHz boot used the compatibility association profile, a
41-position SDPCM credit window and 1 ms credit delay.  The 5 GHz boots use a
10 ms scan-event delay which starts only after the preceding SDIO frame is
consumed.  This ordering and its pending timer migrate with the endpoint.  The
boots skipped the old stress fixtures; they are integration coverage, not a
new BASE/prototype or retry-policy comparison.

Pi 500+ development belongs in
https://github.com/kmehltretter82/qemu-pi5, branch
``pi5-dma-tests-20261010``, pinned at
``68c5213ce84dc1f224c191a30ae088976dba421e``. That fork has storage/DMA
tests but does not yet implement the Wi-Fi endpoint. Do not copy the
Pi 400 board wiring into it unchanged.

Reproduce before extending the model
------------------------------------

Use a fresh clone rather than copying the Hetzner integration repository:
the local repository borrows objects from an older checkout. With the
normal QEMU build dependencies installed, build from the clone's root::

  mkdir build-wifi
  cd build-wifi
  ../configure --target-list=aarch64-softmmu --enable-slirp --disable-docs
  ninja qemu-system-aarch64 tests/qtest/raspi4b-test \
      tests/qtest/raspi400-test tests/qtest/bcm2711-display-test \
      tests/qtest/cyw43455-ap-test tests/qtest/cyw43455-fws-test
  export QTEST_QEMU_BINARY="$PWD/qemu-system-aarch64"
  tests/qtest/raspi4b-test
  tests/qtest/raspi400-test
  tests/qtest/bcm2711-display-test
  tests/qtest/cyw43455-ap-test
  tests/qtest/cyw43455-fws-test

These commands were exercised on Linux with an offline dependency setup and
again on macOS for the current local main. Preserve the compiler,
configuration, source identity and raw test output. The archived Linux smoke
runner has fixed Hetzner paths and a binary hash. Adapt a copy for a new host,
recording its paths and hash; do not edit the archived runner or assume its
Linux binary is portable.

Next Pi 400 milestone
---------------------

The small driver-visible 5 GHz profile based on the successful hardware
channel-36/20-MHz capture is now implemented on local main. Band lists,
bandwidth capabilities, chanspecs, scheduled-scan channel validation,
scan/BSS replies, legacy rates, information elements and per-band roaming
thresholds are covered together. Other 5 GHz channels and widths fail
explicitly. A property naming a synthetic peer's channel does not establish
that Linux can operate this endpoint as an AP. The bounded channel-36 Linux
station boot is now retained with the native Pi 400 artifacts. Expand the RF
profile only from additional captured channels and widths.

The captured firmware flow-control slice is now integrated as an opt-in
private model. It covers negotiation, event-mask readiness, credit-map and
BCMC-support events, packet tags, pending-traffic TLVs, bounded terminal
status batches, explicit FIFO credit returns, shared AC borrowing, a separate
BCMC pool, primary-interface close/open ordering, malformed input, reset and
active-state migration refusal. Native wire tests exercise both host modes:
mode 1 returns ordinary credits through terminal status, while mode 2 waits
for explicit ``FIFO_CREDITBACK``. SDPCM transport sequence credits remain a
different protocol.

The retained Pi 400 captures and Linux runs validate completion/credit framing
and active traffic, while synthetic finite budgets provide the exhaustion and
resume oracle. They do not prove physical firmware starvation timing. The next
FWS milestones are suppression/sequence reuse, compressed completion records,
peer power-save and RX reordering, each requiring separate evidence. Do not
invent XOFF/XON transitions: none occurred in the retained Pi 400 sample.

The physical Pi 400 PMF-disabled AP workaround is useful evidence for an
investigation, not a firmware fix. The private open-AP HLE now covers the
bounded driver-visible primary-BSS control contract and one synthetic peer,
but it does not execute firmware, WPA handshakes or PMF. Do not claim that
synthetic association reproduces the hardware failure.

Next Pi 500+ milestone
----------------------

Add a second BCM2712 SDHCI host and reuse the common endpoint/SD-bus card
interrupt plumbing. Preserve the existing storage host:

* Storage SDIO1: registers ``0x1000fff000``, configuration
  ``0x1000fff400``, SPI 273 (GIC INTID 305).
* Wi-Fi SDIO2: registers ``0x1001100000``, configuration
  ``0x1001100400``, SPI 274 (GIC INTID 306).

The SDIO2 values come from the preserved BCM2712 Linux device tree. The
Pi 5 fork's SDHCI bus currently lacks the card-interrupt callback used by
this endpoint. Import the common implementation and its dependencies as
one reviewed change, then add board state, realization, DT wiring and an
opt-in property. Keep the SD card on SDIO1. Capture the Pi 500+ chip,
revision, SDIO IDs and selected firmware; its ADMA64 host alone does not
establish that the Pi 400 endpoint identity is accurate for that board.

First tests should cover separate host registers/IRQs, CMD5/3/7 discovery,
CMD52 function setup and CMD53 byte/block transfers. Then exercise SDIO
ADMA64 in both directions with descriptor tables and buffers above 4 GiB,
guard bytes and poisoned low-address aliases. Include fragmented F2
control/data traffic, IRQ masking/acknowledgement and reset with pending
traffic. Retain the storage ADMA and NVMe regression tests. Only then
attempt a bounded Linux boot through ``sdhci-brcmstb`` on SDIO2, requiring
observed ADMA64 transfers rather than accepting a PIO fallback as DMA coverage.

Hardware coordination and acceptance
------------------------------------

The Mac agent owns both boards. Finish the current Wi-Fi mechanism pair,
restoration/archive and board release, then the queued DMA Stage 1 work,
before scheduling new model-calibration boots. Offline QEMU work can
proceed while those campaigns own the boards. Keep wired management and
record the actual Wi-Fi topology, band, PMF and firmware configuration.

Capture the driver/SDIO boundary as well as numbered receiver traffic.
An empty ordinary DUT pcap does not prove netpoll sent no packets. Match
hardware and guest kernel/configuration, host mode, credit protocol and
workload before comparing behavior. Every new model feature needs a
focused protocol regression and a bounded real-driver test.

TCG validates modeled transactions and software paths; it does not prove
physical cache coherence, RF loss, firmware scheduling or RT latency.
Keep the physical packet-ordering/AP-loss findings and the netpoll retry
design question open independently of model delivery passes.

Full paths to the sealed inputs, raw results, hardware evidence and next
tasks are in the Hetzner handoff:
``/home/karl/linux-work/dmadebug/hwtest/qemu-pi4-wifi-integration-20261012/HANDOFF.txt``.
