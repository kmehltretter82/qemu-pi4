/*
 * BCM2711 always-on L2 interrupt controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/intc/bcm2838_aon_intr.h"
#include "hw/core/irq.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define AON_INTR_MMIO_SIZE       0x30
#define AON_INTR_BANK_STRIDE     0x18

#define AON_INTR_STATUS          0x00
#define AON_INTR_SET             0x04
#define AON_INTR_CLEAR           0x08
#define AON_INTR_MASK_STATUS     0x0c
#define AON_INTR_MASK_SET        0x10
#define AON_INTR_MASK_CLEAR      0x14

#define AON_INTR_VALID_MASK      ((1U << BCM2838_AON_INTR_LINES) - 1)

enum {
    AON_INTR_CPU,
    AON_INTR_PCI,
};

static void bcm2838_aon_intr_update(BCM2838AonIntrState *s)
{
    for (unsigned int bank = 0; bank < BCM2838_AON_INTR_OUTPUTS; bank++) {
        qemu_set_irq(s->irq[bank],
                     !!(s->status[bank] & ~s->mask[bank] &
                        AON_INTR_VALID_MASK));
    }
}

static void bcm2838_aon_intr_set(void *opaque, int irq, int level)
{
    BCM2838AonIntrState *s = opaque;
    uint32_t bit;
    bool rising;

    assert(irq >= 0 && irq < BCM2838_AON_INTR_LINES);
    bit = 1U << irq;

    rising = level && !(s->input_level & bit);
    if (level) {
        s->input_level |= bit;
    } else {
        s->input_level &= ~bit;
    }

    if (rising) {
        /* Each physical source feeds the independently masked CPU/PCI banks. */
        s->status[AON_INTR_CPU] |= bit;
        s->status[AON_INTR_PCI] |= bit;
        bcm2838_aon_intr_update(s);
    }
}

static uint64_t bcm2838_aon_intr_read(void *opaque, hwaddr offset,
                                      unsigned int size)
{
    BCM2838AonIntrState *s = opaque;
    unsigned int bank = offset / AON_INTR_BANK_STRIDE;
    hwaddr reg = offset % AON_INTR_BANK_STRIDE;

    if (bank >= BCM2838_AON_INTR_OUTPUTS) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2838_AON_INTR, offset);
        return 0;
    }

    switch (reg) {
    case AON_INTR_STATUS:
        return s->status[bank];
    case AON_INTR_MASK_STATUS:
        return s->mask[bank];
    case AON_INTR_SET:
    case AON_INTR_CLEAR:
    case AON_INTR_MASK_SET:
    case AON_INTR_MASK_CLEAR:
        return 0;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2838_AON_INTR, offset);
        return 0;
    }
}

static void bcm2838_aon_intr_write(void *opaque, hwaddr offset,
                                   uint64_t value, unsigned int size)
{
    BCM2838AonIntrState *s = opaque;
    unsigned int bank = offset / AON_INTR_BANK_STRIDE;
    hwaddr reg = offset % AON_INTR_BANK_STRIDE;
    uint32_t bits = value & AON_INTR_VALID_MASK;

    if (bank >= BCM2838_AON_INTR_OUTPUTS) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2838_AON_INTR, offset);
        return;
    }

    switch (reg) {
    case AON_INTR_SET:
        s->status[bank] |= bits;
        break;
    case AON_INTR_CLEAR:
        s->status[bank] &= ~bits;
        break;
    case AON_INTR_MASK_SET:
        s->mask[bank] |= bits;
        break;
    case AON_INTR_MASK_CLEAR:
        s->mask[bank] &= ~bits;
        break;
    case AON_INTR_STATUS:
    case AON_INTR_MASK_STATUS:
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2838_AON_INTR, offset);
        return;
    }

    bcm2838_aon_intr_update(s);
}

static const MemoryRegionOps bcm2838_aon_intr_ops = {
    .read = bcm2838_aon_intr_read,
    .write = bcm2838_aon_intr_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void bcm2838_aon_intr_reset(DeviceState *dev)
{
    BCM2838AonIntrState *s = BCM2838_AON_INTR(dev);

    memset(s->status, 0, sizeof(s->status));
    for (unsigned int bank = 0; bank < BCM2838_AON_INTR_OUTPUTS; bank++) {
        s->mask[bank] = AON_INTR_VALID_MASK;
    }
    /* External source levels are not controller state and survive reset. */
    bcm2838_aon_intr_update(s);
}

static int bcm2838_aon_intr_post_load(void *opaque, int version_id)
{
    BCM2838AonIntrState *s = BCM2838_AON_INTR(opaque);
    uint32_t invalid = s->input_level;

    for (unsigned int bank = 0; bank < BCM2838_AON_INTR_OUTPUTS; bank++) {
        invalid |= s->status[bank] | s->mask[bank];
    }
    if (invalid & ~AON_INTR_VALID_MASK) {
        return -EINVAL;
    }

    bcm2838_aon_intr_update(s);
    return 0;
}

static const VMStateDescription vmstate_bcm2838_aon_intr = {
    .name = TYPE_BCM2838_AON_INTR,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2838_aon_intr_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(status, BCM2838AonIntrState,
                             BCM2838_AON_INTR_OUTPUTS),
        VMSTATE_UINT32_ARRAY(mask, BCM2838AonIntrState,
                             BCM2838_AON_INTR_OUTPUTS),
        VMSTATE_UINT32(input_level, BCM2838AonIntrState),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2838_aon_intr_init(Object *obj)
{
    BCM2838AonIntrState *s = BCM2838_AON_INTR(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &bcm2838_aon_intr_ops, s,
                          TYPE_BCM2838_AON_INTR, AON_INTR_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    for (unsigned int i = 0; i < BCM2838_AON_INTR_OUTPUTS; i++) {
        sysbus_init_irq(sbd, &s->irq[i]);
    }
    qdev_init_gpio_in(DEVICE(obj), bcm2838_aon_intr_set,
                      BCM2838_AON_INTR_LINES);
}

static void bcm2838_aon_intr_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, bcm2838_aon_intr_reset);
    dc->vmsd = &vmstate_bcm2838_aon_intr;
}

static const TypeInfo bcm2838_aon_intr_info = {
    .name = TYPE_BCM2838_AON_INTR,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2838AonIntrState),
    .instance_init = bcm2838_aon_intr_init,
    .class_init = bcm2838_aon_intr_class_init,
};

static void bcm2838_aon_intr_register_types(void)
{
    type_register_static(&bcm2838_aon_intr_info);
}

type_init(bcm2838_aon_intr_register_types)
