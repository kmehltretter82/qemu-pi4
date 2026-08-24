/*
 * BCM2711 HDMI DVP clock and reset controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/bcm2711_dvp.h"
#include "hw/core/irq.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define DVP_MMIO_SIZE                 0x10

#define DVP_HT_RPI_CONTROL            0x00
#define DVP_HT_RPI_SW_INIT            0x04
#define DVP_HT_RPI_MISC_CONFIG        0x08
#define DVP_HT_RPI_SPARE              0x0c

#define DVP_HT_RPI_CONTROL_IDLE       0x00000200
#define DVP_HT_RPI_SW_INIT_MASK       MAKE_64BIT_MASK(0, BCM2711_DVP_RESETS)
#define DVP_HT_RPI_MISC_HDMI0_DISABLE BIT(3)
#define DVP_HT_RPI_MISC_HDMI1_DISABLE BIT(4)
#define DVP_HT_RPI_MISC_CONFIG_MASK   (DVP_HT_RPI_MISC_HDMI0_DISABLE | \
                                       DVP_HT_RPI_MISC_HDMI1_DISABLE)
#define DVP_HT_RPI_SPARE_IDLE         0xffff0000

static void bcm2711_dvp_update(BCM2711DVPState *s)
{
    for (unsigned int i = 0; i < BCM2711_DVP_RESETS; i++) {
        qemu_set_irq(s->reset[i], !!(s->sw_init & BIT(i)));
    }

    qemu_set_irq(s->clock_enable[0],
                 !(s->misc_config & DVP_HT_RPI_MISC_HDMI0_DISABLE));
    qemu_set_irq(s->clock_enable[1],
                 !(s->misc_config & DVP_HT_RPI_MISC_HDMI1_DISABLE));
}

static uint64_t bcm2711_dvp_read(void *opaque, hwaddr offset,
                                 unsigned int size)
{
    BCM2711DVPState *s = opaque;

    switch (offset) {
    case DVP_HT_RPI_CONTROL:
        return DVP_HT_RPI_CONTROL_IDLE;
    case DVP_HT_RPI_SW_INIT:
        return s->sw_init;
    case DVP_HT_RPI_MISC_CONFIG:
        return s->misc_config;
    case DVP_HT_RPI_SPARE:
        return DVP_HT_RPI_SPARE_IDLE;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2711_DVP, offset);
        return 0;
    }
}

static void bcm2711_dvp_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned int size)
{
    BCM2711DVPState *s = opaque;

    switch (offset) {
    case DVP_HT_RPI_SW_INIT:
        s->sw_init = value & DVP_HT_RPI_SW_INIT_MASK;
        break;
    case DVP_HT_RPI_MISC_CONFIG:
        s->misc_config = value & DVP_HT_RPI_MISC_CONFIG_MASK;
        break;
    case DVP_HT_RPI_CONTROL:
    case DVP_HT_RPI_SPARE:
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2711_DVP, offset);
        return;
    }

    bcm2711_dvp_update(s);
}

static const MemoryRegionOps bcm2711_dvp_ops = {
    .read = bcm2711_dvp_read,
    .write = bcm2711_dvp_write,
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

static void bcm2711_dvp_reset(DeviceState *dev)
{
    BCM2711DVPState *s = BCM2711_DVP(dev);

    s->sw_init = 0;
    s->misc_config = DVP_HT_RPI_MISC_CONFIG_MASK;
    bcm2711_dvp_update(s);
}

static int bcm2711_dvp_post_load(void *opaque, int version_id)
{
    BCM2711DVPState *s = BCM2711_DVP(opaque);

    if ((s->sw_init & ~DVP_HT_RPI_SW_INIT_MASK) ||
        (s->misc_config & ~DVP_HT_RPI_MISC_CONFIG_MASK)) {
        return -EINVAL;
    }

    bcm2711_dvp_update(s);
    return 0;
}

static const VMStateDescription vmstate_bcm2711_dvp = {
    .name = TYPE_BCM2711_DVP,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2711_dvp_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(sw_init, BCM2711DVPState),
        VMSTATE_UINT32(misc_config, BCM2711DVPState),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2711_dvp_init(Object *obj)
{
    BCM2711DVPState *s = BCM2711_DVP(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &bcm2711_dvp_ops, s,
                          TYPE_BCM2711_DVP, DVP_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    qdev_init_gpio_out_named(DEVICE(obj), s->reset, "reset",
                             BCM2711_DVP_RESETS);
    qdev_init_gpio_out_named(DEVICE(obj), s->clock_enable, "clock-enable",
                             BCM2711_DVP_CLOCKS);
}

static void bcm2711_dvp_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, bcm2711_dvp_reset);
    dc->vmsd = &vmstate_bcm2711_dvp;
}

static const TypeInfo bcm2711_dvp_info = {
    .name = TYPE_BCM2711_DVP,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2711DVPState),
    .instance_init = bcm2711_dvp_init,
    .class_init = bcm2711_dvp_class_init,
};

static void bcm2711_dvp_register_types(void)
{
    type_register_static(&bcm2711_dvp_info);
}

type_init(bcm2711_dvp_register_types)
