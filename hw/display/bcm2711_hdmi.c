/*
 * BCM2711 HDMI transmitter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/display/bcm2711_hdmi.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/module.h"

#define HDMI_FIFO_CTL                  0x074
#define HDMI_RAM_PACKET_CONFIG         0x0bc
#define HDMI_RAM_PACKET_STATUS         0x0c4
#define HDMI_SCHEDULER_CONTROL         0x0e0
#define HDMI_HOTPLUG                   0x1a8

#define HDMI_FIFO_CTL_RECENTER_DONE    BIT(14)
#define HDMI_FIFO_VALID_WRITE_MASK     0xefff
#define HDMI_RAM_PACKET_ENABLE         BIT(16)
#define HDMI_SCHEDULER_HDMI_ACTIVE     BIT(1)
#define HDMI_SCHEDULER_MODE_HDMI       BIT(0)
#define HDMI_HOTPLUG_CONNECTED         BIT(0)

#define HDMI_DVP_CLOCK_STOP            0x0bc

static const uint16_t bcm2711_hdmi_bank_words[BCM2711_HDMI_BANKS] = {
    [BCM2711_HDMI_CORE] = 0x300 / 4,
    [BCM2711_HDMI_DVP] = 0x200 / 4,
    [BCM2711_HDMI_PHY] = 0x080 / 4,
    [BCM2711_HDMI_RM] = 0x080 / 4,
    [BCM2711_HDMI_PACKET] = 0x200 / 4,
    [BCM2711_HDMI_METADATA] = 0x400 / 4,
    [BCM2711_HDMI_CSC] = 0x080 / 4,
    [BCM2711_HDMI_CEC] = 0x100 / 4,
    [BCM2711_HDMI_HD] = 0x100 / 4,
};

static const char *const bcm2711_hdmi_bank_names[BCM2711_HDMI_BANKS] = {
    [BCM2711_HDMI_CORE] = TYPE_BCM2711_HDMI ".core",
    [BCM2711_HDMI_DVP] = TYPE_BCM2711_HDMI ".dvp",
    [BCM2711_HDMI_PHY] = TYPE_BCM2711_HDMI ".phy",
    [BCM2711_HDMI_RM] = TYPE_BCM2711_HDMI ".rm",
    [BCM2711_HDMI_PACKET] = TYPE_BCM2711_HDMI ".packet",
    [BCM2711_HDMI_METADATA] = TYPE_BCM2711_HDMI ".metadata",
    [BCM2711_HDMI_CSC] = TYPE_BCM2711_HDMI ".csc",
    [BCM2711_HDMI_CEC] = TYPE_BCM2711_HDMI ".cec",
    [BCM2711_HDMI_HD] = TYPE_BCM2711_HDMI ".hd",
};

static uint32_t *bcm2711_hdmi_reg(BCM2711HDMIRegBank *bank, hwaddr offset)
{
    return &bank->owner->regs[bank->first + (offset >> 2)];
}

static uint64_t bcm2711_hdmi_read(void *opaque, hwaddr offset,
                                  unsigned int size)
{
    BCM2711HDMIRegBank *bank = opaque;
    BCM2711HDMIState *s = bank->owner;
    uint32_t value = *bcm2711_hdmi_reg(bank, offset);

    if (bank->id == BCM2711_HDMI_CORE) {
        switch (offset) {
        case HDMI_HOTPLUG:
            return s->connected ? HDMI_HOTPLUG_CONNECTED : 0;
        case HDMI_FIFO_CTL:
            return value | HDMI_FIFO_CTL_RECENTER_DONE;
        case HDMI_RAM_PACKET_STATUS:
            return s->regs[bank->first +
                           (HDMI_RAM_PACKET_CONFIG >> 2)] & 0xffff;
        default:
            break;
        }
    }

    return value;
}

static void bcm2711_hdmi_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned int size)
{
    BCM2711HDMIRegBank *bank = opaque;
    uint32_t *reg = bcm2711_hdmi_reg(bank, offset);

    if (bank->id == BCM2711_HDMI_CORE) {
        switch (offset) {
        case HDMI_HOTPLUG:
        case HDMI_RAM_PACKET_STATUS:
            return;
        case HDMI_FIFO_CTL:
            *reg = ((uint32_t)value & HDMI_FIFO_VALID_WRITE_MASK) |
                   HDMI_FIFO_CTL_RECENTER_DONE;
            return;
        case HDMI_SCHEDULER_CONTROL:
            *reg = (uint32_t)value & ~HDMI_SCHEDULER_HDMI_ACTIVE;
            if (*reg & HDMI_SCHEDULER_MODE_HDMI) {
                *reg |= HDMI_SCHEDULER_HDMI_ACTIVE;
            }
            return;
        default:
            break;
        }
    }

    *reg = value;
}

static const MemoryRegionOps bcm2711_hdmi_ops = {
    .read = bcm2711_hdmi_read,
    .write = bcm2711_hdmi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void bcm2711_hdmi_reset_state(BCM2711HDMIState *s)
{
    BCM2711HDMIRegBank *core = &s->banks[BCM2711_HDMI_CORE];
    BCM2711HDMIRegBank *dvp = &s->banks[BCM2711_HDMI_DVP];

    memset(s->regs, 0, sizeof(s->regs));
    s->regs[core->first + (HDMI_FIFO_CTL >> 2)] =
        HDMI_FIFO_CTL_RECENTER_DONE;
    s->regs[dvp->first + (HDMI_DVP_CLOCK_STOP >> 2)] = 3;
}

static void bcm2711_hdmi_reset(DeviceState *dev)
{
    bcm2711_hdmi_reset_state(BCM2711_HDMI(dev));
}

static void bcm2711_hdmi_reset_input(void *opaque, int irq, int level)
{
    BCM2711HDMIState *s = opaque;

    if (level) {
        bcm2711_hdmi_reset_state(s);
    }
}

static void bcm2711_hdmi_clock_input(void *opaque, int irq, int level)
{
    BCM2711HDMIState *s = opaque;

    s->clock_enabled = level;
}

static int bcm2711_hdmi_post_load(void *opaque, int version_id)
{
    BCM2711HDMIState *s = opaque;
    BCM2711HDMIRegBank *core = &s->banks[BCM2711_HDMI_CORE];
    uint32_t *fifo = &s->regs[core->first + (HDMI_FIFO_CTL >> 2)];
    uint32_t *scheduler =
        &s->regs[core->first + (HDMI_SCHEDULER_CONTROL >> 2)];

    *fifo = (*fifo & HDMI_FIFO_VALID_WRITE_MASK) |
            HDMI_FIFO_CTL_RECENTER_DONE;
    *scheduler &= ~HDMI_SCHEDULER_HDMI_ACTIVE;
    if (*scheduler & HDMI_SCHEDULER_MODE_HDMI) {
        *scheduler |= HDMI_SCHEDULER_HDMI_ACTIVE;
    }
    return 0;
}

static const VMStateDescription vmstate_bcm2711_hdmi = {
    .name = TYPE_BCM2711_HDMI,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2711_hdmi_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, BCM2711HDMIState, BCM2711_HDMI_REGS),
        VMSTATE_BOOL(clock_enabled, BCM2711HDMIState),
        VMSTATE_END_OF_LIST()
    },
};

static const Property bcm2711_hdmi_properties[] = {
    DEFINE_PROP_BOOL("connected", BCM2711HDMIState, connected, true),
};

static void bcm2711_hdmi_init(Object *obj)
{
    BCM2711HDMIState *s = BCM2711_HDMI(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    uint16_t first = 0;

    for (unsigned int i = 0; i < BCM2711_HDMI_BANKS; i++) {
        BCM2711HDMIRegBank *bank = &s->banks[i];

        bank->owner = s;
        bank->first = first;
        bank->words = bcm2711_hdmi_bank_words[i];
        bank->id = i;
        memory_region_init_io(&bank->iomem, obj, &bcm2711_hdmi_ops, bank,
                              bcm2711_hdmi_bank_names[i], bank->words * 4);
        sysbus_init_mmio(sbd, &bank->iomem);
        first += bank->words;
    }
    g_assert(first == BCM2711_HDMI_REGS);

    qdev_init_gpio_in_named(DEVICE(obj), bcm2711_hdmi_reset_input,
                            "reset", 1);
    qdev_init_gpio_in_named(DEVICE(obj), bcm2711_hdmi_clock_input,
                            "clock-enable", 1);
}

static void bcm2711_hdmi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, bcm2711_hdmi_reset);
    device_class_set_props(dc, bcm2711_hdmi_properties);
    dc->vmsd = &vmstate_bcm2711_hdmi;
    dc->desc = "BCM2711 HDMI transmitter";
}

static const TypeInfo bcm2711_hdmi_info = {
    .name = TYPE_BCM2711_HDMI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2711HDMIState),
    .instance_init = bcm2711_hdmi_init,
    .class_init = bcm2711_hdmi_class_init,
};

static void bcm2711_hdmi_register_types(void)
{
    type_register_static(&bcm2711_hdmi_info);
}

type_init(bcm2711_hdmi_register_types)
