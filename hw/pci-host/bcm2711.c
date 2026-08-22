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
#define BCM2711_PCIE_OUT_LO(win)   (0x400c + (win) * 8)
#define BCM2711_PCIE_OUT_HI(win)   (0x4010 + (win) * 8)
#define BCM2711_PCIE_STATUS        0x4068
#define BCM2711_PCIE_REVISION_REG  0x406c
#define BCM2711_PCIE_BASE_LIMIT(win) (0x4070 + (win) * 4)
#define BCM2711_PCIE_BASE_HI(win)    (0x4080 + (win) * 8)
#define BCM2711_PCIE_LIMIT_HI(win)   (0x4084 + (win) * 8)
#define BCM2711_PCIE_HARD_DEBUG    0x4204

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
    }
    if (bcm2711_pcie_is_outbound_register(offset, size)) {
        bcm2711_pcie_update_outbound(s);
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
    s->irq_level[irq_num] = !!level;
    qemu_set_irq(s->irq[irq_num], level);
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
    memset(s->regs, 0, sizeof(s->regs));
    bcm2711_pcie_reg_write32(s, BCM2711_PCIE_SW_INIT,
                             BCM2711_PCIE_SW_INIT_BRIDGE_RESET |
                             BCM2711_PCIE_SW_INIT_PERST);

    /* A base greater than the limit keeps every outbound window disabled. */
    for (i = 0; i < BCM2711_PCIE_NUM_OUT_WINDOWS; i++) {
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_BASE_LIMIT(i), 0xfff0);
        bcm2711_pcie_reg_write32(s, BCM2711_PCIE_BASE_HI(i), 0xff);
    }
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
    for (i = 0; i < BCM2711_PCIE_NUM_IRQS; i++) {
        qemu_set_irq(s->irq[i], s->irq_level[i]);
    }
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

static void bcm2711_pcie_host_realize(DeviceState *dev, Error **errp)
{
    BCM2711PcieHostState *s = BCM2711_PCIE_HOST(dev);
    PCIHostState *host = PCI_HOST_BRIDGE(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    unsigned int i;

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

    if (!qdev_realize(DEVICE(&s->root_port), BUS(host->bus), errp)) {
        pci_unregister_root_bus(host->bus);
        host->bus = NULL;
        return;
    }
}

static void bcm2711_pcie_host_unrealize(DeviceState *dev)
{
    BCM2711PcieHostState *s = BCM2711_PCIE_HOST(dev);
    PCIHostState *host = PCI_HOST_BRIDGE(dev);

    bcm2711_pcie_unmap_outbound(s);
    if (host->bus) {
        pci_unregister_root_bus(host->bus);
        host->bus = NULL;
    }
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
    hc->root_bus_path = bcm2711_pcie_root_bus_path;
    rc->phases.hold = bcm2711_pcie_host_reset_hold;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
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
