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
#define SCALER_CTL0_SCL_MASK         0x7
#define SCALER5_CTL0_UNITY           BIT(15)
#define SCALER5_CTL0_PIXEL_FORMAT_MASK 0x1f

#define SCALER5_POS0_START_Y_MASK    0xfff
#define SCALER5_POS0_START_X_MASK    0x3fff
#define SCALER5_POS0_VFLIP           BIT(31)
#define SCALER5_POS0_HFLIP           BIT(15)
#define SCALER5_POS2_HEIGHT_MASK     0x1fff
#define SCALER5_POS2_WIDTH_MASK      0x1fff

#define SCALER5_CTL2_ALPHA_PREMULT    BIT(29)
#define SCALER5_CTL2_ALPHA_MIX        BIT(28)

#define HVS_CHANNELS                 3
#define HVS_DLIST_WORDS              4096
#define HVS_MAX_XRES                 3840
#define HVS_MAX_YRES                 2560

static unsigned int bcm2711_hvs_channel_from_offset(hwaddr offset,
                                                     hwaddr base)
{
    return (offset - base) / SCALER_CHANNEL_STRIDE;
}

static void bcm2711_hvs_update_irq(BCM2711HVSState *s)
{
    uint32_t control = s->regs[SCALER_DISPCTRL >> 2];
    uint32_t *status = &s->regs[SCALER_DISPSTAT >> 2];
    bool pending = false;

    *status &= ~0xeU;
    for (unsigned int c = 0; c < HVS_CHANNELS; c++) {
        uint32_t events = (*status >> (8 + 8 * c)) & 0x3f;
        bool active = (events & BIT(3)) || /* short frame */
            ((control & BIT(7 + 4 * c)) && (events & BIT(0))) ||
            ((control & BIT(8 + 4 * c)) && (events & BIT(4))) ||
            ((control & BIT(9 + 4 * c)) && (events & (BIT(1) | BIT(2))));

        if (active) {
            *status |= BIT(1 + c);
            pending |= !!(control & BIT(1 + c));
        }
    }
    qemu_set_irq(s->irq, pending);
}

static unsigned int bcm2711_hvs_output_channel(BCM2711HVSState *s,
                                               unsigned int output)
{
    /* Output 4 (PV2/HDMI0) is in DISPEOLN, output 5 in DISPDITHER. */
    return s->regs[(output ? 0x14 : 0x18) >> 2] >> 30;
}

static bool bcm2711_hvs_decode_format(uint32_t ctl,
                                      BCM2835FBHVSLayer *layer)
{
    layer->format = ctl & SCALER5_CTL0_PIXEL_FORMAT_MASK;
    layer->order = (ctl >> SCALER_CTL0_ORDER_SHIFT) & SCALER_CTL0_ORDER_MASK;
    switch (layer->format) {
    case 0: /* RGB332 */
        layer->bpp = 8;
        break;
    case 3: /* RGBA5551 */
    case 4: /* RGB565 */
        layer->bpp = 16;
        break;
    case 5: /* RGB888 */
        layer->bpp = 24;
        break;
    case 7: /* RGBA8888 */
    case 16: /* RGBA1010102 */
        layer->bpp = 32;
        break;
    case 8: /* planar YUV420 */
    case 9: /* interleaved YUV420 */
    case 10: /* planar YUV422 (also used by Linux for YUV444) */
    case 11: /* interleaved YUV422 */
        layer->bpp = 8;
        return layer->order <= 1;
    default:
        return false;
    }
    return layer->order == 2 || layer->order == 3;
}

static void bcm2711_hvs_decode_scaling(uint32_t scl0,
                                       BCM2835FBHVSScaleMode *x_mode,
                                       BCM2835FBHVSScaleMode *y_mode)
{
    *x_mode = BCM2835_FB_HVS_SCALE_NONE;
    *y_mode = BCM2835_FB_HVS_SCALE_NONE;

    switch (scl0) {
    case 0: /* Horizontal PPF, vertical PPF. */
        *x_mode = BCM2835_FB_HVS_SCALE_PPF;
        *y_mode = BCM2835_FB_HVS_SCALE_PPF;
        break;
    case 1: /* Horizontal TPZ, vertical PPF. */
        *x_mode = BCM2835_FB_HVS_SCALE_TPZ;
        *y_mode = BCM2835_FB_HVS_SCALE_PPF;
        break;
    case 2: /* Horizontal PPF, vertical TPZ. */
        *x_mode = BCM2835_FB_HVS_SCALE_PPF;
        *y_mode = BCM2835_FB_HVS_SCALE_TPZ;
        break;
    case 3: /* Horizontal TPZ, vertical TPZ. */
        *x_mode = BCM2835_FB_HVS_SCALE_TPZ;
        *y_mode = BCM2835_FB_HVS_SCALE_TPZ;
        break;
    case 4: /* Horizontal PPF, vertical unity. */
        *x_mode = BCM2835_FB_HVS_SCALE_PPF;
        break;
    case 5: /* Horizontal unity, vertical PPF. */
        *y_mode = BCM2835_FB_HVS_SCALE_PPF;
        break;
    case 6: /* Horizontal unity, vertical TPZ. */
        *y_mode = BCM2835_FB_HVS_SCALE_TPZ;
        break;
    case 7: /* Horizontal TPZ, vertical unity. */
        *x_mode = BCM2835_FB_HVS_SCALE_TPZ;
        break;
    }
}

static bool bcm2711_hvs_decode_filters(BCM2711HVSState *s,
                                       BCM2835FBHVSLayer *layer,
                                       uint32_t ctl, uint32_t at, uint32_t end,
                                       bool yuv)
{
    const uint32_t *ram = s->regs + (SCALER5_DLIST_START >> 2);
    unsigned int channels = yuv ? 2 : 1;
    uint32_t required = 0;
    bool ppf = false;
    bool vertical = false;

    for (unsigned int c = 0; c < channels; c++) {
        BCM2835FBHVSScaleMode x, y;
        uint32_t scl = (ctl >> (c ? 8 : 5)) & SCALER_CTL0_SCL_MASK;

        if ((ctl & SCALER5_CTL0_UNITY) && (!yuv || c == 1)) {
            continue;
        }
        bcm2711_hvs_decode_scaling(scl, &x, &y);
        layer->scale[c][0].mode = x;
        layer->scale[c][1].mode = y;
        vertical |= y != BCM2835_FB_HVS_SCALE_NONE;
        ppf |= x == BCM2835_FB_HVS_SCALE_PPF || y == BCM2835_FB_HVS_SCALE_PPF;
        required += x == BCM2835_FB_HVS_SCALE_PPF ? 1 :
                    x == BCM2835_FB_HVS_SCALE_TPZ ? 2 : 0;
        required += y == BCM2835_FB_HVS_SCALE_PPF ? 2 :
                    y == BCM2835_FB_HVS_SCALE_TPZ ? 3 : 0;
    }
    required += vertical + (ppf ? 4 : 0);
    if (at + required > end) {
        /* Retain the fork's old short RGB lists as a functional fallback. */
        memset(layer->scale, 0, sizeof(layer->scale));
        return !yuv;
    }
    at += vertical; /* LBM allocation: not needed by the software renderer. */
    for (unsigned int c = 0; c < channels; c++) {
        /*
         * Linux emits horizontal PPF, vertical PPF, horizontal TPZ,
         * vertical TPZ in that order, including the context words.
         */
        for (unsigned int mode = 1; mode <= 2; mode++) {
            for (unsigned int axis = 0; axis < 2; axis++) {
                BCM2835FBHVSScale *sc = &layer->scale[c][axis];

                if (sc->mode != mode) {
                    continue;
                }
                sc->param = ram[at++];
                if (mode == BCM2835_FB_HVS_SCALE_TPZ) {
                    sc->reciprocal = ram[at++];
                }
                at += axis; /* vertical context */
            }
        }
    }
    if (ppf) {
        for (unsigned int c = 0; c < channels; c++) {
            for (unsigned int axis = 0; axis < 2; axis++) {
                BCM2835FBHVSScale *sc = &layer->scale[c][axis];
                uint32_t ptr = ram[at + c * 2 + axis] & 0x3fff;

                if (sc->mode != BCM2835_FB_HVS_SCALE_PPF) {
                    continue;
                }
                if (ptr + 6 > HVS_DLIST_WORDS) {
                    return false;
                }
                for (unsigned int i = 0; i < 16; i++) {
                    int16_t coefficient = sextract32(ram[ptr + i / 3],
                                                     9 * (i % 3), 9);
                    sc->kernel[i] = coefficient;
                    sc->kernel[31 - i] = coefficient;
                }
            }
        }
    }
    return true;
}

static bool bcm2711_hvs_apply_scanout(BCM2711HVSState *s,
                                      unsigned int channel, bool completed)
{
    BCM2835FBHVSLayer layers[BCM2835_FB_MAX_HVS_LAYERS] = { 0 };
    uint32_t ctl = s->regs[(SCALER_DISPCTRL0 +
                           channel * SCALER_CHANNEL_STRIDE) >> 2];
    uint32_t width = (ctl >> 16) & 0x1fff;
    uint32_t height = ctl & 0x1fff;
    uint32_t dlist = s->regs[(SCALER_DISPLIST0 >> 2) + channel] & 0xfff;
    const uint32_t *ram = s->regs + (SCALER5_DLIST_START >> 2);
    uint32_t count = 0;

    if ((!(ctl & SCALER_DISPCTRLX_ENABLE) && !completed) || !width || !height ||
        width > HVS_MAX_XRES || height > HVS_MAX_YRES) {
        return false;
    }
    while (dlist < HVS_DLIST_WORDS && !(ram[dlist] & SCALER_CTL0_END)) {
        BCM2835FBHVSLayer *layer;
        uint32_t ctl0 = ram[dlist];
        uint32_t size = (ctl0 >> SCALER_CTL0_SIZE_SHIFT) &
                        SCALER_CTL0_SIZE_MASK;
        uint32_t at = dlist + 3;
        uint32_t end = dlist + size;
        uint32_t pos0, ctl2, pos1 = 0, pos2, tiling, planes;
        bool yuv;

        if (count == BCM2835_FB_MAX_HVS_LAYERS ||
            !(ctl0 & SCALER_CTL0_VALID) || size < 8 ||
            end >= HVS_DLIST_WORDS ||
            !bcm2711_hvs_decode_format(ctl0, &layers[count])) {
            return false;
        }
        layer = &layers[count];
        yuv = layer->format >= 8 && layer->format <= 11;
        planes = !yuv ? 1 : (layer->format == 8 ||
                             layer->format == 10) ? 3 : 2;
        tiling = (ctl0 >> SCALER_CTL0_TILING_SHIFT) & SCALER_CTL0_TILING_MASK;
        if (tiling != 0 && (tiling != 3 || yuv)) {
            return false;
        }
        layer->column_tiled = tiling == 3;
        pos0 = ram[dlist + 1];
        ctl2 = ram[dlist + 2];
        if (!(ctl0 & SCALER5_CTL0_UNITY)) {
            pos1 = ram[at++];
        }
        if (at + 2 + 3 * planes + (yuv ? 3 : 0) > end) {
            return false;
        }
        pos2 = ram[at];
        at += 2; /* source dimensions and context */
        layer->source_width = pos2 & SCALER5_POS2_WIDTH_MASK;
        layer->source_height = (pos2 >> 16) & SCALER5_POS2_HEIGHT_MASK;
        layer->dest_x = pos0 & SCALER5_POS0_START_X_MASK;
        layer->dest_y = (pos0 >> 16) & SCALER5_POS0_START_Y_MASK;
        layer->hflip = pos0 & SCALER5_POS0_HFLIP;
        layer->vflip = pos0 & SCALER5_POS0_VFLIP;
        layer->dest_width = ctl0 & SCALER5_CTL0_UNITY ?
                            layer->source_width : pos1 & 0x1fff;
        layer->dest_height = ctl0 & SCALER5_CTL0_UNITY ?
                             layer->source_height : (pos1 >> 16) & 0x1fff;
        if (!layer->source_width || !layer->source_height ||
            !layer->dest_width || !layer->dest_height ||
            layer->source_width > HVS_MAX_XRES ||
            layer->dest_width > HVS_MAX_XRES ||
            layer->source_height > HVS_MAX_YRES ||
            layer->dest_height > HVS_MAX_YRES) {
            return false;
        }
        layer->base = ram[at];
        for (unsigned int i = 1; i < planes; i++) {
            layer->chroma_base[i - 1] = ram[at + i];
        }
        at += 2 * planes; /* pointers and pointer contexts */
        layer->pitch = ram[at];
        for (unsigned int i = 1; i < planes; i++) {
            layer->chroma_pitch[i - 1] = ram[at + i] & 0xffff;
        }
        at += planes;
        if (!layer->column_tiled) {
            layer->pitch &= 0xffff;
            if (!layer->pitch) {
                return false;
            }
        }
        if (yuv) {
            memcpy(layer->csc, ram + at, sizeof(layer->csc));
            at += 3;
        }
        if (!bcm2711_hvs_decode_filters(s, layer, ctl0, at, end, yuv)) {
            return false;
        }
        layer->alpha = (ctl2 >> 4) & 0xfff;
        layer->alpha_mode = ctl2 >> 30;
        if (layer->alpha_mode > 1) {
            return false;
        }
        layer->alpha_mix = ctl2 & SCALER5_CTL2_ALPHA_MIX;
        layer->alpha_premult = ctl2 & SCALER5_CTL2_ALPHA_PREMULT;
        count++;
        dlist = end;
    }
    if (dlist == HVS_DLIST_WORDS) {
        return false;
    }
    if (channel == bcm2711_hvs_output_channel(s, 0)) {
        bcm2835_fb_reconfigure_hvs(s->fb, width, height, layers, count);
    }
    if (s->fb1 && channel == bcm2711_hvs_output_channel(s, 1)) {
        bcm2835_fb_reconfigure_hvs(s->fb1, width, height, layers, count);
    }
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
        bcm2711_hvs_apply_scanout(s, channel, false);
    } else {
        s->regs[stat_index] = SCALER_DISPSTATX_EMPTY;
    }
}

static void bcm2711_hvs_vblank(void *opaque, int output, int level)
{
    BCM2711HVSState *s = opaque;
    unsigned int c = bcm2711_hvs_output_channel(s, output);
    uint32_t control;

    if (!level || c >= HVS_CHANNELS) {
        return;
    }
    control = s->regs[(SCALER_DISPCTRL0 + c * SCALER_CHANNEL_STRIDE) >> 2];
    if (!(control & SCALER_DISPCTRLX_ENABLE)) {
        return;
    }
    if (bcm2711_hvs_apply_scanout(s, c, false)) {
        s->regs[SCALER_DISPSTAT >> 2] |= BIT(8 + 8 * c); /* EOF */
    } else {
        s->regs[SCALER_DISPSTAT >> 2] |= BIT(11 + 8 * c); /* short frame */
    }
    if (control & BIT(15)) { /* HVS5 one-shot */
        s->regs[(SCALER_DISPCTRL0 + c * SCALER_CHANNEL_STRIDE) >> 2] &=
            ~SCALER_DISPCTRLX_ENABLE;
        s->regs[(SCALER_DISPSTAT0 + c * SCALER_CHANNEL_STRIDE) >> 2] =
            BIT(30); /* EOF mode */
    }
    bcm2711_hvs_update_irq(s);
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
        if (value & BIT(0)) {
            s->regs[index] = 0;
        } else {
            s->regs[index] &= ~((uint32_t)value & 0x3f3f3f00);
        }
        bcm2711_hvs_update_irq(s);
        return;
    case SCALER_DISPLACT0:
    case SCALER_DISPLACT0 + 4:
    case SCALER_DISPLACT0 + 8:
        return;
    default:
        s->regs[index] = value;
        break;
    }

    if (offset == SCALER_DISPCTRL) {
        bcm2711_hvs_update_irq(s);
    }
    if (offset == 0x14 || offset == 0x18) {
        for (channel = 0; channel < HVS_CHANNELS; channel++) {
            bcm2711_hvs_update_channel(s, channel);
        }
    } else if (offset >= SCALER_DISPLIST0 && offset < SCALER_DISPLIST0 + 12) {
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
    } else if (offset >= SCALER5_DLIST_START &&
               offset < SCALER5_DLIST_START +
                        HVS_DLIST_WORDS * sizeof(uint32_t)) {
        for (channel = 0; channel < HVS_CHANNELS; channel++) {
            if (s->regs[(SCALER_DISPCTRL0 +
                         channel * SCALER_CHANNEL_STRIDE) >> 2] &
                SCALER_DISPCTRLX_ENABLE) {
                bcm2711_hvs_update_channel(s, channel);
            }
        }
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
    s->regs[0x14 >> 2] = 3U << 30; /* HDMI1 disabled until routed */
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

    bcm2711_hvs_update_irq(s);
    for (unsigned int channel = 0; channel < HVS_CHANNELS; channel++) {
        uint32_t control = s->regs[(SCALER_DISPCTRL0 +
                                   channel * SCALER_CHANNEL_STRIDE) >> 2];
        uint32_t status = s->regs[(SCALER_DISPSTAT0 +
                                  channel * SCALER_CHANNEL_STRIDE) >> 2];

        if (!(control & SCALER_DISPCTRLX_ENABLE) &&
            (control & BIT(15)) && (status >> 30) == 1) {
            /* A completed one-shot retains its last image and EOF state. */
            bcm2711_hvs_apply_scanout(s, channel, true);
        } else {
            bcm2711_hvs_update_channel(s, channel);
        }
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
    fb = object_property_get_link(OBJECT(dev), "fb1", errp);
    if (!fb) {
        return;
    }
    s->fb1 = BCM2835_FB(fb);
}

static void bcm2711_hvs_init(Object *obj)
{
    BCM2711HVSState *s = BCM2711_HVS(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &bcm2711_hvs_ops, s,
                          TYPE_BCM2711_HVS, BCM2711_HVS_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    qdev_init_gpio_in_named(DEVICE(obj), bcm2711_hvs_vblank, "vblank", 2);
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
