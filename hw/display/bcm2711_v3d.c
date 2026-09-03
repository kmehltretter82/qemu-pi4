/*
 * BCM2711 V3D 4.2 register block
 *
 * This is deliberately only the non-executing, driver-facing register
 * substrate.  The Raspberry Pi 4 machine keeps the V3D device-tree node
 * disabled until command-list execution is modeled.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/module.h"
#include "hw/display/bcm2711_v3d.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"

/* Hub registers. */
#define V3D_HUB_AXICFG                 0x0000
#define V3D_HUB_UIFCFG                 0x0004
#define V3D_HUB_IDENT0                 0x0008
#define V3D_HUB_IDENT1                 0x000c
#define V3D_HUB_IDENT2                 0x0010
#define V3D_HUB_IDENT3                 0x0014
#define V3D_HUB_INT_STS                0x0050
#define V3D_HUB_INT_SET                0x0054
#define V3D_HUB_INT_CLR                0x0058
#define V3D_HUB_INT_MSK_STS            0x005c
#define V3D_HUB_INT_MSK_SET            0x0060
#define V3D_HUB_INT_MSK_CLR            0x0064

#define V3D_MMUC_CONTROL               0x1000
#define V3D_MMUC_CONTROL_FLUSHING      BIT(2)
#define V3D_MMUC_CONTROL_FLUSH         BIT(1)
#define V3D_MMU_CTL                    0x1200
#define V3D_MMU_CTL_TLB_CLEARING       BIT(7)
#define V3D_MMU_CTL_TLB_CLEAR          BIT(2)
#define V3D_MMU_DEBUG_INFO             0x1238

/* Core0 registers. */
#define V3D_CTL_IDENT0                 0x0000
#define V3D_CTL_IDENT1                 0x0004
#define V3D_CTL_IDENT2                 0x0008
#define V3D_CTL_MISCCFG                0x0018
#define V3D_CTL_L2TCACTL               0x0030
#define V3D_CTL_INT_STS                0x0050
#define V3D_CTL_INT_SET                0x0054
#define V3D_CTL_INT_CLR                0x0058
#define V3D_CTL_INT_MSK_STS            0x005c
#define V3D_CTL_INT_MSK_SET            0x0060
#define V3D_CTL_INT_MSK_CLR            0x0064

#define V3D_L2TCACTL_TMUWCF            BIT(8)
#define V3D_L2TCACTL_L2TFLS            BIT(0)

/* Read-only Pi 400 values captured through the Linux V3D driver's debugfs. */
#define BCM2711_V3D_HUB_AXICFG         0x0000000f
#define BCM2711_V3D_HUB_UIFCFG         0x00000045
#define BCM2711_V3D_HUB_IDENT0         0x42554856
#define BCM2711_V3D_HUB_IDENT1         0x000e1124
#define BCM2711_V3D_HUB_IDENT2         0x00000100
#define BCM2711_V3D_HUB_IDENT3         0x00000e00
#define BCM2711_V3D_MMU_DEBUG_INFO     0x00000550
#define BCM2711_V3D_CORE_IDENT0        0x04443356
#define BCM2711_V3D_CORE_IDENT1        0x81001422
#define BCM2711_V3D_CORE_IDENT2        0x40078121
#define BCM2711_V3D_CORE_MISCCFG       0x00000006

static void bcm2711_v3d_update_irq(BCM2711V3DState *s)
{
    bool pending;

    pending = (s->hub_regs[V3D_HUB_INT_STS >> 2] &
               ~s->hub_regs[V3D_HUB_INT_MSK_STS >> 2]) ||
              (s->core0_regs[V3D_CTL_INT_STS >> 2] &
               ~s->core0_regs[V3D_CTL_INT_MSK_STS >> 2]);
    qemu_set_irq(s->irq, pending);
}

static uint64_t bcm2711_v3d_hub_read(void *opaque, hwaddr offset,
                                     unsigned size)
{
    BCM2711V3DState *s = opaque;

    return s->hub_regs[offset >> 2];
}

static void bcm2711_v3d_hub_write(void *opaque, hwaddr offset,
                                  uint64_t value, unsigned size)
{
    BCM2711V3DState *s = opaque;
    uint32_t *reg = &s->hub_regs[offset >> 2];

    switch (offset) {
    case V3D_HUB_IDENT0:
    case V3D_HUB_IDENT1:
    case V3D_HUB_IDENT2:
    case V3D_HUB_IDENT3:
    case V3D_HUB_INT_STS:
    case V3D_HUB_INT_MSK_STS:
    case V3D_MMU_DEBUG_INFO:
        break;
    case V3D_HUB_INT_SET:
        s->hub_regs[V3D_HUB_INT_STS >> 2] |= value;
        break;
    case V3D_HUB_INT_CLR:
        s->hub_regs[V3D_HUB_INT_STS >> 2] &= ~value;
        break;
    case V3D_HUB_INT_MSK_SET:
        s->hub_regs[V3D_HUB_INT_MSK_STS >> 2] |= value;
        break;
    case V3D_HUB_INT_MSK_CLR:
        s->hub_regs[V3D_HUB_INT_MSK_STS >> 2] &= ~value;
        break;
    case V3D_MMUC_CONTROL:
        /* Flushes complete immediately while no GPU work can be in flight. */
        *reg = value & ~(V3D_MMUC_CONTROL_FLUSH |
                         V3D_MMUC_CONTROL_FLUSHING);
        break;
    case V3D_MMU_CTL:
        /* The kernel polls these as transient request/status bits. */
        *reg = value & ~(V3D_MMU_CTL_TLB_CLEAR |
                         V3D_MMU_CTL_TLB_CLEARING);
        break;
    default:
        *reg = value;
        break;
    }

    bcm2711_v3d_update_irq(s);
}

static uint64_t bcm2711_v3d_core0_read(void *opaque, hwaddr offset,
                                       unsigned size)
{
    BCM2711V3DState *s = opaque;

    return s->core0_regs[offset >> 2];
}

static void bcm2711_v3d_core0_write(void *opaque, hwaddr offset,
                                    uint64_t value, unsigned size)
{
    BCM2711V3DState *s = opaque;
    uint32_t *reg = &s->core0_regs[offset >> 2];

    switch (offset) {
    case V3D_CTL_IDENT0:
    case V3D_CTL_IDENT1:
    case V3D_CTL_IDENT2:
    case V3D_CTL_INT_STS:
    case V3D_CTL_INT_MSK_STS:
        break;
    case V3D_CTL_INT_SET:
        s->core0_regs[V3D_CTL_INT_STS >> 2] |= value;
        break;
    case V3D_CTL_INT_CLR:
        s->core0_regs[V3D_CTL_INT_STS >> 2] &= ~value;
        break;
    case V3D_CTL_INT_MSK_SET:
        s->core0_regs[V3D_CTL_INT_MSK_STS >> 2] |= value;
        break;
    case V3D_CTL_INT_MSK_CLR:
        s->core0_regs[V3D_CTL_INT_MSK_STS >> 2] &= ~value;
        break;
    case V3D_CTL_L2TCACTL:
        /* Cache maintenance completes synchronously in this passive model. */
        *reg = value & ~(V3D_L2TCACTL_TMUWCF | V3D_L2TCACTL_L2TFLS);
        break;
    default:
        *reg = value;
        break;
    }

    bcm2711_v3d_update_irq(s);
}

static const MemoryRegionOps bcm2711_v3d_hub_ops = {
    .read = bcm2711_v3d_hub_read,
    .write = bcm2711_v3d_hub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static const MemoryRegionOps bcm2711_v3d_core0_ops = {
    .read = bcm2711_v3d_core0_read,
    .write = bcm2711_v3d_core0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void bcm2711_v3d_reset(DeviceState *dev)
{
    BCM2711V3DState *s = BCM2711_V3D(dev);

    memset(s->hub_regs, 0, sizeof(s->hub_regs));
    memset(s->core0_regs, 0, sizeof(s->core0_regs));

    s->hub_regs[V3D_HUB_AXICFG >> 2] = BCM2711_V3D_HUB_AXICFG;
    s->hub_regs[V3D_HUB_UIFCFG >> 2] = BCM2711_V3D_HUB_UIFCFG;
    s->hub_regs[V3D_HUB_IDENT0 >> 2] = BCM2711_V3D_HUB_IDENT0;
    s->hub_regs[V3D_HUB_IDENT1 >> 2] = BCM2711_V3D_HUB_IDENT1;
    s->hub_regs[V3D_HUB_IDENT2 >> 2] = BCM2711_V3D_HUB_IDENT2;
    s->hub_regs[V3D_HUB_IDENT3 >> 2] = BCM2711_V3D_HUB_IDENT3;
    s->hub_regs[V3D_MMU_DEBUG_INFO >> 2] = BCM2711_V3D_MMU_DEBUG_INFO;

    s->core0_regs[V3D_CTL_IDENT0 >> 2] = BCM2711_V3D_CORE_IDENT0;
    s->core0_regs[V3D_CTL_IDENT1 >> 2] = BCM2711_V3D_CORE_IDENT1;
    s->core0_regs[V3D_CTL_IDENT2 >> 2] = BCM2711_V3D_CORE_IDENT2;
    s->core0_regs[V3D_CTL_MISCCFG >> 2] = BCM2711_V3D_CORE_MISCCFG;

    bcm2711_v3d_update_irq(s);
}

static int bcm2711_v3d_post_load(void *opaque, int version_id)
{
    bcm2711_v3d_update_irq(opaque);
    return 0;
}

static const VMStateDescription vmstate_bcm2711_v3d = {
    .name = TYPE_BCM2711_V3D,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2711_v3d_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(hub_regs, BCM2711V3DState, BCM2711_V3D_REGS),
        VMSTATE_UINT32_ARRAY(core0_regs, BCM2711V3DState,
                             BCM2711_V3D_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2711_v3d_init(Object *obj)
{
    BCM2711V3DState *s = BCM2711_V3D(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->hub_iomem, obj, &bcm2711_v3d_hub_ops, s,
                          TYPE_BCM2711_V3D ".hub", BCM2711_V3D_MMIO_SIZE);
    memory_region_init_io(&s->core0_iomem, obj, &bcm2711_v3d_core0_ops, s,
                          TYPE_BCM2711_V3D ".core0",
                          BCM2711_V3D_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->hub_iomem);
    sysbus_init_mmio(sbd, &s->core0_iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static const Property bcm2711_v3d_properties[] = {
    DEFINE_PROP_BOOL("enable-probe-dtb", BCM2711V3DState, enable_probe_dtb,
                     false),
};

static void bcm2711_v3d_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, bcm2711_v3d_reset);
    device_class_set_props(dc, bcm2711_v3d_properties);
    dc->vmsd = &vmstate_bcm2711_v3d;
    dc->desc = "BCM2711 V3D 4.2 register block";
}

static const TypeInfo bcm2711_v3d_info = {
    .name = TYPE_BCM2711_V3D,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2711V3DState),
    .instance_init = bcm2711_v3d_init,
    .class_init = bcm2711_v3d_class_init,
};

static void bcm2711_v3d_register_types(void)
{
    type_register_static(&bcm2711_v3d_info);
}

type_init(bcm2711_v3d_register_types)
