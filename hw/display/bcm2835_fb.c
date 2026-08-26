/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 * Refactoring for Pi2 Copyright (c) 2015, Microsoft. Written by Andrew Baumann.
 *
 * Heavily based on milkymist-vgafb.c, copyright terms below:
 *  QEMU model of the Milkymist VGA framebuffer.
 *
 *  Copyright (c) 2010-2012 Michael Walle <michael@walle.cc>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/display/bcm2835_fb.h"
#include "hw/core/hw-error.h"
#include "hw/core/irq.h"
#include "ui/console.h"
#include "framebuffer.h"
#include "ui/pixel_ops.h"
#include "hw/misc/bcm2835_mbox_defs.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define DEFAULT_VCRAM_SIZE 0x4000000
#define BCM2835_FB_OFFSET  0x00100000

/* Maximum permitted framebuffer size; experimentally determined on an rpi2 */
#define XRES_MAX 3840
#define YRES_MAX 2560
#define BPP_MAX 32
/* Framebuffer size used if guest requests zero size */
#define XRES_SMALL 592
#define YRES_SMALL 488

static void fb_invalidate_display(void *opaque)
{
    BCM2835FBState *s = BCM2835_FB(opaque);

    s->invalidate = true;
}

static void draw_line_src16(void *opaque, uint8_t *dst, const uint8_t *src,
                            int width, int deststep)
{
    BCM2835FBState *s = opaque;
    uint16_t rgb565;
    uint32_t rgb888;
    uint8_t r, g, b;
    DisplaySurface *surface = qemu_console_surface(s->con);
    int bpp = surface_bits_per_pixel(surface);

    while (width--) {
        switch (s->config.bpp) {
        case 8:
            /* lookup palette starting at video ram base
             * TODO: cache translation, rather than doing this each time!
             */
            rgb888 = ldl_le_phys(&s->dma_as, s->vcram_base + (*src << 2));
            r = (rgb888 >> 0) & 0xff;
            g = (rgb888 >> 8) & 0xff;
            b = (rgb888 >> 16) & 0xff;
            src++;
            break;
        case 16:
            rgb565 = lduw_le_p(src);
            r = ((rgb565 >> 11) & 0x1f) << 3;
            g = ((rgb565 >>  5) & 0x3f) << 2;
            b = ((rgb565 >>  0) & 0x1f) << 3;
            src += 2;
            break;
        case 24:
            rgb888 = bcm2835_fb_read_rgb24(src);
            r = (rgb888 >> 0) & 0xff;
            g = (rgb888 >> 8) & 0xff;
            b = (rgb888 >> 16) & 0xff;
            src += 3;
            break;
        case 32:
            rgb888 = ldl_le_p(src);
            r = (rgb888 >> 0) & 0xff;
            g = (rgb888 >> 8) & 0xff;
            b = (rgb888 >> 16) & 0xff;
            src += 4;
            break;
        default:
            r = 0;
            g = 0;
            b = 0;
            break;
        }

        if (s->config.pixo == 0) {
            /* swap to BGR pixel format */
            uint8_t tmp = r;
            r = b;
            b = tmp;
        }

        switch (bpp) {
        case 8:
            *dst++ = rgb_to_pixel8(r, g, b);
            break;
        case 15:
            *(uint16_t *)dst = rgb_to_pixel15(r, g, b);
            dst += 2;
            break;
        case 16:
            *(uint16_t *)dst = rgb_to_pixel16(r, g, b);
            dst += 2;
            break;
        case 24:
            rgb888 = rgb_to_pixel24(r, g, b);
            *dst++ = rgb888 & 0xff;
            *dst++ = (rgb888 >> 8) & 0xff;
            *dst++ = (rgb888 >> 16) & 0xff;
            break;
        case 32:
            *(uint32_t *)dst = rgb_to_pixel32(r, g, b);
            dst += 4;
            break;
        default:
            return;
        }
    }
}

static void fb_hvs_decode_pixel(const BCM2835FBHVSLayer *layer,
                                const uint8_t *source,
                                uint8_t *red, uint8_t *green,
                                uint8_t *blue, uint8_t *alpha)
{
    uint32_t pixel;

    switch (layer->bpp) {
    case 16:
        pixel = lduw_le_p(source);
        *red = ((pixel >> 11) & 0x1f) << 3;
        *green = ((pixel >> 5) & 0x3f) << 2;
        *blue = (pixel & 0x1f) << 3;
        *alpha = 0xff;
        break;
    case 24:
        *red = source[0];
        *green = source[1];
        *blue = source[2];
        *alpha = 0xff;
        break;
    case 32:
        *red = source[0];
        *green = source[1];
        *blue = source[2];
        *alpha = source[3];
        break;
    default:
        *red = *green = *blue = 0;
        *alpha = 0xff;
        break;
    }

    if (!layer->pixo) {
        uint8_t swap = *red;

        *red = *blue;
        *blue = swap;
    }
}

static uint32_t fb_hvs_blend_pixel(uint32_t destination,
                                   const BCM2835FBHVSLayer *layer,
                                   uint8_t red, uint8_t green,
                                   uint8_t blue, uint8_t pixel_alpha)
{
    unsigned int plane_alpha =
        (MIN(layer->alpha, 0xfffU) * 0xffU + 0x7ffU) / 0xfffU;
    unsigned int alpha;
    unsigned int dest_red = (destination >> 16) & 0xff;
    unsigned int dest_green = (destination >> 8) & 0xff;
    unsigned int dest_blue = destination & 0xff;
    unsigned int out_red;
    unsigned int out_green;
    unsigned int out_blue;

    if (layer->alpha_mode == 0) {
        alpha = pixel_alpha;
        if (layer->alpha_mix) {
            alpha = (alpha * plane_alpha + 0x7f) / 0xff;
        }
    } else {
        alpha = plane_alpha;
    }

    if (!alpha) {
        return destination;
    }
    if (alpha == 0xff && !layer->alpha_premult) {
        return (red << 16) | (green << 8) | blue;
    }

    if (layer->alpha_mode == 0 && layer->alpha_premult) {
        unsigned int color_alpha = layer->alpha_mix ? plane_alpha : 0xff;

        out_red = (red * color_alpha + 0x7f) / 0xff;
        out_green = (green * color_alpha + 0x7f) / 0xff;
        out_blue = (blue * color_alpha + 0x7f) / 0xff;
        out_red += (dest_red * (0xff - alpha) + 0x7f) / 0xff;
        out_green += (dest_green * (0xff - alpha) + 0x7f) / 0xff;
        out_blue += (dest_blue * (0xff - alpha) + 0x7f) / 0xff;
        out_red = MIN(out_red, 0xffU);
        out_green = MIN(out_green, 0xffU);
        out_blue = MIN(out_blue, 0xffU);
    } else {
        out_red = (red * alpha + dest_red * (0xff - alpha) + 0x7f) /
                  0xff;
        out_green = (green * alpha + dest_green * (0xff - alpha) +
                     0x7f) / 0xff;
        out_blue = (blue * alpha + dest_blue * (0xff - alpha) + 0x7f) /
                   0xff;
    }

    return (out_red << 16) | (out_green << 8) | out_blue;
}

static void fb_hvs_store_pixel(uint8_t *destination, int bpp,
                               uint32_t pixel)
{
    uint8_t red = (pixel >> 16) & 0xff;
    uint8_t green = (pixel >> 8) & 0xff;
    uint8_t blue = pixel & 0xff;

    switch (bpp) {
    case 8:
        destination[0] = rgb_to_pixel8(red, green, blue);
        break;
    case 15:
        *(uint16_t *)destination = rgb_to_pixel15(red, green, blue);
        break;
    case 16:
        *(uint16_t *)destination = rgb_to_pixel16(red, green, blue);
        break;
    case 24:
        pixel = rgb_to_pixel24(red, green, blue);
        destination[0] = pixel & 0xff;
        destination[1] = (pixel >> 8) & 0xff;
        destination[2] = (pixel >> 16) & 0xff;
        break;
    case 32:
        *(uint32_t *)destination = rgb_to_pixel32(red, green, blue);
        break;
    default:
        break;
    }
}

static bool fb_hvs_update_display(BCM2835FBState *s)
{
    DisplaySurface *surface = qemu_console_surface(s->con);
    uint32_t width = s->config.xres;
    uint32_t height = s->config.yres;
    size_t pixel_count = (size_t)width * height;
    int surface_bpp = surface_bits_per_pixel(surface);
    unsigned int surface_bytes = DIV_ROUND_UP(surface_bpp, 8);

    if (!width || !height || !surface_bpp) {
        return true;
    }
    if (s->hvs_pixels_count < pixel_count) {
        s->hvs_pixels = g_renew(uint32_t, s->hvs_pixels, pixel_count);
        s->hvs_pixels_count = pixel_count;
    }
    memset(s->hvs_pixels, 0, pixel_count * sizeof(*s->hvs_pixels));

    for (unsigned int index = 0; index < s->hvs_layer_count; index++) {
        const BCM2835FBHVSLayer *layer = &s->hvs_layers[index];
        unsigned int source_bytes = layer->bpp >> 3;
        size_t source_line_size = (size_t)layer->source_width * source_bytes;
        uint64_t dest_right = (uint64_t)layer->dest_x + layer->dest_width;
        uint64_t dest_bottom = (uint64_t)layer->dest_y + layer->dest_height;
        uint32_t first_x = MIN(layer->dest_x, width);
        uint32_t first_y = MIN(layer->dest_y, height);
        uint32_t last_x = MIN(dest_right, (uint64_t)width);
        uint32_t last_y = MIN(dest_bottom, (uint64_t)height);

        if (!layer->source_width || !layer->source_height ||
            !layer->dest_width || !layer->dest_height || !source_bytes ||
            !source_line_size || first_x >= last_x || first_y >= last_y) {
            continue;
        }
        if (s->hvs_source_line_size < source_line_size) {
            s->hvs_source_line = g_realloc(s->hvs_source_line,
                                           source_line_size);
            s->hvs_source_line_size = source_line_size;
        }

        for (uint32_t y = first_y; y < last_y; y++) {
            uint32_t source_y = ((uint64_t)(y - layer->dest_y) *
                                 layer->source_height) /
                                layer->dest_height;
            hwaddr source_address;

            source_y = MIN(source_y, layer->source_height - 1);
            if (layer->vflip) {
                source_y = layer->source_height - 1 - source_y;
            }
            source_address = layer->base + (hwaddr)source_y * layer->pitch;
            if (address_space_read(&s->dma_as, source_address,
                                   MEMTXATTRS_UNSPECIFIED,
                                   s->hvs_source_line,
                                   source_line_size) != MEMTX_OK) {
                continue;
            }

            for (uint32_t x = first_x; x < last_x; x++) {
                uint32_t source_x = ((uint64_t)(x - layer->dest_x) *
                                     layer->source_width) /
                                    layer->dest_width;
                uint8_t red, green, blue, alpha;
                uint32_t *destination;

                source_x = MIN(source_x, layer->source_width - 1);
                if (layer->hflip) {
                    source_x = layer->source_width - 1 - source_x;
                }
                fb_hvs_decode_pixel(layer,
                                    s->hvs_source_line +
                                    (size_t)source_x * source_bytes,
                                    &red, &green, &blue, &alpha);
                destination = &s->hvs_pixels[(size_t)y * width + x];
                *destination = fb_hvs_blend_pixel(*destination, layer,
                                                  red, green, blue, alpha);
            }
        }
    }

    for (uint32_t y = 0; y < height; y++) {
        uint8_t *destination = surface_data(surface) +
                               (size_t)y * surface_stride(surface);

        for (uint32_t x = 0; x < width; x++) {
            fb_hvs_store_pixel(destination + (size_t)x * surface_bytes,
                               surface_bpp,
                               s->hvs_pixels[(size_t)y * width + x]);
        }
    }
    qemu_console_update(s->con, 0, 0, width, height);
    s->invalidate = false;
    return true;
}

static bool fb_use_offsets(BCM2835FBConfig *config)
{
    /*
     * Return true if we should use the viewport offsets.
     * Experimentally, the hardware seems to do this only if the
     * viewport size is larger than the physical screen. (It doesn't
     * prevent the guest setting this silly viewport setting, though...)
     */
    return config->xres_virtual > config->xres ||
        config->yres_virtual > config->yres;
}

static bool fb_update_display(void *opaque)
{
    BCM2835FBState *s = opaque;
    DisplaySurface *surface = qemu_console_surface(s->con);
    int first = 0;
    int last = 0;
    int src_width = 0;
    int dest_width = 0;
    uint32_t xoff = 0, yoff = 0;

    if (s->lock || !s->config.xres) {
        return true;
    }
    if (s->hvs_mode) {
        return fb_hvs_update_display(s);
    }

    src_width = bcm2835_fb_get_pitch(&s->config);
    if (fb_use_offsets(&s->config)) {
        xoff = s->config.xoffset;
        yoff = s->config.yoffset;
    }

    dest_width = s->config.xres;

    switch (surface_bits_per_pixel(surface)) {
    case 0:
        return true;
    case 8:
        break;
    case 15:
        dest_width *= 2;
        break;
    case 16:
        dest_width *= 2;
        break;
    case 24:
        dest_width *= 3;
        break;
    case 32:
        dest_width *= 4;
        break;
    default:
        hw_error("bcm2835_fb: bad color depth\n");
        break;
    }

    if (s->invalidate) {
        hwaddr base = s->config.base +
                      (hwaddr)xoff * (s->config.bpp >> 3) +
                      (hwaddr)yoff * src_width;
        framebuffer_update_memory_section(&s->fbsection, s->dma_mr,
                                          base,
                                          s->config.yres, src_width);
    }

    framebuffer_update_display(surface, &s->fbsection,
                               s->config.xres, s->config.yres,
                               src_width, dest_width, 0, s->invalidate,
                               draw_line_src16, s, &first, &last);

    if (first >= 0) {
        qemu_console_update(s->con, 0, first, s->config.xres, last - first + 1);
    }

    s->invalidate = false;
    return true;
}

void bcm2835_fb_validate_config(BCM2835FBConfig *config)
{
    /*
     * Validate the config, and clip any bogus values into range,
     * as the hardware does. Note that fb_update_display() relies on
     * this happening to prevent it from performing out-of-range
     * accesses on redraw.
     */
    config->xres = MIN(config->xres, XRES_MAX);
    config->xres_virtual = MIN(config->xres_virtual, XRES_MAX);
    config->yres = MIN(config->yres, YRES_MAX);
    config->yres_virtual = MIN(config->yres_virtual, YRES_MAX);
    config->bpp = MIN(config->bpp, BPP_MAX);

    /*
     * These are not minima: a 40x40 framebuffer will be accepted.
     * They're only used as defaults if the guest asks for zero size.
     */
    if (config->xres == 0) {
        config->xres = XRES_SMALL;
    }
    if (config->yres == 0) {
        config->yres = YRES_SMALL;
    }
    if (config->xres_virtual == 0) {
        config->xres_virtual = config->xres;
    }
    if (config->yres_virtual == 0) {
        config->yres_virtual = config->yres;
    }

    if (fb_use_offsets(config)) {
        /* Clip the offsets so the physical viewport stays in the buffer. */
        config->xoffset = MIN(config->xoffset,
                              MAX(config->xres_virtual, config->xres) -
                              config->xres);
        config->yoffset = MIN(config->yoffset,
                              MAX(config->yres_virtual, config->yres) -
                              config->yres);
    }
}

void bcm2835_fb_reconfigure(BCM2835FBState *s, BCM2835FBConfig *newconfig)
{
    s->lock = true;

    s->hvs_mode = false;
    s->hvs_layer_count = 0;
    s->config = *newconfig;

    s->invalidate = true;
    qemu_console_resize(s->con, s->config.xres, s->config.yres);
    s->lock = false;
}

void bcm2835_fb_reconfigure_hvs(BCM2835FBState *s,
                                uint32_t xres, uint32_t yres,
                                const BCM2835FBHVSLayer *layers,
                                uint32_t layer_count)
{
    g_assert(layer_count <= BCM2835_FB_MAX_HVS_LAYERS);

    s->lock = true;
    s->config.xres = xres;
    s->config.yres = yres;
    s->config.xres_virtual = xres;
    s->config.yres_virtual = yres;
    s->config.xoffset = 0;
    s->config.yoffset = 0;
    s->config.bpp = 32;
    s->config.base = layer_count ? layers[0].base : 0;
    s->config.pixo = 1;
    s->config.alpha = 0;
    s->hvs_mode = true;
    s->hvs_layer_count = layer_count;
    memcpy(s->hvs_layers, layers, layer_count * sizeof(*layers));
    s->invalidate = true;
    qemu_console_resize(s->con, xres, yres);
    s->lock = false;
}

static void bcm2835_fb_mbox_push(BCM2835FBState *s, uint32_t value)
{
    uint32_t pitch;
    uint32_t size;
    BCM2835FBConfig newconf;

    value &= ~0xf;

    newconf.xres = ldl_le_phys(&s->dma_as, value);
    newconf.yres = ldl_le_phys(&s->dma_as, value + 4);
    newconf.xres_virtual = ldl_le_phys(&s->dma_as, value + 8);
    newconf.yres_virtual = ldl_le_phys(&s->dma_as, value + 12);
    newconf.bpp = ldl_le_phys(&s->dma_as, value + 20);
    newconf.xoffset = ldl_le_phys(&s->dma_as, value + 24);
    newconf.yoffset = ldl_le_phys(&s->dma_as, value + 28);

    newconf.base = s->vcram_base + BCM2835_FB_OFFSET;

    /* Copy fields which we don't want to change from the existing config */
    newconf.pixo = s->config.pixo;
    newconf.alpha = s->config.alpha;

    bcm2835_fb_validate_config(&newconf);

    pitch = bcm2835_fb_get_pitch(&newconf);
    size = bcm2835_fb_get_size(&newconf);

    stl_le_phys(&s->dma_as, value + 16, pitch);
    stl_le_phys(&s->dma_as, value + 32, newconf.base);
    stl_le_phys(&s->dma_as, value + 36, size);

    bcm2835_fb_reconfigure(s, &newconf);
}

static uint64_t bcm2835_fb_read(void *opaque, hwaddr offset, unsigned size)
{
    BCM2835FBState *s = opaque;
    uint32_t res = 0;

    switch (offset) {
    case MBOX_AS_DATA:
        res = MBOX_CHAN_FB;
        s->pending = false;
        qemu_set_irq(s->mbox_irq, 0);
        break;

    case MBOX_AS_PENDING:
        res = s->pending;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %"HWADDR_PRIx"\n",
                      __func__, offset);
        return 0;
    }

    return res;
}

static void bcm2835_fb_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
{
    BCM2835FBState *s = opaque;

    switch (offset) {
    case MBOX_AS_DATA:
        /* bcm2835_mbox should check our pending status before pushing */
        assert(!s->pending);
        s->pending = true;
        bcm2835_fb_mbox_push(s, value);
        qemu_set_irq(s->mbox_irq, 1);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %"HWADDR_PRIx"\n",
                      __func__, offset);
        return;
    }
}

static const MemoryRegionOps bcm2835_fb_ops = {
    .read = bcm2835_fb_read,
    .write = bcm2835_fb_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static int bcm2835_fb_post_load(void *opaque, int version_id)
{
    BCM2835FBState *s = opaque;

    if (version_id != 1) {
        return -EINVAL;
    }

    /* A migrated config may have been produced before validation tightened. */
    bcm2835_fb_validate_config(&s->config);

    /* lock is a local redraw guard, not guest-visible state. */
    s->lock = true;
    if (s->con) {
        qemu_console_resize(s->con, s->config.xres, s->config.yres);
    }
    s->lock = false;
    s->invalidate = true;
    qemu_set_irq(s->mbox_irq, s->pending);
    return 0;
}

static const VMStateDescription vmstate_bcm2835_fb = {
    .name = TYPE_BCM2835_FB,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2835_fb_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_BOOL(lock, BCM2835FBState),
        VMSTATE_BOOL(invalidate, BCM2835FBState),
        VMSTATE_BOOL(pending, BCM2835FBState),
        VMSTATE_UINT32(config.xres, BCM2835FBState),
        VMSTATE_UINT32(config.yres, BCM2835FBState),
        VMSTATE_UINT32(config.xres_virtual, BCM2835FBState),
        VMSTATE_UINT32(config.yres_virtual, BCM2835FBState),
        VMSTATE_UINT32(config.xoffset, BCM2835FBState),
        VMSTATE_UINT32(config.yoffset, BCM2835FBState),
        VMSTATE_UINT32(config.bpp, BCM2835FBState),
        VMSTATE_UINT32(config.base, BCM2835FBState),
        VMSTATE_UNUSED(8), /* Was pitch and size */
        VMSTATE_UINT32(config.pixo, BCM2835FBState),
        VMSTATE_UINT32(config.alpha, BCM2835FBState),
        VMSTATE_END_OF_LIST()
    }
};

static const GraphicHwOps vgafb_ops = {
    .invalidate  = fb_invalidate_display,
    .gfx_update  = fb_update_display,
};

static void bcm2835_fb_init(Object *obj)
{
    BCM2835FBState *s = BCM2835_FB(obj);

    memory_region_init_io(&s->iomem, obj, &bcm2835_fb_ops, s, TYPE_BCM2835_FB,
                          0x10);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->mbox_irq);
}

static void bcm2835_fb_reset(DeviceState *dev)
{
    BCM2835FBState *s = BCM2835_FB(dev);

    s->lock = true;
    s->pending = false;
    qemu_set_irq(s->mbox_irq, 0);

    s->config = s->initial_config;

    s->hvs_mode = false;
    s->hvs_layer_count = 0;
    s->invalidate = true;
    if (s->con) {
        qemu_console_resize(s->con, s->config.xres, s->config.yres);
    }
    s->lock = false;
}

static void bcm2835_fb_finalize(Object *obj)
{
    BCM2835FBState *s = BCM2835_FB(obj);

    g_free(s->hvs_pixels);
    g_free(s->hvs_source_line);
}

static void bcm2835_fb_realize(DeviceState *dev, Error **errp)
{
    BCM2835FBState *s = BCM2835_FB(dev);
    Object *obj;

    if (s->vcram_base == 0) {
        error_setg(errp, "%s: required vcram-base property not set", __func__);
        return;
    }

    obj = object_property_get_link(OBJECT(dev), "dma-mr", &error_abort);

    /* Fill in the parts of initial_config that are not set by QOM properties */
    s->initial_config.xres_virtual = s->initial_config.xres;
    s->initial_config.yres_virtual = s->initial_config.yres;
    s->initial_config.xoffset = 0;
    s->initial_config.yoffset = 0;
    s->initial_config.base = s->vcram_base + BCM2835_FB_OFFSET;
    bcm2835_fb_validate_config(&s->initial_config);

    s->dma_mr = MEMORY_REGION(obj);
    address_space_init(&s->dma_as, s->dma_mr, TYPE_BCM2835_FB "-memory");

    bcm2835_fb_reset(dev);

    s->con = qemu_graphic_console_create(dev, 0, &vgafb_ops, s);
    qemu_console_resize(s->con, s->config.xres, s->config.yres);
}

static const Property bcm2835_fb_props[] = {
    DEFINE_PROP_UINT32("vcram-base", BCM2835FBState, vcram_base, 0),/*required*/
    DEFINE_PROP_UINT32("vcram-size", BCM2835FBState, vcram_size,
                       DEFAULT_VCRAM_SIZE),
    DEFINE_PROP_UINT32("xres", BCM2835FBState, initial_config.xres, 640),
    DEFINE_PROP_UINT32("yres", BCM2835FBState, initial_config.yres, 480),
    DEFINE_PROP_UINT32("bpp", BCM2835FBState, initial_config.bpp, 16),
    DEFINE_PROP_UINT32("pixo", BCM2835FBState,
                       initial_config.pixo, 1), /* 1=RGB, 0=BGR */
    DEFINE_PROP_UINT32("alpha", BCM2835FBState,
                       initial_config.alpha, 2), /* alpha ignored */
};

static void bcm2835_fb_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, bcm2835_fb_props);
    dc->realize = bcm2835_fb_realize;
    device_class_set_legacy_reset(dc, bcm2835_fb_reset);
    dc->vmsd = &vmstate_bcm2835_fb;
}

static const TypeInfo bcm2835_fb_info = {
    .name          = TYPE_BCM2835_FB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2835FBState),
    .class_init    = bcm2835_fb_class_init,
    .instance_init = bcm2835_fb_init,
    .instance_finalize = bcm2835_fb_finalize,
};

static void bcm2835_fb_register_types(void)
{
    type_register_static(&bcm2835_fb_info);
}

type_init(bcm2835_fb_register_types)
