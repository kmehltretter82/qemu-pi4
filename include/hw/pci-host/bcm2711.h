/*
 * Broadcom BCM2711 PCIe host controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_PCI_HOST_BCM2711_H
#define HW_PCI_HOST_BCM2711_H

#include "hw/pci/pci_host.h"
#include "hw/pci/pcie_port.h"

#define TYPE_BCM2711_PCIE_HOST "bcm2711-pcie-host"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711PcieHostState, BCM2711_PCIE_HOST)

#define TYPE_BCM2711_PCIE_ROOT_PORT "bcm2711-pcie-root-port"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711PcieRootPortState,
                           BCM2711_PCIE_ROOT_PORT)

#define BCM2711_PCIE_REGS_SIZE       0x9310
#define BCM2711_PCIE_NUM_OUT_WINDOWS 4
#define BCM2711_PCIE_NUM_IRQS        6

struct BCM2711PcieRootPortState {
    PCIESlot parent_obj;
};

struct BCM2711PcieHostState {
    PCIHostState parent_obj;

    BCM2711PcieRootPortState root_port;

    MemoryRegion regs_mr;
    MemoryRegion pci_mem;
    MemoryRegion pci_mem_window;
    MemoryRegion pci_io;
    MemoryRegion outbound_alias[BCM2711_PCIE_NUM_OUT_WINDOWS];
    bool outbound_mapped[BCM2711_PCIE_NUM_OUT_WINDOWS];

    MemoryRegion dma_root;
    AddressSpace dma_as;
    MemoryRegion inbound_alias;
    MemoryRegion msi_doorbell;
    MemoryRegion *ram_mr;
    bool inbound_mapped;
    bool msi_mapped;

    qemu_irq irq[BCM2711_PCIE_NUM_IRQS];
    uint8_t irq_level[BCM2711_PCIE_NUM_IRQS];

    /* Root configuration occupies the first 4 KiB of the aperture. */
    uint8_t regs[BCM2711_PCIE_REGS_SIZE - PCIE_CONFIG_SPACE_SIZE];
};

#endif /* HW_PCI_HOST_BCM2711_H */
