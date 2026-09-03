/*
 * Broadcom BCM2711 PCIe host controller
 *
 * Copyright (C) 2026 Karl Mehltretter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/range.h"
#include "qemu/units.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/irq.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/msi.h"
#include "hw/pci/pcie.h"
#include "hw/pci/pcie_host.h"
#include "hw/pci/pcie_port.h"
#include "hw/pci-host/bcm2711.h"
#include "migration/vmstate.h"
#include "system/address-spaces.h"
#include "system/ioport.h"

#define BCM2711_PCIE_VENDOR_ID 0x14e4
#define BCM2711_PCIE_DEVICE_ID 0x2711
#define BCM2711_PCIE_REVISION  0x20

#define BCM2711_PCIE_EXP_CAP_OFFSET 0xac
#define BCM2711_PCIE_AER_CAP_OFFSET 0x100

#define BCM2711_PCIE_VENDOR_SPECIFIC_REG1 0x0188
#define BCM2711_PCIE_ID_VAL3             0x043c
#define BCM2711_PCIE_PRIV1_LINK_CAP      0x04dc
#define BCM2711_PCIE_PRIV1_ROOT_CAP      0x04f8

#define BCM2711_PCIE_MDIO_ADDR    0x1100
#define BCM2711_PCIE_MDIO_WR_DATA 0x1104
#define BCM2711_PCIE_MDIO_RD_DATA 0x1108
#define BCM2711_PCIE_MDIO_DONE    BIT(31)
#define BCM2711_PCIE_MDIO_SSC_ACTIVE BIT(10)
#define BCM2711_PCIE_MDIO_PLL_LOCK   BIT(11)

#define BCM2711_PCIE_MISC_CTRL     0x4008
#define BCM2711_PCIE_MISC_CTRL_SCB_ACCESS_EN BIT(12)
#define BCM2711_PCIE_OUT_LO(win)   (0x400c + (win) * 8)
#define BCM2711_PCIE_OUT_HI(win)   (0x4010 + (win) * 8)
#define BCM2711_PCIE_RC_BAR_LO(bar) (0x402c + ((bar) - 1) * 8)
#define BCM2711_PCIE_RC_BAR_HI(bar) (0x4030 + ((bar) - 1) * 8)
#define BCM2711_PCIE_RC_BAR_SIZE_MASK 0x1f
#define BCM2711_PCIE_MSI_BAR_LO    0x4044
#define BCM2711_PCIE_MSI_BAR_HI    0x4048
#define BCM2711_PCIE_MSI_DATA      0x404c
#define BCM2711_PCIE_MSI_BAR_ENABLE BIT(0)
#define BCM2711_PCIE_STATUS        0x4068
#define BCM2711_PCIE_REVISION_REG  0x406c
#define BCM2711_PCIE_BASE_LIMIT(win) (0x4070 + (win) * 4)
#define BCM2711_PCIE_BASE_HI(win)    (0x4080 + (win) * 8)
#define BCM2711_PCIE_LIMIT_HI(win)   (0x4084 + (win) * 8)
#define BCM2711_PCIE_HARD_DEBUG    0x4204

#define BCM2711_PCIE_MSI_STATUS      0x4500
#define BCM2711_PCIE_MSI_SET         0x4504
#define BCM2711_PCIE_MSI_CLEAR       0x4508
#define BCM2711_PCIE_MSI_MASK_STATUS 0x450c
#define BCM2711_PCIE_MSI_MASK_SET    0x4510
#define BCM2711_PCIE_MSI_MASK_CLEAR  0x4514
#define BCM2711_PCIE_MSI_NUM_VECTORS 32
#define BCM2711_PCIE_EVENT_IRQ       4
#define BCM2711_PCIE_MSI_IRQ         5

#define BCM2711_PCIE_STATUS_RC_MODE BIT(7)
#define BCM2711_PCIE_STATUS_DL_ACTIVE BIT(5)
#define BCM2711_PCIE_STATUS_PHY_LINK_UP BIT(4)

#define BCM2711_PCIE_EXT_CFG_DATA  0x8000
#define BCM2711_PCIE_EXT_CFG_INDEX 0x9000
#define BCM2711_PCIE_SW_INIT       0x9210
#define BCM2711_PCIE_SW_INIT_BRIDGE_RESET BIT(1)
#define BCM2711_PCIE_SW_INIT_PERST        BIT(0)

#define BCM2711_PCIE_HW_REVISION 0x0320
#define BCM2711_PCIE_INTX_SPI_BASE 143

static uint8_t *bcm2711_pcie_reg_ptr(BCM2711PcieHostState *s,
                                     hwaddr offset)
{
    assert(offset >= PCIE_CONFIG_SPACE_SIZE);
    assert(offset < BCM2711_PCIE_REGS_SIZE);
    return &s->regs[offset - PCIE_CONFIG_SPACE_SIZE];
}

static uint32_t bcm2711_pcie_reg_read32(BCM2711PcieHostState *s,
                                        hwaddr offset)
{
    return ldl_le_p(bcm2711_pcie_reg_ptr(s, offset));
}

static void bcm2711_pcie_reg_write32(BCM2711PcieHostState *s,
                                     hwaddr offset, uint32_t value)
{
    stl_le_p(bcm2711_pcie_reg_ptr(s, offset), value);
}

static bool bcm2711_pcie_link_up(BCM2711PcieHostState *s)
{
    uint32_t sw_init = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_SW_INIT);

    return !(sw_init & (BCM2711_PCIE_SW_INIT_BRIDGE_RESET |
                        BCM2711_PCIE_SW_INIT_PERST));
}

static void bcm2711_pcie_unmap_outbound(BCM2711PcieHostState *s)
{
    MemoryRegion *system_memory = get_system_memory();
    unsigned int i;

    memory_region_transaction_begin();
    for (i = 0; i < BCM2711_PCIE_NUM_OUT_WINDOWS; i++) {
        if (s->outbound_mapped[i]) {
            memory_region_del_subregion(system_memory,
                                        &s->outbound_alias[i]);
            s->outbound_mapped[i] = false;
        }
    }
    memory_region_transaction_commit();
}

static void bcm2711_pcie_update_outbound(BCM2711PcieHostState *s)
{
    MemoryRegion *system_memory = get_system_memory();
    unsigned int i;

    memory_region_transaction_begin();
    for (i = 0; i < BCM2711_PCIE_NUM_OUT_WINDOWS; i++) {
        uint32_t base_limit;
        uint64_t base_mb;
        uint64_t limit_mb;
        uint64_t cpu_base;
        uint64_t pci_base;
        uint64_t size;

        if (s->outbound_mapped[i]) {
            memory_region_del_subregion(system_memory,
                                        &s->outbound_alias[i]);
            s->outbound_mapped[i] = false;
        }

        base_limit = bcm2711_pcie_reg_read32(
            s, BCM2711_PCIE_BASE_LIMIT(i));
        base_mb = extract32(base_limit, 4, 12) |
                  ((uint64_t)(bcm2711_pcie_reg_read32(
                      s, BCM2711_PCIE_BASE_HI(i)) & 0xff) << 12);
        limit_mb = extract32(base_limit, 20, 12) |
                   ((uint64_t)(bcm2711_pcie_reg_read32(
                       s, BCM2711_PCIE_LIMIT_HI(i)) & 0xff) << 12);
        if (limit_mb < base_mb) {
            continue;
        }

        cpu_base = base_mb * MiB;
        size = (limit_mb - base_mb + 1) * MiB;
        pci_base = deposit64(
            bcm2711_pcie_reg_read32(s, BCM2711_PCIE_OUT_LO(i)), 32, 32,
            bcm2711_pcie_reg_read32(s, BCM2711_PCIE_OUT_HI(i)));
        if (cpu_base + size < cpu_base || pci_base + size < pci_base) {
            continue;
        }

        memory_region_set_alias_offset(&s->outbound_alias[i], pci_base);
        memory_region_set_size(&s->outbound_alias[i], size);
        memory_region_add_subregion_overlap(system_memory, cpu_base,
                                            &s->outbound_alias[i], i + 1);
        s->outbound_mapped[i] = true;
    }
    memory_region_transaction_commit();
}

static void bcm2711_pcie_unmap_inbound(BCM2711PcieHostState *s)
{
    if (!s->inbound_mapped) {
        return;
    }

    memory_region_transaction_begin();
    memory_region_del_subregion(&s->dma_root, &s->inbound_alias);
    s->inbound_mapped = false;
    memory_region_transaction_commit();
}

static uint64_t bcm2711_pcie_inbound_size(uint32_t value)
{
    unsigned int encoding = value & BCM2711_PCIE_RC_BAR_SIZE_MASK;

    if (encoding >= 0x1c) {
        return 1ULL << (12 + encoding - 0x1c);
    }
    if (encoding >= 1 && encoding <= 0x15) {
        return 1ULL << (15 + encoding);
    }
    return 0;
}

static void bcm2711_pcie_update_inbound(BCM2711PcieHostState *s)
{
    uint32_t low = bcm2711_pcie_reg_read32(
        s, BCM2711_PCIE_RC_BAR_LO(2));
    uint64_t pci_base;
    uint64_t size;

    bcm2711_pcie_unmap_inbound(s);
    if (!bcm2711_pcie_link_up(s) ||
        !(bcm2711_pcie_reg_read32(s, BCM2711_PCIE_MISC_CTRL) &
          BCM2711_PCIE_MISC_CTRL_SCB_ACCESS_EN)) {
        return;
    }

    size = bcm2711_pcie_inbound_size(low);
    if (!size) {
        return;
    }

    pci_base = deposit64(low & ~BCM2711_PCIE_RC_BAR_SIZE_MASK, 32, 32,
                         bcm2711_pcie_reg_read32(
                             s, BCM2711_PCIE_RC_BAR_HI(2)));
    pci_base &= ~(size - 1);
    size = MIN(size, memory_region_size(s->ram_mr));
    if (!size || pci_base + size < pci_base) {
        return;
    }

    memory_region_transaction_begin();
    memory_region_set_alias_offset(&s->inbound_alias, 0);
    memory_region_set_size(&s->inbound_alias, size);
    memory_region_add_subregion_overlap(&s->dma_root, pci_base,
                                        &s->inbound_alias, 0);
    s->inbound_mapped = true;
    memory_region_transaction_commit();
}

static void bcm2711_pcie_unmap_msi(BCM2711PcieHostState *s)
{
    if (!s->msi_mapped) {
        return;
    }

    memory_region_transaction_begin();
    memory_region_del_subregion(&s->dma_root, &s->msi_doorbell);
    s->msi_mapped = false;
    memory_region_transaction_commit();
}

static void bcm2711_pcie_update_msi_mapping(BCM2711PcieHostState *s)
{
    uint32_t low = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_MSI_BAR_LO);
    uint64_t address;

    bcm2711_pcie_unmap_msi(s);
    if (!bcm2711_pcie_link_up(s) ||
        !(low & BCM2711_PCIE_MSI_BAR_ENABLE)) {
        return;
    }

    address = deposit64(low & ~0x3U, 32, 32,
                        bcm2711_pcie_reg_read32(s,
                                               BCM2711_PCIE_MSI_BAR_HI));
    if (address + 4 < address) {
        return;
    }

    memory_region_transaction_begin();
    memory_region_add_subregion_overlap(&s->dma_root, address,
                                        &s->msi_doorbell, 1);
    s->msi_mapped = true;
    memory_region_transaction_commit();
}

static void bcm2711_pcie_update_msi_irq(BCM2711PcieHostState *s)
{
    uint32_t status = bcm2711_pcie_reg_read32(s,
                                              BCM2711_PCIE_MSI_STATUS);
    uint32_t mask = bcm2711_pcie_reg_read32(
        s, BCM2711_PCIE_MSI_MASK_STATUS);
    bool level = bcm2711_pcie_link_up(s) && (status & ~mask);

    s->irq_level[BCM2711_PCIE_MSI_IRQ] = level;
    qemu_set_irq(s->irq[BCM2711_PCIE_MSI_IRQ], level);
}

/*
 * BCM2711 has a separate controller-event output in addition to the four
 * downstream INTx lines.  QEMU's generic root-port code reports AER and
 * slot-service notifications through the root port's PCI INTx state.  The
 * PCI bridge machinery also forwards ordinary downstream INTx through that
 * same root bus, so distinguish the root port's own state from the aggregate
 * count before driving the SoC outputs.  The generic slot implementation
 * retains a pending service notification separately, so that it survives an
 * unrelated AER update to the root port's PCI INTx state.
 */
static void bcm2711_pcie_update_pci_irqs(BCM2711PcieHostState *s)
{
    PCIHostState *host = PCI_HOST_BRIDGE(s);
    PCIDevice *root = PCI_DEVICE(&s->root_port);
    bool root_irq_enabled = !pci_irq_disabled(root);
    unsigned int i;

    assert(host->bus);
    assert(host->bus->nirq == PCI_NUM_PINS);

    for (i = 0; i < PCI_NUM_PINS; i++) {
        bool root_irq = root_irq_enabled && (root->irq_state & BIT(i));
        int count = host->bus->irq_count[i] - root_irq;

        /* The root port can contribute at most one level to each pin. */
        assert(count >= 0);
        s->irq_level[i] = count != 0;
        qemu_set_irq(s->irq[i], s->irq_level[i]);
    }

    s->irq_level[BCM2711_PCIE_EVENT_IRQ] = root_irq_enabled &&
        (root->irq_state || root->exp.hpev_notified);
    qemu_set_irq(s->irq[BCM2711_PCIE_EVENT_IRQ],
                 s->irq_level[BCM2711_PCIE_EVENT_IRQ]);
}

static uint64_t bcm2711_pcie_msi_doorbell_read(void *opaque, hwaddr offset,
                                                unsigned size)
{
    return 0;
}

static void bcm2711_pcie_msi_doorbell_write(void *opaque, hwaddr offset,
                                             uint64_t value, unsigned size)
{
    BCM2711PcieHostState *s = opaque;
    uint32_t data = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_MSI_DATA);
    uint32_t status;
    uint16_t compare_mask = data >> 16;
    uint16_t incoming = value;
    uint16_t match = data;
    unsigned int vector;

    if ((incoming & compare_mask) != (match & compare_mask)) {
        return;
    }

    vector = incoming & ~compare_mask;
    if (vector >= BCM2711_PCIE_MSI_NUM_VECTORS) {
        return;
    }
    status = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_MSI_STATUS);
    bcm2711_pcie_reg_write32(s, BCM2711_PCIE_MSI_STATUS,
                             status | BIT(vector));
    bcm2711_pcie_update_msi_irq(s);
}

static const MemoryRegionOps bcm2711_pcie_msi_doorbell_ops = {
    .read = bcm2711_pcie_msi_doorbell_read,
    .write = bcm2711_pcie_msi_doorbell_write,
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

static bool bcm2711_pcie_is_outbound_register(hwaddr offset, unsigned size)
{
    return ranges_overlap(offset, size, BCM2711_PCIE_OUT_LO(0),
                          BCM2711_PCIE_OUT_HI(3) + 4 -
                          BCM2711_PCIE_OUT_LO(0)) ||
           ranges_overlap(offset, size, BCM2711_PCIE_BASE_LIMIT(0),
                          BCM2711_PCIE_LIMIT_HI(3) + 4 -
                          BCM2711_PCIE_BASE_LIMIT(0));
}

static uint64_t bcm2711_pcie_absent_value(unsigned size)
{
    return MAKE_64BIT_MASK(0, size * 8);
}

static uint64_t bcm2711_pcie_downstream_config_read(
    BCM2711PcieHostState *s, hwaddr offset, unsigned size)
{
    PCIHostState *host = PCI_HOST_BRIDGE(s);
    uint32_t index = bcm2711_pcie_reg_read32(s,
                                             BCM2711_PCIE_EXT_CFG_INDEX);
    uint32_t where = PCIE_MMCFG_CONFOFFSET(index) +
                     offset - BCM2711_PCIE_EXT_CFG_DATA;
    PCIDevice *pdev;

    if (!bcm2711_pcie_link_up(s) || where + size > PCIE_CONFIG_SPACE_SIZE) {
        return bcm2711_pcie_absent_value(size);
    }

    pdev = pci_find_device(host->bus, PCIE_MMCFG_BUS(index),
                           PCIE_MMCFG_DEVFN(index));
    if (!pdev) {
        return bcm2711_pcie_absent_value(size);
    }

    return pci_host_config_read_common(pdev, where, pci_config_size(pdev),
                                       size);
}

static void bcm2711_pcie_downstream_config_write(BCM2711PcieHostState *s,
                                                  hwaddr offset,
                                                  uint64_t value,
                                                  unsigned size)
{
    PCIHostState *host = PCI_HOST_BRIDGE(s);
    uint32_t index = bcm2711_pcie_reg_read32(s,
                                             BCM2711_PCIE_EXT_CFG_INDEX);
    uint32_t where = PCIE_MMCFG_CONFOFFSET(index) +
                     offset - BCM2711_PCIE_EXT_CFG_DATA;
    PCIDevice *pdev;

    if (!bcm2711_pcie_link_up(s) || where + size > PCIE_CONFIG_SPACE_SIZE) {
        return;
    }

    pdev = pci_find_device(host->bus, PCIE_MMCFG_BUS(index),
                           PCIE_MMCFG_DEVFN(index));
    if (pdev) {
        pci_host_config_write_common(pdev, where, pci_config_size(pdev),
                                     value, size);
    }
}

static uint32_t bcm2711_pcie_status(BCM2711PcieHostState *s)
{
    uint32_t status = BCM2711_PCIE_STATUS_RC_MODE;

    if (bcm2711_pcie_link_up(s)) {
        status |= BCM2711_PCIE_STATUS_DL_ACTIVE |
                  BCM2711_PCIE_STATUS_PHY_LINK_UP;
    }
    return status;
}

static bool bcm2711_pcie_read_special(BCM2711PcieHostState *s,
                                      hwaddr offset, unsigned size,
                                      uint64_t *value)
{
    hwaddr reg = offset & ~3ULL;
    unsigned shift = (offset & 3) * 8;
    uint32_t reg_value;

    if (offset + size > reg + 4) {
        return false;
    }

    switch (reg) {
    case BCM2711_PCIE_STATUS:
        reg_value = bcm2711_pcie_status(s);
        break;
    case BCM2711_PCIE_REVISION_REG:
        reg_value = BCM2711_PCIE_HW_REVISION;
        break;
    case BCM2711_PCIE_MDIO_RD_DATA:
        reg_value = BCM2711_PCIE_MDIO_DONE;
        if ((bcm2711_pcie_reg_read32(s, BCM2711_PCIE_MDIO_ADDR) & 0xffff)
            == 1) {
            reg_value |= BCM2711_PCIE_MDIO_SSC_ACTIVE |
                         BCM2711_PCIE_MDIO_PLL_LOCK;
        }
        break;
    default:
        return false;
    }

    *value = extract64(reg_value, shift, size * 8);
    return true;
}

static uint64_t bcm2711_pcie_host_read(void *opaque, hwaddr offset,
                                       unsigned size)
{
    BCM2711PcieHostState *s = opaque;
    PCIDevice *root = PCI_DEVICE(&s->root_port);
    uint64_t value;

    if (offset < PCIE_CONFIG_SPACE_SIZE) {
        if (offset + size > PCIE_CONFIG_SPACE_SIZE) {
            return bcm2711_pcie_absent_value(size);
        }
        return pci_host_config_read_common(root, offset,
                                           pci_config_size(root), size);
    }

    if (offset >= BCM2711_PCIE_EXT_CFG_DATA &&
        offset + size <= BCM2711_PCIE_EXT_CFG_DATA +
                         PCIE_CONFIG_SPACE_SIZE) {
        return bcm2711_pcie_downstream_config_read(s, offset, size);
    }

    if (offset + size > BCM2711_PCIE_REGS_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: out-of-range access, %u bytes at 0x%04" HWADDR_PRIx
                      "\n", __func__, size, offset);
        return bcm2711_pcie_absent_value(size);
    }

    if (bcm2711_pcie_read_special(s, offset, size, &value)) {
        return value;
    }
    return ldn_le_p(bcm2711_pcie_reg_ptr(s, offset), size);
}

static void bcm2711_pcie_handle_sw_init(BCM2711PcieHostState *s,
                                        uint32_t old_value)
{
    uint32_t value = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_SW_INIT);

    if (!(old_value & BCM2711_PCIE_SW_INIT_PERST) &&
        (value & BCM2711_PCIE_SW_INIT_PERST)) {
        bus_cold_reset(BUS(&PCI_BRIDGE(&s->root_port)->sec_bus));
    }
}

static bool bcm2711_pcie_handle_msi_write(BCM2711PcieHostState *s,
                                           hwaddr offset, uint64_t value,
                                           unsigned size)
{
    hwaddr reg = offset & ~3ULL;
    uint32_t bits;
    uint32_t status;
    uint32_t mask;

    if (offset + size > reg + 4) {
        return false;
    }

    bits = (uint32_t)value << ((offset & 3) * 8);
    status = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_MSI_STATUS);
    mask = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_MSI_MASK_STATUS);

    switch (reg) {
    case BCM2711_PCIE_MSI_STATUS:
    case BCM2711_PCIE_MSI_MASK_STATUS:
        break;
    case BCM2711_PCIE_MSI_SET:
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_MSI_STATUS,
                                 status | bits);
        break;
    case BCM2711_PCIE_MSI_CLEAR:
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_MSI_STATUS,
                                 status & ~bits);
        break;
    case BCM2711_PCIE_MSI_MASK_SET:
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_MSI_MASK_STATUS,
                                 mask | bits);
        break;
    case BCM2711_PCIE_MSI_MASK_CLEAR:
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_MSI_MASK_STATUS,
                                 mask & ~bits);
        break;
    default:
        return false;
    }

    bcm2711_pcie_update_msi_irq(s);
    return true;
}

static void bcm2711_pcie_host_write(void *opaque, hwaddr offset,
                                    uint64_t value, unsigned size)
{
    BCM2711PcieHostState *s = opaque;
    PCIDevice *root = PCI_DEVICE(&s->root_port);
    uint32_t old_sw_init;

    if (offset < PCIE_CONFIG_SPACE_SIZE) {
        if (offset + size <= PCIE_CONFIG_SPACE_SIZE) {
            pci_host_config_write_common(root, offset, pci_config_size(root),
                                         value, size);
            bcm2711_pcie_update_pci_irqs(s);
        }
        return;
    }

    if (offset >= BCM2711_PCIE_EXT_CFG_DATA &&
        offset + size <= BCM2711_PCIE_EXT_CFG_DATA +
                         PCIE_CONFIG_SPACE_SIZE) {
        bcm2711_pcie_downstream_config_write(s, offset, value, size);
        return;
    }

    if (offset + size > BCM2711_PCIE_REGS_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: out-of-range access, %u bytes at 0x%04" HWADDR_PRIx
                      "\n", __func__, size, offset);
        return;
    }

    if (bcm2711_pcie_handle_msi_write(s, offset, value, size)) {
        return;
    }

    if (ranges_overlap(offset, size, BCM2711_PCIE_STATUS, 4) ||
        ranges_overlap(offset, size, BCM2711_PCIE_REVISION_REG, 4) ||
        ranges_overlap(offset, size, BCM2711_PCIE_MDIO_RD_DATA, 4)) {
        return;
    }

    old_sw_init = bcm2711_pcie_reg_read32(s, BCM2711_PCIE_SW_INIT);
    stn_le_p(bcm2711_pcie_reg_ptr(s, offset), size, value);

    if (ranges_overlap(offset, size, BCM2711_PCIE_MDIO_WR_DATA, 4)) {
        uint32_t mdio = bcm2711_pcie_reg_read32(s,
                                                BCM2711_PCIE_MDIO_WR_DATA);

        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_MDIO_WR_DATA,
                                 mdio & ~BCM2711_PCIE_MDIO_DONE);
    }
    if (ranges_overlap(offset, size, BCM2711_PCIE_SW_INIT, 4)) {
        bcm2711_pcie_handle_sw_init(s, old_sw_init);
        bcm2711_pcie_update_inbound(s);
        bcm2711_pcie_update_msi_mapping(s);
        bcm2711_pcie_update_msi_irq(s);
    }
    if (bcm2711_pcie_is_outbound_register(offset, size)) {
        bcm2711_pcie_update_outbound(s);
    }
    if (ranges_overlap(offset, size, BCM2711_PCIE_MISC_CTRL, 4) ||
        ranges_overlap(offset, size, BCM2711_PCIE_RC_BAR_LO(2), 8)) {
        bcm2711_pcie_update_inbound(s);
    }
    if (ranges_overlap(offset, size, BCM2711_PCIE_MSI_BAR_LO, 8)) {
        bcm2711_pcie_update_msi_mapping(s);
    }
}

static const MemoryRegionOps bcm2711_pcie_host_ops = {
    .read = bcm2711_pcie_host_read,
    .write = bcm2711_pcie_host_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void bcm2711_pcie_set_irq(void *opaque, int irq_num, int level)
{
    BCM2711PcieHostState *s = opaque;

    assert(irq_num >= 0 && irq_num < PCI_NUM_PINS);
    (void)level;
    /* The PCI core updates bus->irq_count before invoking this callback. */
    bcm2711_pcie_update_pci_irqs(s);
}

static int bcm2711_pcie_map_irq(PCIDevice *pdev, int pin)
{
    return pin;
}

static PCIINTxRoute bcm2711_pcie_route_irq(void *opaque, int pin)
{
    PCIINTxRoute route = {
        .mode = PCI_INTX_ENABLED,
        .irq = BCM2711_PCIE_INTX_SPI_BASE + pin,
    };

    return route;
}

static void bcm2711_pcie_host_reset_hold(Object *obj, ResetType type)
{
    BCM2711PcieHostState *s = BCM2711_PCIE_HOST(obj);
    unsigned int i;

    bcm2711_pcie_unmap_outbound(s);
    bcm2711_pcie_unmap_inbound(s);
    bcm2711_pcie_unmap_msi(s);
    memset(s->regs, 0, sizeof(s->regs));
    bcm2711_pcie_reg_write32(s, BCM2711_PCIE_SW_INIT,
                             BCM2711_PCIE_SW_INIT_BRIDGE_RESET |
                             BCM2711_PCIE_SW_INIT_PERST);

    /* A base greater than the limit keeps every outbound window disabled. */
    for (i = 0; i < BCM2711_PCIE_NUM_OUT_WINDOWS; i++) {
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_BASE_LIMIT(i), 0xfff0);
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_BASE_HI(i), 0xff);
    }
    bcm2711_pcie_reg_write32(s, BCM2711_PCIE_MSI_MASK_STATUS,
                             UINT32_MAX);
    for (i = 0; i < BCM2711_PCIE_NUM_IRQS; i++) {
        s->irq_level[i] = 0;
        qemu_set_irq(s->irq[i], 0);
    }
}

static int bcm2711_pcie_host_post_load(void *opaque, int version_id)
{
    BCM2711PcieHostState *s = opaque;
    unsigned int i;

    bcm2711_pcie_update_outbound(s);
    bcm2711_pcie_update_inbound(s);
    bcm2711_pcie_update_msi_mapping(s);
    for (i = 0; i < BCM2711_PCIE_MSI_IRQ; i++) {
        qemu_set_irq(s->irq[i], s->irq_level[i]);
    }
    bcm2711_pcie_update_msi_irq(s);
    return 0;
}

static const VMStateDescription vmstate_bcm2711_pcie_host = {
    .name = "bcm2711-pcie-host",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2711_pcie_host_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8_ARRAY(regs, BCM2711PcieHostState,
                            BCM2711_PCIE_REGS_SIZE -
                            PCIE_CONFIG_SPACE_SIZE),
        VMSTATE_UINT8_ARRAY(irq_level, BCM2711PcieHostState,
                            BCM2711_PCIE_NUM_IRQS),
        VMSTATE_END_OF_LIST()
    },
};

static const char *bcm2711_pcie_root_bus_path(PCIHostState *host,
                                              PCIBus *root_bus)
{
    return "0000:00";
}

static AddressSpace *bcm2711_pcie_dma_iommu(PCIBus *bus, void *opaque,
                                             int devfn)
{
    BCM2711PcieHostState *s = opaque;

    return &s->dma_as;
}

static const PCIIOMMUOps bcm2711_pcie_iommu_ops = {
    .get_address_space = bcm2711_pcie_dma_iommu,
};

static void bcm2711_pcie_host_realize(DeviceState *dev, Error **errp)
{
    BCM2711PcieHostState *s = BCM2711_PCIE_HOST(dev);
    PCIHostState *host = PCI_HOST_BRIDGE(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    unsigned int i;

    if (!s->ram_mr) {
        error_setg(errp, "BCM2711 PCIe host requires a RAM link");
        return;
    }

    memory_region_init_io(&s->regs_mr, OBJECT(s), &bcm2711_pcie_host_ops, s,
                          "bcm2711-pcie-registers",
                          BCM2711_PCIE_REGS_SIZE);
    sysbus_init_mmio(sbd, &s->regs_mr);

    memory_region_init(&s->pci_mem, OBJECT(s), "bcm2711-pcie-memory",
                       UINT64_MAX);
    memory_region_init_io(&s->pci_mem_window, OBJECT(s), &unassigned_io_ops,
                          s, "bcm2711-pcie-memory-window", UINT64_MAX);
    memory_region_add_subregion(&s->pci_mem_window, 0, &s->pci_mem);
    memory_region_init(&s->pci_io, OBJECT(s), "bcm2711-pcie-io", 64 * KiB);

    memory_region_init(&s->dma_root, OBJECT(s), "bcm2711-pcie-dma",
                       UINT64_MAX);
    memory_region_init_alias(&s->inbound_alias, OBJECT(s),
                             "bcm2711-pcie-inbound", s->ram_mr, 0, 1);
    memory_region_init_io(&s->msi_doorbell, OBJECT(s),
                          &bcm2711_pcie_msi_doorbell_ops, s,
                          "bcm2711-pcie-msi-doorbell", 4);
    address_space_init(&s->dma_as, &s->dma_root, "bcm2711-pcie-dma");

    for (i = 0; i < BCM2711_PCIE_NUM_OUT_WINDOWS; i++) {
        g_autofree char *name = g_strdup_printf("bcm2711-pcie-outbound-%u",
                                                i);

        memory_region_init_alias(&s->outbound_alias[i], OBJECT(s), name,
                                 &s->pci_mem_window, 0, 1);
    }
    for (i = 0; i < BCM2711_PCIE_NUM_IRQS; i++) {
        sysbus_init_irq(sbd, &s->irq[i]);
    }

    host->bus = pci_register_root_bus(dev, "pcie.0", bcm2711_pcie_set_irq,
                                      bcm2711_pcie_map_irq, s, &s->pci_mem,
                                      &s->pci_io, 0, PCI_NUM_PINS,
                                      TYPE_PCIE_BUS);
    host->bus->flags |= PCI_BUS_EXTENDED_CONFIG_SPACE;
    pci_bus_set_route_irq_fn(host->bus, bcm2711_pcie_route_irq);
    pci_setup_iommu(host->bus, &bcm2711_pcie_iommu_ops, s);

    if (!qdev_realize(DEVICE(&s->root_port), BUS(host->bus), errp)) {
        pci_unregister_root_bus(host->bus);
        host->bus = NULL;
        address_space_destroy(&s->dma_as);
        return;
    }
}

static void bcm2711_pcie_host_unrealize(DeviceState *dev)
{
    BCM2711PcieHostState *s = BCM2711_PCIE_HOST(dev);
    PCIHostState *host = PCI_HOST_BRIDGE(dev);
    unsigned int i;

    bcm2711_pcie_unmap_outbound(s);
    bcm2711_pcie_unmap_inbound(s);
    bcm2711_pcie_unmap_msi(s);
    if (host->bus) {
        pci_unregister_root_bus(host->bus);
        host->bus = NULL;
    }
    for (i = 0; i < BCM2711_PCIE_NUM_IRQS; i++) {
        s->irq_level[i] = 0;
        qemu_set_irq(s->irq[i], 0);
    }
    address_space_destroy(&s->dma_as);
}

static void bcm2711_pcie_host_init(Object *obj)
{
    BCM2711PcieHostState *s = BCM2711_PCIE_HOST(obj);

    object_initialize_child(obj, "root-port", &s->root_port,
                            TYPE_BCM2711_PCIE_ROOT_PORT);
    qdev_prop_set_int32(DEVICE(&s->root_port), "addr", PCI_DEVFN(0, 0));
    qdev_prop_set_bit(DEVICE(&s->root_port), "multifunction", false);
    qdev_prop_set_bit(DEVICE(&s->root_port), "hotplug", false);
}

static const Property bcm2711_pcie_host_properties[] = {
    DEFINE_PROP_LINK("ram", BCM2711PcieHostState, ram_mr,
                     TYPE_MEMORY_REGION, MemoryRegion *),
};

static void bcm2711_pcie_host_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIHostBridgeClass *hc = PCI_HOST_BRIDGE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    dc->desc = "Broadcom BCM2711 PCIe host controller";
    dc->realize = bcm2711_pcie_host_realize;
    dc->unrealize = bcm2711_pcie_host_unrealize;
    dc->user_creatable = false;
    dc->fw_name = "pci";
    dc->vmsd = &vmstate_bcm2711_pcie_host;
    device_class_set_props(dc, bcm2711_pcie_host_properties);
    hc->root_bus_path = bcm2711_pcie_root_bus_path;
    rc->phases.hold = bcm2711_pcie_host_reset_hold;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    msi_nonbroken = true;
}

static void bcm2711_pcie_root_port_realize(DeviceState *dev, Error **errp)
{
    PCIDevice *pdev = PCI_DEVICE(dev);
    PCIERootPortClass *rpc = PCIE_ROOT_PORT_GET_CLASS(dev);
    Error *local_err = NULL;

    rpc->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    pci_set_long(pdev->config + BCM2711_PCIE_ID_VAL3, 0x060400);
    pci_set_long(pdev->config + BCM2711_PCIE_PRIV1_LINK_CAP, 1 << 4);

    pci_set_long(pdev->wmask + BCM2711_PCIE_VENDOR_SPECIFIC_REG1, 0xc);
    pci_set_long(pdev->wmask + BCM2711_PCIE_ID_VAL3, 0x00ffffff);
    pci_set_long(pdev->wmask + BCM2711_PCIE_PRIV1_LINK_CAP, 0x1f0);
    pci_set_long(pdev->wmask + BCM2711_PCIE_PRIV1_ROOT_CAP, 0xf8);
}

static void bcm2711_pcie_root_port_reset_hold(Object *obj, ResetType type)
{
    PCIERootPortClass *rpc = PCIE_ROOT_PORT_GET_CLASS(obj);
    PCIDevice *pdev = PCI_DEVICE(obj);

    if (rpc->parent_phases.hold) {
        rpc->parent_phases.hold(obj, type);
    }

    pci_set_long(pdev->config + BCM2711_PCIE_VENDOR_SPECIFIC_REG1, 0);
    pci_set_long(pdev->config + BCM2711_PCIE_ID_VAL3, 0x060400);
    pci_set_long(pdev->config + BCM2711_PCIE_PRIV1_LINK_CAP, 1 << 4);
    pci_set_long(pdev->config + BCM2711_PCIE_PRIV1_ROOT_CAP, 0);
}

static void bcm2711_pcie_root_port_init(Object *obj)
{
    PCIBridge *bridge = PCI_BRIDGE(obj);
    PCIESlot *slot = PCIE_SLOT(obj);

    bridge->bus_name = "pcie.1";
    slot->speed = QEMU_PCI_EXP_LNK_5GT;
    slot->width = QEMU_PCI_EXP_LNK_X1;
}

static const VMStateDescription vmstate_bcm2711_pcie_root_port = {
    .name = "bcm2711-pcie-root-port",
    .priority = MIG_PRI_PCI_BUS,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = pcie_cap_slot_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj.parent_obj.parent_obj.parent_obj,
                           BCM2711PcieRootPortState),
        VMSTATE_STRUCT(parent_obj.parent_obj.parent_obj.parent_obj.exp.aer_log,
                       BCM2711PcieRootPortState, 0, vmstate_pcie_aer_log,
                       PCIEAERLog),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2711_pcie_root_port_class_init(ObjectClass *oc,
                                              const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);
    PCIERootPortClass *rpc = PCIE_ROOT_PORT_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    dc->desc = "Broadcom BCM2711 PCIe root port";
    dc->user_creatable = false;
    dc->vmsd = &vmstate_bcm2711_pcie_root_port;
    pc->vendor_id = BCM2711_PCIE_VENDOR_ID;
    pc->device_id = BCM2711_PCIE_DEVICE_ID;
    pc->revision = BCM2711_PCIE_REVISION;
    pc->class_id = PCI_CLASS_BRIDGE_PCI;
    device_class_set_parent_realize(dc, bcm2711_pcie_root_port_realize,
                                    &rpc->parent_realize);
    resettable_class_set_parent_phases(rc, NULL,
                                       bcm2711_pcie_root_port_reset_hold,
                                       NULL, &rpc->parent_phases);
    rpc->exp_offset = BCM2711_PCIE_EXP_CAP_OFFSET;
    rpc->aer_offset = BCM2711_PCIE_AER_CAP_OFFSET;
}

static const TypeInfo bcm2711_pcie_types[] = {
    {
        .name = TYPE_BCM2711_PCIE_ROOT_PORT,
        .parent = TYPE_PCIE_ROOT_PORT,
        .instance_size = sizeof(BCM2711PcieRootPortState),
        .instance_init = bcm2711_pcie_root_port_init,
        .class_init = bcm2711_pcie_root_port_class_init,
    },
    {
        .name = TYPE_BCM2711_PCIE_HOST,
        .parent = TYPE_PCI_HOST_BRIDGE,
        .instance_size = sizeof(BCM2711PcieHostState),
        .instance_init = bcm2711_pcie_host_init,
        .class_init = bcm2711_pcie_host_class_init,
    },
};

DEFINE_TYPES(bcm2711_pcie_types)
