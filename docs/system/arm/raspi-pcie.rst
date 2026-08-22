Raspberry Pi 4 PCIe and VL805 implementation plan
=================================================

The Pi 4 family's external USB ports are not attached to BCM2711's internal
xHCI block. They are reached through the BCM2711 PCIe root complex and a VIA
Labs VL805 PCIe xHCI controller. The Pi 400's integrated keyboard is behind
that VL805 path as well. Enabling the internal xHCI or substituting a generic
ECAM host would therefore produce a useful virtual computer but not a faithful
Pi 400.

Target topology
---------------

The first complete topology should be::

  BCM2711 PCIe host at 0xfd500000
    00:00.0 Broadcom root port [14e4:2711]
      01:00.0 VIA Labs VL805 xHCI [1106:3483]
        USB 2 hub [2109:3431]
          Pi 400 keyboard [04d9:0007]
        USB 3 ports

The host controller belongs to the BCM2711 SoC. The fixed VL805 population is
a property of Pi 4 Model B and Pi 400 boards; it must not be built into the SoC
because Compute Module 4 exposes PCIe for external devices instead.

BCM2711 host requirements
-------------------------

QEMU's PCIe bus, root-port and configuration-dispatch cores can be reused, but
the guest-visible front end must implement BCM2711 registers used by Linux's
``brcmstb`` driver. The minimum credible model includes:

* the controller window at CPU address ``0xfd500000``;
* root configuration and indirect downstream configuration at offsets
  ``0x8000`` and ``0x9000``;
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

The active upstream Linux DT uses a 64 MiB outbound window from PCI
``0xf8000000`` to CPU ``0x600000000``. Older downstream trees use a 1 GiB
window beginning at PCI ``0xc0000000``. The model must honor the values the
guest programs instead of choosing one DT generation at build time.

VL805 requirements
------------------

The first functional endpoint can wrap QEMU's existing xHCI engine, but it must
identify as VIA ``1106:3483`` and should evolve into a parameterized VL805
personality. Generic ``qemu-xhci`` differs in BAR size, capability length,
doorbell and runtime offsets, ports, slots, interrupters and PCI capabilities.
A faithful personality should expose the hardware's PM, MSI, PCIe and AER
capability chain and omit MSI-X.

Disabling MSI is only a diagnostic fallback. The PCIe node must remain hidden
from the guest until DMA-backed xHCI events and MSI delivery work; enumeration
alone is not the completion criterion.

Implementation stages
---------------------

1. Implement the BCM2711 host, real root port, indirect configuration,
   reset/link/MDIO behavior and dynamic outbound windows. Prove root-port and
   VL805 configuration enumeration.
2. Add the private inbound DMA mapping and complete MSI delivery.
3. Add the VL805 xHCI personality and populate it from the Pi board model.
4. Implement the firmware ``NOTIFY_XHCI_RESET`` request as an observable
   endpoint reset without attempting to emulate proprietary VL805 firmware.
5. Add the Pi 400 hub and integrated-keyboard topology.

Required tests
--------------

Qtests must cover reset and link transitions, revision and MDIO completion,
root and endpoint IDs, indirect configuration, both known outbound-window
layouts, DMA below 3 GiB, DMA rejection outside the inbound range, INTx, MSI
masking and clearing, firmware-requested reset, and migration.

The Linux acceptance test must boot an unmodified Pi DT with its PCIe node
enabled. It must show ``14e4:2711`` and ``1106:3483`` in ``lspci -nn``,
enumerate xHCI root hubs, transfer keyboard or storage traffic, show increasing
MSI interrupt counters, and survive controller unbind/rebind or reset.

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
root writes that bypass bridge-window updates, missing CPU/PCI outbound
translation, all-ones controller reset values, no MSI, and a fixed legacy
window. These are patch-series defects rather than bugs in a released QEMU
model and must remain classified separately in the upstream tracker.
