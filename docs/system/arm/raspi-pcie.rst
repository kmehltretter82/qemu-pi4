Raspberry Pi 4 PCIe, VL805 and external USB status
==================================================

The Pi 4 family's external USB ports are not attached to BCM2711's internal
xHCI block. They are reached through the BCM2711 PCIe root complex and a VIA
Labs VL805 PCIe xHCI controller. The Pi 400's integrated keyboard is behind
that VL805 path as well. Enabling the internal xHCI or substituting a generic
ECAM host would therefore produce a useful virtual computer but not a faithful
Pi 400.

Implemented topology
--------------------

The board models now expose::

  BCM2711 PCIe host at 0xfd500000
    00:00.0 Broadcom root port [14e4:2711]
      01:00.0 VIA Labs VL805 xHCI [1106:3483]
        USB 2 hub [2109:3431], four high-speed downstream ports
          Pi 400 keyboard [04d9:0007], on hub port 4 (Pi 400 only)
        USB 3 ports

The host controller belongs to the BCM2711 SoC. The fixed VL805 population is
a property of Pi 4 Model B and Pi 400 boards; it must not be built into the SoC
because Compute Module 4 exposes PCIe for external devices instead.

Current fork status
-------------------

All five implementation slices are complete, and the PCIe device-tree node is
guest-visible.  The host-controller work provides the BCM2711 controller
aperture, a Broadcom ``14e4:2711`` root port with PCI revision ``0x20`` and a
Gen2 x1 capability, root and indirect configuration access, reset/link
scaffolding, minimal synchronous MDIO/SSC behavior, four guest-programmed
outbound windows, INTx routing, all six GIC outputs, and migration state.

Downstream bus masters now use a private PCI DMA address space.  BCM2711 BAR2
maps only the guest-programmed inbound portion of SDRAM; CPU peripheral MMIO
is never exposed as an accidental DMA fallback.  The mapping follows SCB
access enable, the hardware's nonlinear size encoding, the programmed PCI
base, and bridge reset.  The 32-vector MSI block implements the Linux-used
doorbell, status, mask and clear path on GIC SPI 148.

Qtests use QEMU's small ``edu`` PCI endpoint to cover endpoint configuration,
BAR probing and outbound forwarding, INTx, bidirectional DMA through a high
PCI address, rejection outside the inbound window, reset gating, and MSI
masking and clearing.  The earlier root, MDIO, absent-BDF, outbound-window and
system-reset coverage remains.

A fixed VIA ``1106:3483`` revision-one endpoint is populated at downstream
slot zero by the Pi 4 Model B and Pi 400 boards, but not by the BCM2711 SoC
model.  Its 4 KiB 64-bit BAR wraps QEMU's xHCI engine with the captured VL805
register layout: 32 slots, four interrupters, one USB 2 port followed by four
USB 3 ports, capability length ``0x20``, doorbells at ``0x100`` and runtime
registers at ``0x200``.  The captured PM, four-vector MSI, PCIe v2, AER and
xHCI extended-capability identities are present; MSI-X is absent.

Qtests additionally prove the VL805 PCI identity and BAR, captured PCI and
xHCI capability values, endpoint reset, and the functional data path.  A USB
port reset produces a port-status event which the xHCI engine writes through
the private PCI DMA window before sending MSI through the BCM2711 doorbell to
GIC SPI 148.  Both ``raspi4b`` and ``raspi400`` instantiate the fixed endpoint,
its stable ``vl805.0`` USB bus, and a high-speed VIA ``2109:3431`` hub.  The
``raspi400`` board additionally places its low-speed ``04d9:0007`` integrated
keyboard on hub port four.  Its two HID interfaces reproduce the captured
keyboard and consumer-control descriptors; QEMU input events currently drive
only the keyboard interface.

The VL805 advertises an ERST Max exponent of three and accepts up to eight
event-ring segments.  The engine validates each segment, crosses noncontiguous
segment boundaries, interprets the ERDP DESI hint, and toggles the producer
cycle only on a complete ring wrap.  Qtests exercise a two-segment active ring
and file-migrate it while the producer is in the second segment, then prove
that the destination writes the next event at the migrated segment and index.
Version-one single-segment xHCI migration streams remain loadable.

The firmware property interface implements ``NOTIFY_XHCI_RESET`` for the
board-level VL805 at firmware-encoded BDF ``01:00.0``.  It returns the
hardware-observed status value ``0`` for that address and ``0xffffffff`` for
an invalid address.  A valid notification halts a running xHCI core while
preserving its other operational registers and PCI configuration; it is not a
PCI or xHCI cold reset.  BCM2711 PERST performs the fundamental endpoint reset,
while this call tells VideoCore firmware to initialize the VL805 afterward.
Qtests cover both status results, the valid-request halt, and preserved
``CONFIG``, BAR and command-register state.

The guest-visible path passes the pinned unmodified Linux v7.2 acceptance boot
on both boards.  Those boots also attach a disposable mass-storage device to
the VIA hub, verify data transfers with the guest block cache flushed, retain
the data across xHCI driver unbind/rebind, and complete a fresh transfer after
re-enumeration.  Remaining approximations are narrower: the advertised debug
capability and vendor-specific extended capabilities provide captured
read-only identity values rather than proprietary behavior, link-up follows
the two reset bits rather than endpoint link training, MDIO operations
complete immediately, and the host controller-event interrupt is not yet
modeled.  The Pi 400 consumer-control HID endpoint enumerates but does not yet
produce media-key events.

BCM2711 host requirements
-------------------------

QEMU's PCIe bus, root-port and configuration-dispatch cores can be reused, but
the guest-visible front end must implement BCM2711 registers used by Linux's
``brcmstb`` driver. The minimum credible model includes:

* the controller window at CPU address ``0xfd500000``;
* root configuration at offsets ``0x0000`` through ``0x0fff``, indirect
  downstream configuration data at ``0x8000`` through ``0x8fff``, and its
  selector at ``0x9000``;
* PERST/reset at ``0x9210`` and link status at ``0x4068``;
* revision and the observable MDIO/SSC completion behavior;
* dynamically programmed outbound windows rather than a fixed translation;
* INTx A through D on GIC SPIs 143 through 146;
* controller-event and MSI interrupts on SPIs 147 and 148;
* a private PCI bus-master address space limited by BCM2711 ``dma-ranges``;
* MSI status, mask, clear and doorbell behavior; and
* migration state for translation, reset, link, interrupt and index registers.

The private DMA address space is required. Routing PCI DMA directly to QEMU's
system memory would incorrectly expose CPU peripheral ranges and would collide
with the MSI target near the top of the 32-bit PCI address space.

The pristine Linux v7.2 DT uses a 64 MiB outbound window from PCI
``0xf8000000`` to CPU ``0x600000000`` and a 3 GiB identity DMA range.  The DT
expanded by the firmware on the captured Pi 400 instead uses a 1 GiB window
from PCI ``0xc0000000`` to CPU ``0x600000000`` and maps the 4 GiB PCI DMA
range beginning at ``0x400000000`` to CPU address zero.  The model must honor
the values the guest programs instead of choosing one layout at build time.

Pi 400 hardware evidence
------------------------

A read-only capture on 2026-08-22 confirms the controller aperture at
``0xfd500000`` with size ``0x9310`` and this PCI topology:

* root port ``00:00.0 [14e4:2711]``, class ``0604``, revision ``0x20``;
* VL805 ``01:00.0 [1106:3483]``, class ``0c0330``, revision ``0x01``;
* a 5 GT/s x1 link with spread-spectrum clocking;
* a 4 KiB 64-bit VL805 BAR mapped at CPU address ``0x600000000``; and
* xHCI using MSI vector zero.

The endpoint exposes PM at ``0x80``, a 64-bit four-vector MSI capability at
``0x90``, PCIe v2 at ``0xc4``, and AER at ``0x100``.  Its xHCI capability
registers report 32 slots, four interrupters and five protocol ports, with the
single USB 2 port numbered before the four USB 3 ports.  The capability,
doorbell and runtime offsets are ``0x20``, ``0x100`` and ``0x200``.

The device tree assigns INTx A through D to SPIs 143 through 146, the
controller event to SPI 147, and MSI to SPI 148.  USB enumeration shows a
480 Mbit/s USB 2 root hub with one port leading to a four-port high-speed
``2109:3431`` hub.  The ``04d9:0007`` Pi 400 keyboard is a 1.5 Mbit/s device on
hub port four with two HID interfaces.  The separate USB 3 root hub has four
5 Gbit/s ports.  Controller revision ``0x0320`` at offset ``0x406c`` is
currently inferred from the observed PCI revision and Linux behavior; it has
not yet been confirmed by a raw MMIO capture.

A controlled follow-up with the xHCI driver unbound tested property tag
``0x00030058`` directly through ``/dev/vcio``.  Request data ``0x00100000``
(bus 1, slot 0, function 0) was replaced with status ``0``; request data
``0`` was replaced with ``0xffffffff``.  In both cases a deliberately set
xHCI ``CONFIG`` value remained unchanged.  The VL805 BAR also survived the
driver unbind/rebind cycle, after which the external hub and integrated
keyboard enumerated again.  A second request while the driver was active
changed ``USBCMD`` from ``0x5`` to ``0`` and ``USBSTS`` from ``0`` to
``0x1009``, while preserving ``CONFIG`` at ``0x20``.  USB was restored by an
immediate driver unbind/rebind.  Active use is outside Linux's normal sequence,
so the model reproduces the durable halt and preserved-register behavior, not
the incidental error bits.  This evidence corrects the earlier assumption
that the notification either does nothing or cold-resets the endpoint.  It
does not characterize proprietary firmware loading after physical PERST on
every Pi 4 board revision.

VL805 requirements
------------------

The endpoint wraps QEMU's xHCI engine in a VL805 register and PCI personality.
It identifies as VIA ``1106:3483`` and differs from generic ``qemu-xhci`` in
BAR size, capability length, doorbell and runtime offsets, ports, slots,
interrupters, ERST maximum and PCI capabilities.  It exposes the captured PM,
MSI, PCIe and AER capability chain and omits MSI-X.

MSI, private DMA, event delivery and the fixed board topology are required
parts of the guest-visible model, not optional enumeration shortcuts.

Implementation stages
---------------------

1. Implement the BCM2711 host, real root port, indirect configuration,
   reset/link/MDIO behavior and dynamic outbound windows. Prove root-port
   configuration and safe absent-device access.  This slice is implemented.
2. Add the private inbound DMA mapping and complete MSI delivery.  This slice
   is implemented and covered with a DMA-capable test endpoint.
3. Add the VL805 xHCI personality, populate it from the Pi board model, and
   prove endpoint enumeration, DMA-backed events, MSI and PERST.  This slice
   is implemented and covered by qtests.
4. Implement the firmware ``NOTIFY_XHCI_RESET`` request with the observed
   valid/invalid BDF status values and controller halt without turning it into
   a cold reset or attempting to emulate proprietary VL805 firmware.  This
   slice is implemented and covered by qtests.
5. Add the Pi 4-family VIA hub and Pi 400 integrated-keyboard topology.  This
   slice is implemented and covered by qtests and Linux acceptance boots.

Required tests
--------------

Qtests cover reset and link transitions, revision and MDIO completion, root
and endpoint IDs, indirect configuration, both known outbound-window layouts,
DMA below 3 GiB, DMA rejection outside the inbound range, INTx, MSI masking
and clearing, firmware-notification status, halt-without-cold-reset behavior,
preserved register state, fixed USB topology, multi-segment event rings, and
active-ring migration.

The pinned unmodified Linux v7.2 acceptance test passes on ``raspi4b`` and
``raspi400`` with their upstream DTBs.  It verifies ``14e4:2711`` and
``1106:3483``, both xHCI root hubs, the ``2109:3431`` hub, and nonzero MSI
activity.  On ``raspi400`` it additionally verifies ``04d9:0007`` and both HID
interfaces.  It unbinds and rebinds the xHCI driver, proves that the hub,
keyboard, HID interfaces and disposable USB mass-storage device re-enumerate
as applicable, verifies storage data across the rebind, performs a second
write/read cycle, observes fresh MSI activity, and obtains a DHCP lease through
GENET.  Pi 4 Model B correctly has no integrated keyboard.

External work under review
--------------------------

The July 2026 `working PCIe/GENET series
<https://patchew.org/QEMU/20260725004219.66222-1-marcelomanzo@gmail.com/>`_ and
its `enumeration follow-up
<https://patchew.org/QEMU/20260725124231.86233-1-marcelomanzo%40gmail.com/>`_
are useful scaffolding, as is the `original 2024 Pi 4 series
<https://patchew.org/QEMU/20240226000259.2752893-1-sergey.kambalin%40auriga.com/>`_.
They are references, not code to import wholesale into this fork.

Review notes for those unmerged patches include unsigned offset underflow that
hides root configuration, an uninitialized internal configuration region,
root writes that bypass ``pci_bridge_write_config()`` and bridge-window
updates, missing CPU/PCI outbound translation, decimal revision ``20``
producing ``0x14`` instead of the observed ``0x20``, all-ones controller reset
values, no private DMA, MSI or migration, and a fixed legacy window.  These
are patch-series defects rather than bugs in a released QEMU model and must
remain classified separately in the upstream tracker.
