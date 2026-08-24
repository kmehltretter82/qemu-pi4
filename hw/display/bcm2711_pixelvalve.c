/*
 * BCM2711 Pixel Valve
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/display/bcm2711_pixelvalve.h"
#include "hw/core/irq.h"
#include "migration/vmstate.h"
#include "qemu/module.h"

#define PV_CONTROL              0x00
#define PV_V_CONTROL            0x04
#define PV_INTEN                0x24
#define PV_INTSTAT              0x28

#define PV_CONTROL_FIFO_CLR     BIT(1)
#define PV_CONTROL_EN           BIT(0)
#define PV_VCONTROL_VIDEN       BIT(0)
#define PV_INT_VFP_START        BIT(7)
#define PV_INT_MASK             0x3ff

#define PV_CONTROL_IDLE         0x00048000
#define PV_V_CONTROL_IDLE       0x01000000
#define PV_FRAME_PERIOD_NS      16666667

static bool bcm2711_pixelvalve_running(BCM2711PixelValveState *s)
{
    return (s->regs[PV_CONTROL >> 2] & PV_CONTROL_EN) &&
           (s->regs[PV_V_CONTROL >> 2] & PV_VCONTROL_VIDEN);
}

static void bcm2711_pixelvalve_update_irq(BCM2711PixelValveState *s)
{
    qemu_set_irq(s->irq, !!(s->regs[PV_INTEN >> 2] &
                            s->regs[PV_INTSTAT >> 2] & PV_INT_MASK));
}

static void bcm2711_pixelvalve_schedule(BCM2711PixelValveState *s)
{
    if (bcm2711_pixelvalve_running(s) &&
        !(s->regs[PV_INTSTAT >> 2] & PV_INT_VFP_START)) {
        if (!timer_pending(s->vblank_timer)) {
            timer_mod(s->vblank_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                      PV_FRAME_PERIOD_NS);
        }
    } else {
        timer_del(s->vblank_timer);
    }
}

static void bcm2711_pixelvalve_vblank(void *opaque)
{
    BCM2711PixelValveState *s = opaque;

    if (!bcm2711_pixelvalve_running(s)) {
        return;
    }

    s->regs[PV_INTSTAT >> 2] |= PV_INT_VFP_START;
    bcm2711_pixelvalve_update_irq(s);
}

static uint64_t bcm2711_pixelvalve_read(void *opaque, hwaddr offset,
                                        unsigned int size)
{
    BCM2711PixelValveState *s = opaque;

    return s->regs[offset >> 2];
}

static void bcm2711_pixelvalve_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned int size)
{
    BCM2711PixelValveState *s = opaque;
    uint32_t index = offset >> 2;

    switch (offset) {
    case PV_CONTROL:
        s->regs[index] = value & ~PV_CONTROL_FIFO_CLR;
        break;
    case PV_INTEN:
        s->regs[index] = value & PV_INT_MASK;
        break;
    case PV_INTSTAT:
        s->regs[index] &= ~((uint32_t)value & PV_INT_MASK);
        break;
    default:
        s->regs[index] = value;
        break;
    }

    bcm2711_pixelvalve_update_irq(s);
    bcm2711_pixelvalve_schedule(s);
}

static const MemoryRegionOps bcm2711_pixelvalve_ops = {
    .read = bcm2711_pixelvalve_read,
    .write = bcm2711_pixelvalve_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void bcm2711_pixelvalve_reset(DeviceState *dev)
{
    BCM2711PixelValveState *s = BCM2711_PIXELVALVE(dev);

    memset(s->regs, 0, sizeof(s->regs));
    s->regs[PV_CONTROL >> 2] = PV_CONTROL_IDLE;
    s->regs[PV_V_CONTROL >> 2] = PV_V_CONTROL_IDLE;
    timer_del(s->vblank_timer);
    bcm2711_pixelvalve_update_irq(s);
}

static int bcm2711_pixelvalve_post_load(void *opaque, int version_id)
{
    BCM2711PixelValveState *s = opaque;

    s->regs[PV_INTEN >> 2] &= PV_INT_MASK;
    s->regs[PV_INTSTAT >> 2] &= PV_INT_MASK;
    bcm2711_pixelvalve_update_irq(s);
    if (!timer_pending(s->vblank_timer)) {
        bcm2711_pixelvalve_schedule(s);
    }
    return 0;
}

static const VMStateDescription vmstate_bcm2711_pixelvalve = {
    .name = TYPE_BCM2711_PIXELVALVE,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2711_pixelvalve_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, BCM2711PixelValveState,
                             BCM2711_PIXELVALVE_REGS),
        VMSTATE_TIMER_PTR(vblank_timer, BCM2711PixelValveState),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2711_pixelvalve_realize(DeviceState *dev, Error **errp)
{
    BCM2711PixelValveState *s = BCM2711_PIXELVALVE(dev);

    s->vblank_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                   bcm2711_pixelvalve_vblank, s);
}

static void bcm2711_pixelvalve_unrealize(DeviceState *dev)
{
    BCM2711PixelValveState *s = BCM2711_PIXELVALVE(dev);

    timer_free(s->vblank_timer);
    s->vblank_timer = NULL;
}

static void bcm2711_pixelvalve_init(Object *obj)
{
    BCM2711PixelValveState *s = BCM2711_PIXELVALVE(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &bcm2711_pixelvalve_ops, s,
                          TYPE_BCM2711_PIXELVALVE,
                          BCM2711_PIXELVALVE_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void bcm2711_pixelvalve_class_init(ObjectClass *klass,
                                          const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = bcm2711_pixelvalve_realize;
    dc->unrealize = bcm2711_pixelvalve_unrealize;
    device_class_set_legacy_reset(dc, bcm2711_pixelvalve_reset);
    dc->vmsd = &vmstate_bcm2711_pixelvalve;
    dc->desc = "BCM2711 Pixel Valve";
}

static const TypeInfo bcm2711_pixelvalve_info = {
    .name = TYPE_BCM2711_PIXELVALVE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2711PixelValveState),
    .instance_init = bcm2711_pixelvalve_init,
    .class_init = bcm2711_pixelvalve_class_init,
};

static void bcm2711_pixelvalve_register_types(void)
{
    type_register_static(&bcm2711_pixelvalve_info);
}

type_init(bcm2711_pixelvalve_register_types)
