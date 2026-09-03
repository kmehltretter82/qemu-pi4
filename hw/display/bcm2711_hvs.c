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
#define SCALER_CTL0_TILING_LINEAR    0
#define SCALER_CTL0_TILING_T         3
#define SCALER_CTL0_ORDER_SHIFT      13
#define SCALER_CTL0_ORDER_MASK       0x3
#define SCALER_CTL0_SCL0_SHIFT       5
#define SCALER_CTL0_SCL_MASK         0x7
#define SCALER5_CTL0_UNITY           BIT(15)
#define SCALER5_CTL0_PIXEL_FORMAT_MASK 0x1f

#define SCALER5_POS0_START_Y_SHIFT   16
#define SCALER5_POS0_START_Y_MASK    0xfff
#define SCALER5_POS0_START_X_MASK    0x3fff
#define SCALER5_POS0_VFLIP           BIT(31)
#define SCALER5_POS0_HFLIP           BIT(15)
#define SCALER5_POS1_HEIGHT_SHIFT    16
#define SCALER5_POS1_HEIGHT_MASK     0x1fff
#define SCALER5_POS1_WIDTH_MASK      0x1fff
#define SCALER5_POS2_HEIGHT_SHIFT    16
#define SCALER5_POS2_HEIGHT_MASK     0x1fff
#define SCALER5_POS2_WIDTH_MASK      0x1fff

/* T-tiled PITCH0 fields, for SCALER_CTL0_TILING_256B_OR_T. */
#define SCALER_PITCH0_SINK_PIX_MASK  (0x3fU << 26)
#define SCALER_PITCH0_TILE_WIDTH_L_MASK (0x7fU << 16)
#define SCALER_PITCH0_TILE_LINE_DIR  BIT(15)
#define SCALER_PITCH0_TILE_INITIAL_LINE_DIR BIT(14)
#define SCALER_PITCH0_TILE_Y_OFFSET_MASK (0x3fU << 8)
#define SCALER_PITCH0_TILE_WIDTH_R_MASK 0x7fU

#define SCALER5_CTL2_ALPHA_MODE_SHIFT 30
#define SCALER5_CTL2_ALPHA_MODE_MASK  0x3
#define SCALER5_CTL2_ALPHA_MODE_PIPELINE 0
#define SCALER5_CTL2_ALPHA_MODE_FIXED 1
#define SCALER5_CTL2_ALPHA_PREMULT    BIT(29)
#define SCALER5_CTL2_ALPHA_MIX        BIT(28)
#define SCALER5_CTL2_ALPHA_SHIFT      4
#define SCALER5_CTL2_ALPHA_MASK       0xfff

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

typedef enum BCM2711HVSScaleMode {
    BCM2711_HVS_SCALE_NONE,
    BCM2711_HVS_SCALE_PPF,
    BCM2711_HVS_SCALE_TPZ,
} BCM2711HVSScaleMode;

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
            /* DRM RGB888 is stored as B, G, R on little-endian guests. */
            *pixo = 0;
            return true;
        }
        if (order == HVS_PIXEL_ORDER_XBGR) {
            *pixo = 1;
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

static void bcm2711_hvs_decode_scaling(uint32_t ctl,
                                       BCM2711HVSScaleMode *x_mode,
                                       BCM2711HVSScaleMode *y_mode)
{
    uint32_t scl0 = (ctl >> SCALER_CTL0_SCL0_SHIFT) & SCALER_CTL0_SCL_MASK;

    *x_mode = BCM2711_HVS_SCALE_NONE;
    *y_mode = BCM2711_HVS_SCALE_NONE;

    switch (scl0) {
    case 0: /* Horizontal PPF, vertical PPF. */
        *x_mode = BCM2711_HVS_SCALE_PPF;
        *y_mode = BCM2711_HVS_SCALE_PPF;
        break;
    case 1: /* Horizontal TPZ, vertical PPF. */
        *x_mode = BCM2711_HVS_SCALE_TPZ;
        *y_mode = BCM2711_HVS_SCALE_PPF;
        break;
    case 2: /* Horizontal PPF, vertical TPZ. */
        *x_mode = BCM2711_HVS_SCALE_PPF;
        *y_mode = BCM2711_HVS_SCALE_TPZ;
        break;
    case 3: /* Horizontal TPZ, vertical TPZ. */
        *x_mode = BCM2711_HVS_SCALE_TPZ;
        *y_mode = BCM2711_HVS_SCALE_TPZ;
        break;
    case 4: /* Horizontal PPF, vertical unity. */
        *x_mode = BCM2711_HVS_SCALE_PPF;
        break;
    case 5: /* Horizontal unity, vertical PPF. */
        *y_mode = BCM2711_HVS_SCALE_PPF;
        break;
    case 6: /* Horizontal unity, vertical TPZ. */
        *y_mode = BCM2711_HVS_SCALE_TPZ;
        break;
    case 7: /* Horizontal TPZ, vertical unity. */
        *x_mode = BCM2711_HVS_SCALE_TPZ;
        break;
    }
}

/*
 * A complete Linux RGB display list carries the line-buffer, PPF/TPZ
 * parameters, and (when PPF is in use) four kernel pointers after the source
 * pitch.  Older fork-local tests deliberately exercise only the short,
 * functional list form, which has none of that state and remains on the
 * nearest-neighbour path.  This lets the PPF approximation below target the
 * real Linux-programmed contract without treating an incomplete list as a
 * hardware filter configuration.
 */
static bool bcm2711_hvs_has_complete_scaling_layout(uint32_t dlist,
                                                    uint32_t size,
                                                    uint32_t pitch_index,
                                                    BCM2711HVSScaleMode x_mode,
                                                    BCM2711HVSScaleMode y_mode)
{
    uint32_t next_word = pitch_index + 1;
    uint32_t list_end = dlist + size;

    if (x_mode == BCM2711_HVS_SCALE_NONE &&
        y_mode == BCM2711_HVS_SCALE_NONE) {
        return false;
    }

    if (y_mode != BCM2711_HVS_SCALE_NONE) {
        next_word++; /* LBM base address */
    }
    next_word += x_mode == BCM2711_HVS_SCALE_PPF ? 1 :
                 x_mode == BCM2711_HVS_SCALE_TPZ ? 2 : 0;
    next_word += y_mode == BCM2711_HVS_SCALE_PPF ? 2 :
                 y_mode == BCM2711_HVS_SCALE_TPZ ? 3 : 0;
    if (x_mode == BCM2711_HVS_SCALE_PPF ||
        y_mode == BCM2711_HVS_SCALE_PPF) {
        next_word += 4; /* H/V kernel pointers for both plane channels */
    }

    return next_word <= list_end;
}

static bool bcm2711_hvs_apply_scanout(BCM2711HVSState *s,
                                      unsigned int channel)
{
    BCM2835FBHVSLayer layers[BCM2835_FB_MAX_HVS_LAYERS] = { 0 };
    BCM2835FBConfig config = { 0 };
    uint32_t channel_ctl;
    uint32_t dlist;
    uint32_t layer_count = 0;

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
    while (true) {
        uint32_t base_index = (SCALER5_DLIST_START >> 2) + dlist;
        uint32_t ctl = s->regs[base_index];
        BCM2835FBHVSLayer *layer;
        uint32_t size;
        uint32_t pos0;
        uint32_t ctl2;
        uint32_t pos1 = 0;
        uint32_t pos2_index;
        uint32_t pos2;
        uint32_t ptr_index;
        uint32_t pitch_index;
        uint32_t bytes_per_pixel;
        uint32_t tiling;
        uint32_t pitch;
        BCM2711HVSScaleMode x_mode;
        BCM2711HVSScaleMode y_mode;

        if (ctl & SCALER_CTL0_END) {
            break;
        }
        if (layer_count == BCM2835_FB_MAX_HVS_LAYERS) {
            qemu_log_mask(LOG_UNIMP,
                          "%s: more than %u HVS planes are not implemented\n",
                          TYPE_BCM2711_HVS, BCM2835_FB_MAX_HVS_LAYERS);
            return false;
        }
        layer = &layers[layer_count];
        if (!(ctl & SCALER_CTL0_VALID)) {
            return false;
        }
        tiling = (ctl >> SCALER_CTL0_TILING_SHIFT) &
                 SCALER_CTL0_TILING_MASK;
        if (tiling != SCALER_CTL0_TILING_LINEAR &&
            tiling != SCALER_CTL0_TILING_T) {
            qemu_log_mask(LOG_UNIMP,
                          "%s: HVS tiling mode %u is not implemented\n",
                          TYPE_BCM2711_HVS, tiling);
            return false;
        }
        if (tiling == SCALER_CTL0_TILING_T &&
            !(ctl & SCALER5_CTL0_UNITY)) {
            qemu_log_mask(LOG_UNIMP,
                          "%s: scaled HVS T-tiled planes are not implemented\n",
                          TYPE_BCM2711_HVS);
            return false;
        }

        size = (ctl >> SCALER_CTL0_SIZE_SHIFT) & SCALER_CTL0_SIZE_MASK;
        if (size < 8 || dlist + size >= HVS_DLIST_WORDS) {
            return false;
        }
        if (!bcm2711_hvs_decode_format(ctl, &layer->bpp,
                                       &layer->pixo)) {
            return false;
        }

        pos0 = s->regs[base_index + 1];
        ctl2 = s->regs[base_index + 2];
        if (ctl & SCALER5_CTL0_UNITY) {
            pos2_index = dlist + 3;
        } else {
            pos1 = s->regs[base_index + 3];
            pos2_index = dlist + 4;
        }
        ptr_index = pos2_index + 2;
        pitch_index = ptr_index + 2;
        if (pitch_index >= dlist + size) {
            return false;
        }
        pos2 = s->regs[(SCALER5_DLIST_START >> 2) + pos2_index];

        layer->source_width = pos2 & SCALER5_POS2_WIDTH_MASK;
        layer->source_height =
            (pos2 >> SCALER5_POS2_HEIGHT_SHIFT) &
            SCALER5_POS2_HEIGHT_MASK;
        layer->dest_x = pos0 & SCALER5_POS0_START_X_MASK;
        layer->dest_y = (pos0 >> SCALER5_POS0_START_Y_SHIFT) &
                        SCALER5_POS0_START_Y_MASK;
        layer->hflip = pos0 & SCALER5_POS0_HFLIP;
        layer->vflip = pos0 & SCALER5_POS0_VFLIP;
        if (ctl & SCALER5_CTL0_UNITY) {
            layer->dest_width = layer->source_width;
            layer->dest_height = layer->source_height;
        } else {
            layer->dest_width = pos1 & SCALER5_POS1_WIDTH_MASK;
            layer->dest_height = (pos1 >> SCALER5_POS1_HEIGHT_SHIFT) &
                                 SCALER5_POS1_HEIGHT_MASK;
        }
        if (!layer->source_width || !layer->source_height ||
            !layer->dest_width || !layer->dest_height ||
            layer->source_width > HVS_MAX_XRES ||
            layer->source_height > HVS_MAX_YRES) {
            return false;
        }

        bytes_per_pixel = layer->bpp >> 3;
        pitch = s->regs[(SCALER5_DLIST_START >> 2) + pitch_index];
        layer->base =
            s->regs[(SCALER5_DLIST_START >> 2) + ptr_index];
        if (tiling == SCALER_CTL0_TILING_LINEAR) {
            layer->pitch = pitch & 0xffff;
            if (!layer->pitch || layer->pitch % bytes_per_pixel ||
                layer->pitch < layer->source_width * bytes_per_pixel ||
                layer->pitch / bytes_per_pixel > HVS_MAX_XRES) {
                return false;
            }
        } else {
            uint32_t tile_width;

            /*
             * The full-surface T form is the useful first case for a GPU
             * scanout buffer.  Cropping and vertical reflection alter both
             * the base and PITCH0 traversal fields, so decline them until
             * that complete contract can be modeled rather than scrambling
             * the source image.
             */
            if (layer->bpp != 16 && layer->bpp != 32) {
                qemu_log_mask(LOG_UNIMP,
                              "%s: only RGB565 and RGBA8888 HVS T-tiled "
                              "planes are implemented\n",
                              TYPE_BCM2711_HVS);
                return false;
            }
            if (layer->vflip || (layer->base & 0xfff) ||
                (pitch & (SCALER_PITCH0_SINK_PIX_MASK |
                          SCALER_PITCH0_TILE_WIDTH_L_MASK |
                          SCALER_PITCH0_TILE_LINE_DIR |
                          SCALER_PITCH0_TILE_INITIAL_LINE_DIR |
                          SCALER_PITCH0_TILE_Y_OFFSET_MASK))) {
                qemu_log_mask(LOG_UNIMP,
                              "%s: cropped or vertically reflected HVS "
                              "T-tiled plane is not implemented\n",
                              TYPE_BCM2711_HVS);
                return false;
            }
            tile_width = layer->bpp == 16 ? 64 : 32;
            layer->tile_columns = pitch & SCALER_PITCH0_TILE_WIDTH_R_MASK;
            if (!layer->tile_columns ||
                layer->tile_columns > DIV_ROUND_UP(HVS_MAX_XRES,
                                                    tile_width) ||
                layer->tile_columns * tile_width < layer->source_width) {
                return false;
            }
            layer->t_tiled = true;
        }
        layer->ppf_x = false;
        layer->ppf_y = false;
        layer->tpz_x = false;
        layer->tpz_y = false;
        if (!(ctl & SCALER5_CTL0_UNITY)) {
            bcm2711_hvs_decode_scaling(ctl, &x_mode, &y_mode);
            if (bcm2711_hvs_has_complete_scaling_layout(dlist, size,
                                                        pitch_index,
                                                        x_mode, y_mode)) {
                layer->ppf_x = x_mode == BCM2711_HVS_SCALE_PPF;
                layer->ppf_y = y_mode == BCM2711_HVS_SCALE_PPF;
                layer->tpz_x = x_mode == BCM2711_HVS_SCALE_TPZ &&
                               layer->source_width > layer->dest_width;
                layer->tpz_y = y_mode == BCM2711_HVS_SCALE_TPZ &&
                               layer->source_height > layer->dest_height;
            }
        }
        layer->alpha = (ctl2 >> SCALER5_CTL2_ALPHA_SHIFT) &
                       SCALER5_CTL2_ALPHA_MASK;
        layer->alpha_mode = (ctl2 >> SCALER5_CTL2_ALPHA_MODE_SHIFT) &
                            SCALER5_CTL2_ALPHA_MODE_MASK;
        if (layer->alpha_mode != SCALER5_CTL2_ALPHA_MODE_PIPELINE &&
            layer->alpha_mode != SCALER5_CTL2_ALPHA_MODE_FIXED) {
            qemu_log_mask(LOG_UNIMP,
                          "%s: HVS alpha mode %u is not implemented\n",
                          TYPE_BCM2711_HVS, layer->alpha_mode);
            return false;
        }
        layer->alpha_mix = ctl2 & SCALER5_CTL2_ALPHA_MIX;
        layer->alpha_premult = ctl2 & SCALER5_CTL2_ALPHA_PREMULT;

        layer_count++;
        dlist += size;
    }
    if (!layer_count) {
        return false;
    }

    if (layer_count == 1 && !layers[0].t_tiled && layers[0].dest_x == 0 &&
        layers[0].dest_y == 0 && !layers[0].hflip && !layers[0].vflip &&
        layers[0].source_width == config.xres &&
        layers[0].source_height == config.yres &&
        layers[0].dest_width == config.xres &&
        layers[0].dest_height == config.yres &&
        layers[0].alpha_mode == SCALER5_CTL2_ALPHA_MODE_FIXED &&
        layers[0].alpha == 0xfff) {
        uint32_t bytes_per_pixel = layers[0].bpp >> 3;

        config.bpp = layers[0].bpp;
        config.pixo = layers[0].pixo;
        config.xres_virtual = layers[0].pitch / bytes_per_pixel;
        config.yres_virtual = config.yres;
        config.xoffset = 0;
        config.yoffset = 0;
        config.base = layers[0].base;
        config.alpha = 0;
        bcm2835_fb_validate_config(&config);
        bcm2835_fb_reconfigure(s->fb, &config);
    } else {
        bcm2835_fb_reconfigure_hvs(s->fb, config.xres, config.yres,
                                   layers, layer_count);
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
