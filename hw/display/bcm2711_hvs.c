/*
 * BCM2711 Hardware Video Scaler
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/display/bcm2711_hvs.h"
#include "hw/core/irq.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define SCALER_DISPCTRL             0x0000
#define SCALER_DISPSTAT             0x0004
#define SCALER_DISPID               0x0008
#define SCALER_DISPLIST0            0x0020
#define SCALER_DISPLACT0            0x0030
#define SCALER_DISPCTRL0            0x0040
#define SCALER_DISPSTAT0            0x0048
#define SCALER_CHANNEL_STRIDE        0x0010
#define SCALER5_DLIST_START          0x4000

#define SCALER_DISPCTRLX_ENABLE      BIT(31)
#define SCALER_DISPCTRLX_RESET       BIT(30)
#define SCALER_DISPSTATX_MODE_RUN    (2U << 30)
#define SCALER_DISPSTATX_EMPTY       BIT(28)

#define SCALER_CTL0_END              BIT(31)
#define SCALER_CTL0_VALID            BIT(30)
#define SCALER_CTL0_SIZE_SHIFT       24
#define SCALER_CTL0_SIZE_MASK        0x3f
#define SCALER_CTL0_TILING_SHIFT     20
#define SCALER_CTL0_TILING_MASK      0x3
#define SCALER_CTL0_ORDER_SHIFT      13
#define SCALER_CTL0_ORDER_MASK       0x3
#define SCALER5_CTL0_UNITY           BIT(15)
#define SCALER5_CTL0_PIXEL_FORMAT_MASK 0x1f

#define SCALER5_POS0_START_Y_SHIFT   16
#define SCALER5_POS0_START_Y_MASK    0xfff
#define SCALER5_POS0_START_X_MASK    0x3fff
#define SCALER5_POS2_HEIGHT_SHIFT    16
#define SCALER5_POS2_HEIGHT_MASK     0x1fff
#define SCALER5_POS2_WIDTH_MASK      0x1fff

#define HVS_PIXEL_FORMAT_RGB565      4
#define HVS_PIXEL_FORMAT_RGB888      5
#define HVS_PIXEL_FORMAT_RGBA8888    7
#define HVS_PIXEL_ORDER_XRGB         2
#define HVS_PIXEL_ORDER_XBGR         3
#define HVS_PIXEL_ORDER_ARGB         2
#define HVS_PIXEL_ORDER_ABGR         3

#define HVS_CHANNELS                 3
#define HVS_DLIST_WORDS              4096
#define HVS_MAX_XRES                 3840
#define HVS_MAX_YRES                 2560

static unsigned int bcm2711_hvs_channel_from_offset(hwaddr offset,
                                                     hwaddr base)
{
    return (offset - base) / SCALER_CHANNEL_STRIDE;
}

static bool bcm2711_hvs_decode_format(uint32_t ctl, uint32_t *bpp,
                                      uint32_t *pixo)
{
    uint32_t format = ctl & SCALER5_CTL0_PIXEL_FORMAT_MASK;
    uint32_t order = (ctl >> SCALER_CTL0_ORDER_SHIFT) &
                     SCALER_CTL0_ORDER_MASK;

    switch (format) {
    case HVS_PIXEL_FORMAT_RGB565:
        *bpp = 16;
        if (order == HVS_PIXEL_ORDER_XRGB) {
            *pixo = 1;
            return true;
        }
        if (order == HVS_PIXEL_ORDER_XBGR) {
            *pixo = 0;
            return true;
        }
        return false;
    case HVS_PIXEL_FORMAT_RGB888:
        *bpp = 24;
        if (order == HVS_PIXEL_ORDER_XRGB) {
            *pixo = 1;
            return true;
        }
        if (order == HVS_PIXEL_ORDER_XBGR) {
            *pixo = 0;
            return true;
        }
        return false;
    case HVS_PIXEL_FORMAT_RGBA8888:
        *bpp = 32;
        if (order == HVS_PIXEL_ORDER_ARGB) {
            *pixo = 0;
            return true;
        }
        if (order == HVS_PIXEL_ORDER_ABGR) {
            *pixo = 1;
            return true;
        }
        return false;
    default:
        return false;
    }
}

static bool bcm2711_hvs_apply_scanout(BCM2711HVSState *s,
                                      unsigned int channel)
{
    BCM2835FBConfig config = { 0 };
    uint32_t channel_ctl;
    uint32_t dlist;
    uint32_t ctl;
    uint32_t size;
    uint32_t pos0;
    uint32_t pos2;
    uint32_t ptr_index;
    uint32_t pitch_index;
    uint32_t pitch;
    uint32_t bytes_per_pixel;
    uint32_t source_width;
    uint32_t source_height;
    uint32_t next_ctl;

    g_assert(channel < HVS_CHANNELS);

    channel_ctl = s->regs[(SCALER_DISPCTRL0 +
                           channel * SCALER_CHANNEL_STRIDE) >> 2];
    if (!(channel_ctl & SCALER_DISPCTRLX_ENABLE)) {
        return false;
    }

    config.xres = (channel_ctl >> 16) & 0x1fff;
    config.yres = channel_ctl & 0x1fff;
    if (!config.xres || !config.yres || config.xres > HVS_MAX_XRES ||
        config.yres > HVS_MAX_YRES) {
        return false;
    }

    dlist = s->regs[(SCALER_DISPLIST0 >> 2) + channel] &
            (HVS_DLIST_WORDS - 1);
    ctl = s->regs[(SCALER5_DLIST_START >> 2) + dlist];
    if (!(ctl & SCALER_CTL0_VALID) || (ctl & SCALER_CTL0_END) ||
        ((ctl >> SCALER_CTL0_TILING_SHIFT) & SCALER_CTL0_TILING_MASK) ||
        !(ctl & SCALER5_CTL0_UNITY)) {
        return false;
    }

    size = (ctl >> SCALER_CTL0_SIZE_SHIFT) & SCALER_CTL0_SIZE_MASK;
    if (size < 8 || dlist + size >= HVS_DLIST_WORDS) {
        return false;
    }

    pos0 = s->regs[(SCALER5_DLIST_START >> 2) + dlist + 1];
    pos2 = s->regs[(SCALER5_DLIST_START >> 2) + dlist + 3];
    if ((pos0 & SCALER5_POS0_START_X_MASK) ||
        ((pos0 >> SCALER5_POS0_START_Y_SHIFT) &
         SCALER5_POS0_START_Y_MASK)) {
        return false;
    }

    source_width = pos2 & SCALER5_POS2_WIDTH_MASK;
    source_height = (pos2 >> SCALER5_POS2_HEIGHT_SHIFT) &
                    SCALER5_POS2_HEIGHT_MASK;
    if (source_width != config.xres || source_height != config.yres) {
        return false;
    }

    if (!bcm2711_hvs_decode_format(ctl, &config.bpp, &config.pixo)) {
        return false;
    }

    ptr_index = dlist + 5;
    pitch_index = dlist + 7;
    if (pitch_index >= dlist + size) {
        return false;
    }

    bytes_per_pixel = config.bpp >> 3;
    pitch = s->regs[(SCALER5_DLIST_START >> 2) + pitch_index] & 0xffff;
    if (!pitch || pitch % bytes_per_pixel ||
        pitch < config.xres * bytes_per_pixel ||
        pitch / bytes_per_pixel > HVS_MAX_XRES) {
        return false;
    }

    next_ctl = s->regs[(SCALER5_DLIST_START >> 2) + dlist + size];
    if (!(next_ctl & SCALER_CTL0_END)) {
        qemu_log_mask(LOG_UNIMP,
                      "%s: multi-plane HVS composition is not implemented\n",
                      TYPE_BCM2711_HVS);
    }

    config.xres_virtual = pitch / bytes_per_pixel;
    config.yres_virtual = config.yres;
    config.xoffset = 0;
    config.yoffset = 0;
    config.base = s->regs[(SCALER5_DLIST_START >> 2) + ptr_index];
    config.alpha = 0;

    bcm2835_fb_validate_config(&config);
    bcm2835_fb_reconfigure(s->fb, &config);
    return true;
}

static void bcm2711_hvs_update_channel(BCM2711HVSState *s,
                                       unsigned int channel)
{
    uint32_t ctl_index = (SCALER_DISPCTRL0 +
                          channel * SCALER_CHANNEL_STRIDE) >> 2;
    uint32_t stat_index = (SCALER_DISPSTAT0 +
                           channel * SCALER_CHANNEL_STRIDE) >> 2;

    if (s->regs[ctl_index] & SCALER_DISPCTRLX_ENABLE) {
        s->regs[stat_index] = SCALER_DISPSTATX_MODE_RUN;
        bcm2711_hvs_apply_scanout(s, channel);
    } else {
        s->regs[stat_index] = SCALER_DISPSTATX_EMPTY;
    }
}

static uint64_t bcm2711_hvs_read(void *opaque, hwaddr offset,
                                 unsigned int size)
{
    BCM2711HVSState *s = opaque;

    return s->regs[offset >> 2];
}

static void bcm2711_hvs_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned int size)
{
    BCM2711HVSState *s = opaque;
    uint32_t index = offset >> 2;
    unsigned int channel;

    switch (offset) {
    case SCALER_DISPSTAT:
        s->regs[index] &= ~(uint32_t)value;
        qemu_set_irq(s->irq, false);
        return;
    case SCALER_DISPLACT0:
    case SCALER_DISPLACT0 + 4:
    case SCALER_DISPLACT0 + 8:
        return;
    default:
        s->regs[index] = value;
        break;
    }

    if (offset >= SCALER_DISPLIST0 && offset < SCALER_DISPLIST0 + 12) {
        channel = (offset - SCALER_DISPLIST0) >> 2;
        s->regs[(SCALER_DISPLACT0 >> 2) + channel] =
            s->regs[index] & (HVS_DLIST_WORDS - 1);
        bcm2711_hvs_update_channel(s, channel);
    } else if (offset >= SCALER_DISPCTRL0 &&
               offset <= SCALER_DISPCTRL0 +
                         (HVS_CHANNELS - 1) * SCALER_CHANNEL_STRIDE &&
               !((offset - SCALER_DISPCTRL0) % SCALER_CHANNEL_STRIDE)) {
        channel = bcm2711_hvs_channel_from_offset(offset,
                                                  SCALER_DISPCTRL0);
        if (value & SCALER_DISPCTRLX_RESET) {
            s->regs[index] = 0;
        }
        bcm2711_hvs_update_channel(s, channel);
    }
}

static const MemoryRegionOps bcm2711_hvs_ops = {
    .read = bcm2711_hvs_read,
    .write = bcm2711_hvs_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void bcm2711_hvs_reset(DeviceState *dev)
{
    BCM2711HVSState *s = BCM2711_HVS(dev);

    memset(s->regs, 0, sizeof(s->regs));
    s->regs[SCALER_DISPID >> 2] = 0x64647276;
    for (unsigned int channel = 0; channel < HVS_CHANNELS; channel++) {
        s->regs[(SCALER_DISPSTAT0 +
                 channel * SCALER_CHANNEL_STRIDE) >> 2] =
            SCALER_DISPSTATX_EMPTY;
    }
    qemu_set_irq(s->irq, false);
}

static int bcm2711_hvs_post_load(void *opaque, int version_id)
{
    BCM2711HVSState *s = opaque;

    qemu_set_irq(s->irq, false);
    for (unsigned int channel = 0; channel < HVS_CHANNELS; channel++) {
        bcm2711_hvs_update_channel(s, channel);
    }
    return 0;
}

static const VMStateDescription vmstate_bcm2711_hvs = {
    .name = TYPE_BCM2711_HVS,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2711_hvs_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, BCM2711HVSState, BCM2711_HVS_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2711_hvs_realize(DeviceState *dev, Error **errp)
{
    BCM2711HVSState *s = BCM2711_HVS(dev);
    Object *fb;

    fb = object_property_get_link(OBJECT(dev), "fb", errp);
    if (!fb) {
        return;
    }
    s->fb = BCM2835_FB(fb);
}

static void bcm2711_hvs_init(Object *obj)
{
    BCM2711HVSState *s = BCM2711_HVS(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &bcm2711_hvs_ops, s,
                          TYPE_BCM2711_HVS, BCM2711_HVS_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void bcm2711_hvs_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = bcm2711_hvs_realize;
    device_class_set_legacy_reset(dc, bcm2711_hvs_reset);
    dc->vmsd = &vmstate_bcm2711_hvs;
    dc->desc = "BCM2711 Hardware Video Scaler";
}

static const TypeInfo bcm2711_hvs_info = {
    .name = TYPE_BCM2711_HVS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2711HVSState),
    .instance_init = bcm2711_hvs_init,
    .class_init = bcm2711_hvs_class_init,
};

static void bcm2711_hvs_register_types(void)
{
    type_register_static(&bcm2711_hvs_info);
}

type_init(bcm2711_hvs_register_types)
