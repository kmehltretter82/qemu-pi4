/*
 * Broadcom BCM2711 GENET v5 Ethernet controller
 *
 * Copyright (C) 2026 Karl Mehltretter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This model implements the GENET v5 programming interface used by the
 * BCM2711 Linux driver.  Earlier GENET revisions have different descriptor
 * and register layouts and are intentionally outside its scope.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/net/bcm2838_genet.h"
#include "hw/net/mii.h"
#include "migration/vmstate.h"
#include "net/checksum.h"
#include "net/eth.h"
#include "system/dma.h"

/* Register block offsets */
#define GENET_SYS_REV_CTRL              0x0000
#define GENET_EXT_RGMII_OOB_CTRL        0x008c
#define GENET_INTRL2_0_BASE             0x0200
#define GENET_INTRL2_1_BASE             0x0240
#define GENET_RBUF_CTRL                 0x0300
#define GENET_TBUF_CTRL                 0x0600
#define GENET_UMAC_CMD                  0x0808
#define GENET_UMAC_MAC0                 0x080c
#define GENET_UMAC_MAC1                 0x0810
#define GENET_UMAC_MODE                 0x0844
#define GENET_UMAC_MDIO_CMD             0x0e14

/* Interrupt controller registers */
#define INTRL2_CPU_STAT                 0x00
#define INTRL2_CPU_SET                  0x04
#define INTRL2_CPU_CLEAR                0x08
#define INTRL2_CPU_MASK_STATUS          0x0c
#define INTRL2_CPU_MASK_SET             0x10
#define INTRL2_CPU_MASK_CLEAR           0x14

/* Interrupt sources */
#define UMAC_IRQ_LINK_UP                BIT(4)
#define UMAC_IRQ_LINK_DOWN              BIT(5)
#define UMAC_IRQ_MDIO_DONE              BIT(23)
#define UMAC_IRQ_MDIO_ERROR             BIT(24)
#define UMAC_IRQ1_RX_SHIFT              16

/* UniMAC command and link bits */
#define UMAC_CMD_TX_EN                  BIT(0)
#define UMAC_CMD_RX_EN                  BIT(1)
#define UMAC_CMD_SW_RESET               BIT(13)
#define UMAC_MODE_LINK_STATUS           BIT(5)
#define RGMII_LINK                      BIT(4)

/* UniMAC MDIO command fields */
#define MDIO_START_BUSY                 BIT(29)
#define MDIO_READ_FAIL                  BIT(28)
#define MDIO_OP_MASK                    (3U << 26)
#define MDIO_OP_READ                    (2U << 26)
#define MDIO_OP_WRITE                   (1U << 26)
#define MDIO_PHY_SHIFT                  21
#define MDIO_PHY_MASK                   0x1f
#define MDIO_REG_SHIFT                  16
#define MDIO_REG_MASK                   0x1f

/* GENET v4/v5 DMA layout */
#define GENET_RDMA_DESC_BASE            0x2000
#define GENET_TDMA_DESC_BASE            0x4000
#define GENET_DMA_DESC_SIZE             0x0c
#define GENET_DMA_DESC_COUNT            256
#define GENET_RDMA_RING_BASE            0x2c00
#define GENET_TDMA_RING_BASE            0x4c00
#define GENET_DMA_RING_SIZE             0x40
#define GENET_RDMA_COMMON_BASE          0x3040
#define GENET_TDMA_COMMON_BASE          0x5040

/* Per-ring registers */
#define DMA_RING_READ_WRITE_PTR_LO      0x00
#define DMA_RING_CONS_PROD_INDEX        0x08
#define DMA_RING_PROD_CONS_INDEX        0x0c
#define DMA_RING_BUF_SIZE               0x10
#define DMA_RING_START_ADDR_LO          0x14
#define DMA_RING_END_ADDR_LO            0x1c

/* Common DMA registers */
#define DMA_RING_CFG                    0x00
#define DMA_CTRL                        0x04
#define DMA_STATUS                      0x08
#define DMA_EN                          BIT(0)
#define DMA_ENABLED_MASK                0x1ffff

/* Descriptor length/status fields */
#define DMA_BUFLENGTH_SHIFT             16
#define DMA_BUFLENGTH_MASK              0x0fff
#define DMA_EOP                         0x4000
#define DMA_SOP                         0x2000
#define DMA_TX_DO_CSUM                  0x0010
#define DMA_RX_BROADCAST                0x0040
#define DMA_RX_MULTICAST                0x0020

#define GENET_STATUS_BLOCK_SIZE         64
#define GENET_RX_PAD_SIZE               2
#define GENET_RX_PREFIX_SIZE            (GENET_STATUS_BLOCK_SIZE + \
                                         GENET_RX_PAD_SIZE)

#define BCM54213PE_PHY_ID1              0x600d
#define BCM54213PE_PHY_ID2              0x84a2

static inline uint32_t genet_reg_read(BCM2838GenetState *s, hwaddr offset)
{
    return s->regs[offset >> 2];
}

static inline void genet_reg_write(BCM2838GenetState *s, hwaddr offset,
                                   uint32_t value)
{
    s->regs[offset >> 2] = value;
}

static bool genet_irq_decode(hwaddr offset, unsigned int *index,
                             hwaddr *reg)
{
    if (offset >= GENET_INTRL2_0_BASE &&
        offset <= GENET_INTRL2_0_BASE + INTRL2_CPU_MASK_CLEAR) {
        *index = 0;
        *reg = offset - GENET_INTRL2_0_BASE;
        return true;
    }

    if (offset >= GENET_INTRL2_1_BASE &&
        offset <= GENET_INTRL2_1_BASE + INTRL2_CPU_MASK_CLEAR) {
        *index = 1;
        *reg = offset - GENET_INTRL2_1_BASE;
        return true;
    }

    return false;
}

static hwaddr genet_irq_base(unsigned int index)
{
    return index ? GENET_INTRL2_1_BASE : GENET_INTRL2_0_BASE;
}

static void genet_update_irqs(BCM2838GenetState *s)
{
    for (unsigned int i = 0; i < ARRAY_SIZE(s->irq); i++) {
        hwaddr base = genet_irq_base(i);
        uint32_t status = genet_reg_read(s, base + INTRL2_CPU_STAT);
        uint32_t mask = genet_reg_read(s, base + INTRL2_CPU_MASK_STATUS);

        qemu_set_irq(s->irq[i], (status & ~mask) != 0);
    }
}

static void genet_raise_irq(BCM2838GenetState *s, unsigned int index,
                            uint32_t bits)
{
    hwaddr status = genet_irq_base(index) + INTRL2_CPU_STAT;

    genet_reg_write(s, status, genet_reg_read(s, status) | bits);
    genet_update_irqs(s);
}

static bool genet_backend_link_up(BCM2838GenetState *s)
{
    return s->nic && !qemu_get_queue(s->nic)->link_down;
}

static bool genet_phy_link_up(BCM2838GenetState *s)
{
    uint16_t bmcr = s->phy_regs[MII_BMCR];

    return genet_backend_link_up(s) &&
           !(bmcr & (MII_BMCR_PDOWN | MII_BMCR_ISOLATE));
}

static void genet_update_link_registers(BCM2838GenetState *s)
{
    uint32_t mode = genet_reg_read(s, GENET_UMAC_MODE);
    uint32_t rgmii = genet_reg_read(s, GENET_EXT_RGMII_OOB_CTRL);

    if (genet_phy_link_up(s)) {
        mode |= UMAC_MODE_LINK_STATUS;
        rgmii |= RGMII_LINK;
    } else {
        mode &= ~UMAC_MODE_LINK_STATUS;
        rgmii &= ~RGMII_LINK;
    }

    genet_reg_write(s, GENET_UMAC_MODE, mode);
    genet_reg_write(s, GENET_EXT_RGMII_OOB_CTRL, rgmii);
}

static void genet_phy_reset(BCM2838GenetState *s)
{
    memset(s->phy_regs, 0, sizeof(s->phy_regs));

    s->phy_regs[MII_BMCR] = MII_BMCR_AUTOEN | MII_BMCR_FD |
                            MII_BMCR_SPEED1000;
    s->phy_regs[MII_BMSR] = MII_BMSR_100TX_FD | MII_BMSR_100TX_HD |
                            MII_BMSR_10T_FD | MII_BMSR_10T_HD |
                            MII_BMSR_EXTSTAT | MII_BMSR_AN_COMP |
                            MII_BMSR_AUTONEG | MII_BMSR_EXTCAP;
    s->phy_regs[MII_PHYID1] = BCM54213PE_PHY_ID1;
    s->phy_regs[MII_PHYID2] = BCM54213PE_PHY_ID2;
    s->phy_regs[MII_ANAR] = MII_ANAR_TXFD | MII_ANAR_TX |
                            MII_ANAR_10FD | MII_ANAR_10 |
                            MII_ANAR_PAUSE | MII_ANAR_PAUSE_ASYM |
                            MII_ANAR_CSMACD;
    s->phy_regs[MII_ANLPAR] = MII_ANLPAR_ACK | MII_ANLPAR_TXFD |
                              MII_ANLPAR_TX | MII_ANLPAR_10FD |
                              MII_ANLPAR_10 | MII_ANLPAR_PAUSE |
                              MII_ANLPAR_PAUSEASY | MII_ANLPAR_CSMACD;
    s->phy_regs[MII_CTRL1000] = MII_CTRL1000_FULL;
    s->phy_regs[MII_STAT1000] = MII_STAT1000_LOK | MII_STAT1000_ROK |
                                MII_STAT1000_FULL;
    s->phy_regs[MII_EXTSTAT] = MII_EXTSTAT_1000T_FD |
                               MII_EXTSTAT_1000T_HD;
}

static uint16_t genet_phy_read(BCM2838GenetState *s, unsigned int reg)
{
    uint16_t value = s->phy_regs[reg & MDIO_REG_MASK];

    if (reg == MII_BMSR) {
        if (genet_phy_link_up(s)) {
            value |= MII_BMSR_LINK_ST | MII_BMSR_AN_COMP;
        } else {
            value &= ~(MII_BMSR_LINK_ST | MII_BMSR_AN_COMP);
        }
    }

    return value;
}

static void genet_phy_write(BCM2838GenetState *s, unsigned int reg,
                            uint16_t value)
{
    reg &= MDIO_REG_MASK;

    switch (reg) {
    case MII_BMCR:
        if (value & MII_BMCR_RESET) {
            genet_phy_reset(s);
        } else {
            s->phy_regs[reg] = value & ~MII_BMCR_ANRESTART;
        }
        genet_update_link_registers(s);
        break;
    case MII_BMSR:
    case MII_PHYID1:
    case MII_PHYID2:
    case MII_STAT1000:
    case MII_EXTSTAT:
        break;
    default:
        /* Preserve vendor-specific selectors used by the BCM54xx driver. */
        s->phy_regs[reg] = value;
        break;
    }
}

static void genet_mdio_command(BCM2838GenetState *s, uint32_t command)
{
    unsigned int phy = (command >> MDIO_PHY_SHIFT) & MDIO_PHY_MASK;
    unsigned int reg = (command >> MDIO_REG_SHIFT) & MDIO_REG_MASK;
    uint32_t result = command & ~(MDIO_START_BUSY | MDIO_READ_FAIL);
    uint32_t irq = UMAC_IRQ_MDIO_DONE;

    if (phy != s->phy_addr) {
        result = (result & ~UINT16_MAX) | UINT16_MAX | MDIO_READ_FAIL;
        irq |= UMAC_IRQ_MDIO_ERROR;
    } else if ((command & MDIO_OP_MASK) == MDIO_OP_READ) {
        result = (result & ~UINT16_MAX) | genet_phy_read(s, reg);
    } else if ((command & MDIO_OP_MASK) == MDIO_OP_WRITE) {
        genet_phy_write(s, reg, command & UINT16_MAX);
    } else {
        result |= MDIO_READ_FAIL;
        irq |= UMAC_IRQ_MDIO_ERROR;
    }

    genet_reg_write(s, GENET_UMAC_MDIO_CMD, result);
    genet_raise_irq(s, 0, irq);
}

static hwaddr genet_ring_reg(unsigned int ring_base, unsigned int ring,
                             unsigned int reg)
{
    return ring_base + ring * GENET_DMA_RING_SIZE + reg;
}

static bool genet_dma_ring_enabled(BCM2838GenetState *s, bool tx,
                                   unsigned int ring)
{
    hwaddr common = tx ? GENET_TDMA_COMMON_BASE : GENET_RDMA_COMMON_BASE;
    uint32_t config = genet_reg_read(s, common + DMA_RING_CFG);
    uint32_t ctrl = genet_reg_read(s, common + DMA_CTRL);

    return (config & BIT(ring)) && (ctrl & DMA_EN) &&
           (ctrl & BIT(ring + 1));
}

static unsigned int genet_ring_buffer_count(BCM2838GenetState *s,
                                            bool tx, unsigned int ring)
{
    hwaddr base = tx ? GENET_TDMA_RING_BASE : GENET_RDMA_RING_BASE;

    return genet_reg_read(s, genet_ring_reg(base, ring, DMA_RING_BUF_SIZE)) >>
           16;
}

static unsigned int genet_ring_descriptor(BCM2838GenetState *s, bool tx,
                                          unsigned int ring,
                                          uint16_t index)
{
    hwaddr base = tx ? GENET_TDMA_RING_BASE : GENET_RDMA_RING_BASE;
    unsigned int count = genet_ring_buffer_count(s, tx, ring);
    uint32_t start = genet_reg_read(s, genet_ring_reg(
                                       base, ring,
                                       DMA_RING_START_ADDR_LO));

    if (!count) {
        return GENET_DMA_DESC_COUNT;
    }

    return start / 3 + index % count;
}

static uint64_t genet_descriptor_address(BCM2838GenetState *s, bool tx,
                                         unsigned int descriptor)
{
    hwaddr base = tx ? GENET_TDMA_DESC_BASE : GENET_RDMA_DESC_BASE;
    hwaddr offset = base + descriptor * GENET_DMA_DESC_SIZE;
    uint64_t address = genet_reg_read(s, offset + 4);

    address |= (uint64_t)genet_reg_read(s, offset + 8) << 32;
    return address;
}

static void genet_tx_descriptor(BCM2838GenetState *s, unsigned int ring,
                                unsigned int descriptor)
{
    hwaddr offset = GENET_TDMA_DESC_BASE +
                    descriptor * GENET_DMA_DESC_SIZE;
    uint32_t length_status = genet_reg_read(s, offset);
    uint64_t address = genet_descriptor_address(s, true, descriptor);
    unsigned int length = (length_status >> DMA_BUFLENGTH_SHIFT) &
                          DMA_BUFLENGTH_MASK;
    unsigned int skip = 0;
    unsigned int copy_length;

    if (length_status & DMA_SOP) {
        s->tx_packet_active[ring] = true;
        s->tx_packet_len[ring] = 0;
        s->tx_packet_checksum[ring] = !!(length_status & DMA_TX_DO_CSUM);
        skip = MIN(length, GENET_STATUS_BLOCK_SIZE);
    } else if (!s->tx_packet_active[ring]) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      TYPE_BCM2838_GENET ": TX descriptor without SOP\n");
        return;
    }

    copy_length = length - skip;
    if (copy_length > BCM2838_GENET_MAX_FRAME_SIZE -
                      s->tx_packet_len[ring]) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      TYPE_BCM2838_GENET ": oversized TX frame\n");
        s->tx_packet_active[ring] = false;
        s->tx_packet_len[ring] = 0;
        return;
    }

    if (copy_length &&
        dma_memory_read(&s->dma_as, address + skip,
                        s->tx_packet[ring] + s->tx_packet_len[ring],
                        copy_length, MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      TYPE_BCM2838_GENET ": TX DMA read failed at 0x%" PRIx64
                      "\n", address + skip);
        s->tx_packet_active[ring] = false;
        s->tx_packet_len[ring] = 0;
        return;
    }

    s->tx_packet_len[ring] += copy_length;

    if (length_status & DMA_EOP) {
        if (s->tx_packet_active[ring] && s->tx_packet_len[ring]) {
            if (s->tx_packet_checksum[ring]) {
                net_checksum_calculate(s->tx_packet[ring],
                                       s->tx_packet_len[ring], CSUM_ALL);
            }
            qemu_send_packet(qemu_get_queue(s->nic), s->tx_packet[ring],
                             s->tx_packet_len[ring]);
        }
        s->tx_packet_active[ring] = false;
        s->tx_packet_len[ring] = 0;
    }
}

static void genet_process_tx_ring(BCM2838GenetState *s, unsigned int ring)
{
    hwaddr ring_base = genet_ring_reg(GENET_TDMA_RING_BASE, ring, 0);
    uint16_t consumer = genet_reg_read(s, ring_base +
                                      DMA_RING_CONS_PROD_INDEX);
    uint16_t producer = genet_reg_read(s, ring_base +
                                      DMA_RING_PROD_CONS_INDEX);
    unsigned int count = genet_ring_buffer_count(s, true, ring);
    unsigned int completed = 0;

    if (!(genet_reg_read(s, GENET_UMAC_CMD) & UMAC_CMD_TX_EN) ||
        !genet_dma_ring_enabled(s, true, ring) || !count) {
        return;
    }

    while (consumer != producer && completed < count) {
        unsigned int descriptor = genet_ring_descriptor(s, true, ring,
                                                        consumer);

        if (descriptor >= GENET_DMA_DESC_COUNT) {
            break;
        }

        genet_tx_descriptor(s, ring, descriptor);
        consumer++;
        completed++;

        genet_reg_write(s, ring_base + DMA_RING_CONS_PROD_INDEX,
                        consumer);
        genet_reg_write(s, ring_base + DMA_RING_READ_WRITE_PTR_LO,
                        genet_ring_descriptor(s, true, ring, consumer) * 3);
    }

    if (completed) {
        genet_raise_irq(s, 1, BIT(ring));
    }
}

static void genet_kick_tx(BCM2838GenetState *s)
{
    for (unsigned int ring = 0; ring < BCM2838_GENET_NUM_RINGS; ring++) {
        genet_process_tx_ring(s, ring);
    }
}

static bool genet_can_receive(NetClientState *nc)
{
    BCM2838GenetState *s = qemu_get_nic_opaque(nc);
    hwaddr ring = GENET_RDMA_RING_BASE;
    uint16_t producer = genet_reg_read(s, ring +
                                      DMA_RING_CONS_PROD_INDEX);
    uint16_t consumer = genet_reg_read(s, ring +
                                      DMA_RING_PROD_CONS_INDEX);
    unsigned int count = genet_ring_buffer_count(s, false, 0);
    unsigned int descriptor;

    if (!genet_phy_link_up(s) ||
        !(genet_reg_read(s, GENET_UMAC_CMD) & UMAC_CMD_RX_EN) ||
        !genet_dma_ring_enabled(s, false, 0) || !count ||
        (uint16_t)(producer - consumer) >= count) {
        return false;
    }

    descriptor = genet_ring_descriptor(s, false, 0, producer);
    return descriptor < GENET_DMA_DESC_COUNT &&
           genet_descriptor_address(s, false, descriptor) != 0;
}

static ssize_t genet_receive(NetClientState *nc, const uint8_t *buf,
                             size_t size)
{
    BCM2838GenetState *s = qemu_get_nic_opaque(nc);
    hwaddr ring = GENET_RDMA_RING_BASE;
    uint16_t producer;
    unsigned int descriptor;
    hwaddr desc_offset;
    uint64_t address;
    uint8_t packet[BCM2838_GENET_MAX_FRAME_SIZE] = { 0 };
    size_t dma_length = size + GENET_RX_PREFIX_SIZE;
    uint32_t flags = DMA_SOP | DMA_EOP;
    uint32_t length_status;

    if (!genet_can_receive(nc)) {
        return -1;
    }

    producer = genet_reg_read(s, ring + DMA_RING_CONS_PROD_INDEX);
    descriptor = genet_ring_descriptor(s, false, 0, producer);
    desc_offset = GENET_RDMA_DESC_BASE +
                  descriptor * GENET_DMA_DESC_SIZE;
    address = genet_descriptor_address(s, false, descriptor);

    if (dma_length > sizeof(packet)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      TYPE_BCM2838_GENET ": oversized RX frame\n");
        return size;
    }

    if (size >= 6 && is_broadcast_ether_addr(buf)) {
        flags |= DMA_RX_BROADCAST;
    } else if (size >= 1 && is_multicast_ether_addr(buf)) {
        flags |= DMA_RX_MULTICAST;
    }

    length_status = (dma_length << DMA_BUFLENGTH_SHIFT) | flags;
    stl_le_p(packet, length_status);
    memcpy(packet + GENET_RX_PREFIX_SIZE, buf, size);

    if (dma_memory_write(&s->dma_as, address, packet, dma_length,
                         MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      TYPE_BCM2838_GENET ": RX DMA write failed at 0x%" PRIx64
                      "\n", address);
        return size;
    }

    genet_reg_write(s, desc_offset, length_status);
    producer++;
    genet_reg_write(s, ring + DMA_RING_CONS_PROD_INDEX, producer);
    genet_reg_write(s, ring + DMA_RING_READ_WRITE_PTR_LO,
                    genet_ring_descriptor(s, false, 0, producer) * 3);
    genet_raise_irq(s, 1, BIT(UMAC_IRQ1_RX_SHIFT));

    return size;
}

static void genet_link_status_changed(NetClientState *nc)
{
    BCM2838GenetState *s = qemu_get_nic_opaque(nc);
    bool was_up = !!(genet_reg_read(s, GENET_UMAC_MODE) &
                     UMAC_MODE_LINK_STATUS);
    bool is_up = genet_phy_link_up(s);

    genet_update_link_registers(s);
    if (was_up != is_up) {
        genet_raise_irq(s, 0, is_up ? UMAC_IRQ_LINK_UP :
                                      UMAC_IRQ_LINK_DOWN);
    }

    if (is_up) {
        qemu_flush_queued_packets(nc);
    }
}

static uint64_t genet_read(void *opaque, hwaddr offset, unsigned int size)
{
    BCM2838GenetState *s = opaque;
    unsigned int irq;
    hwaddr reg;

    if (genet_irq_decode(offset, &irq, &reg)) {
        hwaddr base = genet_irq_base(irq);

        switch (reg) {
        case INTRL2_CPU_STAT:
        case INTRL2_CPU_MASK_STATUS:
            return genet_reg_read(s, base + reg);
        default:
            return 0;
        }
    }

    if (offset == GENET_SYS_REV_CTRL) {
        /* Raw major 6 is reported by Linux as GENET v5. */
        return 0x06000000;
    }

    if (offset == GENET_RDMA_COMMON_BASE + DMA_STATUS ||
        offset == GENET_TDMA_COMMON_BASE + DMA_STATUS) {
        hwaddr common = offset - DMA_STATUS;

        return ~genet_reg_read(s, common + DMA_CTRL) & DMA_ENABLED_MASK;
    }

    if (offset == GENET_UMAC_MODE ||
        offset == GENET_EXT_RGMII_OOB_CTRL) {
        genet_update_link_registers(s);
    }

    return genet_reg_read(s, offset);
}

static void genet_write(void *opaque, hwaddr offset, uint64_t value,
                        unsigned int size)
{
    BCM2838GenetState *s = opaque;
    NetClientState *nc = qemu_get_queue(s->nic);
    unsigned int irq;
    hwaddr reg;

    if (genet_irq_decode(offset, &irq, &reg)) {
        hwaddr base = genet_irq_base(irq);
        uint32_t status = genet_reg_read(s, base + INTRL2_CPU_STAT);
        uint32_t mask = genet_reg_read(s, base + INTRL2_CPU_MASK_STATUS);

        switch (reg) {
        case INTRL2_CPU_SET:
            status |= value;
            break;
        case INTRL2_CPU_CLEAR:
            status &= ~value;
            break;
        case INTRL2_CPU_MASK_SET:
            mask |= value;
            break;
        case INTRL2_CPU_MASK_CLEAR:
            mask &= ~value;
            break;
        default:
            return;
        }

        genet_reg_write(s, base + INTRL2_CPU_STAT, status);
        genet_reg_write(s, base + INTRL2_CPU_MASK_STATUS, mask);
        genet_update_irqs(s);
        return;
    }

    if (offset == GENET_SYS_REV_CTRL ||
        offset == GENET_RDMA_COMMON_BASE + DMA_STATUS ||
        offset == GENET_TDMA_COMMON_BASE + DMA_STATUS) {
        return;
    }

    genet_reg_write(s, offset, value);

    if (offset == GENET_UMAC_MDIO_CMD && (value & MDIO_START_BUSY)) {
        genet_mdio_command(s, value);
        return;
    }

    if (offset == GENET_UMAC_CMD) {
        genet_kick_tx(s);
        if (genet_can_receive(nc)) {
            qemu_flush_queued_packets(nc);
        }
        return;
    }

    if (offset == GENET_TDMA_COMMON_BASE + DMA_CTRL) {
        genet_kick_tx(s);
        return;
    }

    if (offset == GENET_RDMA_COMMON_BASE + DMA_CTRL) {
        if (genet_can_receive(nc)) {
            qemu_flush_queued_packets(nc);
        }
        return;
    }

    if (offset >= GENET_TDMA_RING_BASE &&
        offset < GENET_TDMA_COMMON_BASE &&
        (offset - GENET_TDMA_RING_BASE) % GENET_DMA_RING_SIZE ==
        DMA_RING_PROD_CONS_INDEX) {
        unsigned int ring = (offset - GENET_TDMA_RING_BASE) /
                            GENET_DMA_RING_SIZE;

        genet_process_tx_ring(s, ring);
        return;
    }

    if (offset >= GENET_RDMA_RING_BASE &&
        offset < GENET_RDMA_COMMON_BASE &&
        (offset - GENET_RDMA_RING_BASE) % GENET_DMA_RING_SIZE ==
        DMA_RING_PROD_CONS_INDEX && genet_can_receive(nc)) {
        qemu_flush_queued_packets(nc);
    }
}

static const MemoryRegionOps genet_ops = {
    .read = genet_read,
    .write = genet_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static NetClientInfo genet_net_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = genet_can_receive,
    .receive = genet_receive,
    .link_status_changed = genet_link_status_changed,
};

static void genet_reset(DeviceState *dev)
{
    BCM2838GenetState *s = BCM2838_GENET(dev);
    const uint8_t *mac = s->conf.macaddr.a;

    memset(s->regs, 0, sizeof(s->regs));
    memset(s->tx_packet_len, 0, sizeof(s->tx_packet_len));
    memset(s->tx_packet_active, 0, sizeof(s->tx_packet_active));
    memset(s->tx_packet_checksum, 0, sizeof(s->tx_packet_checksum));

    genet_reg_write(s, GENET_INTRL2_0_BASE + INTRL2_CPU_MASK_STATUS,
                    UINT32_MAX);
    genet_reg_write(s, GENET_INTRL2_1_BASE + INTRL2_CPU_MASK_STATUS,
                    UINT32_MAX);

    genet_reg_write(s, GENET_UMAC_MAC0,
                    (uint32_t)mac[0] << 24 | (uint32_t)mac[1] << 16 |
                    (uint32_t)mac[2] << 8 | mac[3]);
    genet_reg_write(s, GENET_UMAC_MAC1,
                    (uint32_t)mac[4] << 8 | mac[5]);

    genet_phy_reset(s);
    genet_update_link_registers(s);
    genet_update_irqs(s);
}

static void genet_init(Object *obj)
{
    BCM2838GenetState *s = BCM2838_GENET(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &genet_ops, s,
                          TYPE_BCM2838_GENET, BCM2838_GENET_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq[0]);
    sysbus_init_irq(sbd, &s->irq[1]);
}

static void genet_realize(DeviceState *dev, Error **errp)
{
    BCM2838GenetState *s = BCM2838_GENET(dev);

    if (!s->dma_mr) {
        error_setg(errp, TYPE_BCM2838_GENET " 'dma-memory' link not set");
        return;
    }

    address_space_init(&s->dma_as, s->dma_mr, TYPE_BCM2838_GENET "-dma");

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&genet_net_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id,
                          &dev->mem_reentrancy_guard, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static int genet_post_load(void *opaque, int version_id)
{
    BCM2838GenetState *s = opaque;

    genet_update_link_registers(s);
    genet_update_irqs(s);
    return 0;
}

static const VMStateDescription vmstate_genet = {
    .name = TYPE_BCM2838_GENET,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = genet_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_MACADDR(conf.macaddr, BCM2838GenetState),
        VMSTATE_UINT32_ARRAY(regs, BCM2838GenetState,
                             BCM2838_GENET_MMIO_SIZE / sizeof(uint32_t)),
        VMSTATE_UINT16_ARRAY(phy_regs, BCM2838GenetState, 32),
        VMSTATE_UINT8(phy_addr, BCM2838GenetState),
        VMSTATE_UINT32_ARRAY(tx_packet_len, BCM2838GenetState,
                             BCM2838_GENET_NUM_RINGS),
        VMSTATE_UINT8_ARRAY(tx_packet_active, BCM2838GenetState,
                            BCM2838_GENET_NUM_RINGS),
        VMSTATE_UINT8_ARRAY(tx_packet_checksum, BCM2838GenetState,
                            BCM2838_GENET_NUM_RINGS),
        VMSTATE_UINT8_2DARRAY(tx_packet, BCM2838GenetState,
                              BCM2838_GENET_NUM_RINGS,
                              BCM2838_GENET_MAX_FRAME_SIZE),
        VMSTATE_END_OF_LIST()
    },
};

static const Property genet_properties[] = {
    DEFINE_NIC_PROPERTIES(BCM2838GenetState, conf),
    DEFINE_PROP_UINT8("phy-addr", BCM2838GenetState, phy_addr, 1),
    DEFINE_PROP_LINK("dma-memory", BCM2838GenetState, dma_mr,
                     TYPE_MEMORY_REGION, MemoryRegion *),
};

static void genet_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = genet_realize;
    dc->vmsd = &vmstate_genet;
    device_class_set_legacy_reset(dc, genet_reset);
    device_class_set_props(dc, genet_properties);
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

static const TypeInfo genet_type_info = {
    .name = TYPE_BCM2838_GENET,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2838GenetState),
    .instance_init = genet_init,
    .class_init = genet_class_init,
};

static void genet_register_types(void)
{
    type_register_static(&genet_type_info);
}

type_init(genet_register_types)
