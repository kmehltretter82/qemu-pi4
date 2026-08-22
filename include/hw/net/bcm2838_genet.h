/*
 * Broadcom BCM2711 GENET v5 Ethernet controller
 *
 * Copyright (C) 2026 Karl Mehltretter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_NET_BCM2838_GENET_H
#define HW_NET_BCM2838_GENET_H

#include "hw/core/sysbus.h"
#include "net/net.h"

#define TYPE_BCM2838_GENET "bcm2838-genet"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2838GenetState, BCM2838_GENET)

#define BCM2838_GENET_MMIO_SIZE        0x10000
#define BCM2838_GENET_NUM_RINGS        17
#define BCM2838_GENET_MAX_FRAME_SIZE   2048

struct BCM2838GenetState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegion *dma_mr;
    AddressSpace dma_as;
    qemu_irq irq[2];

    NICState *nic;
    NICConf conf;

    uint32_t regs[BCM2838_GENET_MMIO_SIZE / sizeof(uint32_t)];
    uint16_t phy_regs[32];
    uint8_t phy_addr;

    uint32_t tx_packet_len[BCM2838_GENET_NUM_RINGS];
    uint8_t tx_packet_active[BCM2838_GENET_NUM_RINGS];
    uint8_t tx_packet_checksum[BCM2838_GENET_NUM_RINGS];
    uint8_t tx_packet[BCM2838_GENET_NUM_RINGS]
                     [BCM2838_GENET_MAX_FRAME_SIZE];
};

#endif /* HW_NET_BCM2838_GENET_H */
