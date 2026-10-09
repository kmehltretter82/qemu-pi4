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

/* HVS components remain at 12 bits through filtering, CSC and blending. */
typedef struct HVSImage {
    uint16_t *data;
    uint32_t width, height, components;
} HVSImage;

static HVSImage fb_hvs_image(uint32_t width, uint32_t height,
                            uint32_t components)
{
    HVSImage image = { .width = width, .height = height,
                       .components = components };

    image.data = g_new0(uint16_t, (size_t)width * height * components);
    return image;
}

static uint16_t fb_hvs_clamp(int64_t value)
{
    return MAX(0, MIN(value, 4095));
}

static uint16_t fb_hvs_expand(uint32_t value, unsigned int bits)
{
    uint32_t result = 0;

    for (int shift = 12 - bits; shift > -(int)bits; shift -= bits) {
        result |= shift >= 0 ? value << shift : value >> -shift;
    }
    return result & 0xfff;
}

static void fb_hvs_decode_pixel(const BCM2835FBHVSLayer *layer,
                                const uint8_t *source, uint16_t *rgba)
{
    uint32_t v = 0;
    unsigned int bits = 8;
    uint32_t r = 0, g = 0, b = 0;

    for (unsigned int i = 0; i < layer->bpp / 8; i++) {
        v |= (uint32_t)source[i] << (8 * i);
    }
    rgba[3] = 4095;
    switch (layer->format) {
    case 0:
        if (layer->order == 3) {
            rgba[0] = fb_hvs_expand(v & 7, 3);
            rgba[1] = fb_hvs_expand((v >> 3) & 7, 3);
            rgba[2] = fb_hvs_expand(v >> 6, 2);
        } else {
            rgba[0] = fb_hvs_expand(v >> 5, 3);
            rgba[1] = fb_hvs_expand((v >> 2) & 7, 3);
            rgba[2] = fb_hvs_expand(v & 3, 2);
        }
        return;
    case 3:
        r = (v >> 10) & 31;
        g = (v >> 5) & 31;
        b = v & 31;
        bits = 5;
        rgba[3] = v & BIT(15) ? 4095 : 0;
        break;
    case 4:
        rgba[0] = fb_hvs_expand((v >> 11) & 31, 5);
        rgba[1] = fb_hvs_expand((v >> 5) & 63, 6);
        rgba[2] = fb_hvs_expand(v & 31, 5);
        goto order;
    case 5:
    case 7:
        r = (v >> 16) & 255;
        g = (v >> 8) & 255;
        b = v & 255;
        if (layer->format == 7) {
            rgba[3] = fb_hvs_expand(v >> 24, 8);
        }
        break;
    case 16:
        r = (v >> 20) & 1023;
        g = (v >> 10) & 1023;
        b = v & 1023;
        bits = 10;
        rgba[3] = fb_hvs_expand(v >> 30, 2);
        break;
    }
    rgba[0] = fb_hvs_expand(r, bits);
    rgba[1] = fb_hvs_expand(g, bits);
    rgba[2] = fb_hvs_expand(b, bits);
order:
    if (layer->order == 3) {
        uint16_t swap = rgba[0];

        rgba[0] = rgba[2];
        rgba[2] = swap;
    }
}

static bool fb_hvs_read_line(BCM2835FBState *s,
                             const BCM2835FBHVSLayer *layer,
                             uint32_t base, uint32_t pitch, uint32_t y,
                             uint8_t *line, size_t length)
{
    int64_t row = layer->vflip ? -(int64_t)y : y;

    if (!layer->column_tiled) {
        uint32_t address = base + row * pitch;

        return address_space_read(&s->dma_as, address, MEMTXATTRS_UNSPECIFIED,
                                  line, length) == MEMTX_OK;
    }
    /*
     * HVS5 tiling=3 selects 256-byte columns.  The low pitch halfword is
     * the column stride in rows; bits 22:16 encode the additional row step.
     * The pointer already includes the crop and vertical reflection origin.
     */
    for (size_t at = 0; at < length; ) {
        uint32_t x = (base & 255) + at;
        size_t chunk = MIN(length - at, 256 - (x & 255));
        uint32_t address = (base & ~255U) + (x / 256) * (pitch & 0xffff) * 256 +
                           row * (((pitch >> 16) & 127) + 1) * 256 + (x & 255);

        if (address_space_read(&s->dma_as, address, MEMTXATTRS_UNSPECIFIED,
                               line + at, chunk) != MEMTX_OK) {
            return false;
        }
        at += chunk;
    }
    return true;
}

static HVSImage fb_hvs_read_rgb(BCM2835FBState *s,
                                const BCM2835FBHVSLayer *layer)
{
    HVSImage image = fb_hvs_image(layer->source_width, layer->source_height, 4);
    unsigned int cpp = layer->bpp / 8;
    size_t length = (size_t)image.width * cpp;
    g_autofree uint8_t *line = g_malloc(length);

    for (uint32_t y = 0; y < image.height; y++) {
        if (!fb_hvs_read_line(s, layer, layer->base, layer->pitch, y,
                              line, length)) {
            g_free(image.data);
            image.data = NULL;
            break;
        }
        for (uint32_t x = 0; x < image.width; x++) {
            uint16_t *rgba = image.data + ((size_t)y * image.width + x) * 4;

            fb_hvs_decode_pixel(layer, line + x * cpp,
                                 rgba);
        }
    }
    return image;
}

static HVSImage fb_hvs_read_yuv(BCM2835FBState *s,
                                const BCM2835FBHVSLayer *layer, bool chroma)
{
    unsigned int vsub = layer->format == 8 || layer->format == 9 ? 2 : 1;
    uint32_t width = chroma ? DIV_ROUND_UP(layer->source_width, 2) :
                             layer->source_width;
    uint32_t height = chroma ? MAX(1, layer->source_height / vsub) :
                              layer->source_height;
    bool planar = layer->format == 8 || layer->format == 10;
    unsigned int step = chroma && !planar ? 2 : 1;
    HVSImage image = fb_hvs_image(width, height, chroma ? 2 : 1);
    g_autofree uint8_t *line = g_malloc(width * step);

    for (unsigned int c = 0; c < image.components; c++) {
        uint32_t base = chroma ? layer->chroma_base[planar ? c : 0] :
                                layer->base;
        uint32_t pitch = chroma ? layer->chroma_pitch[planar ? c : 0] :
                                 layer->pitch;
        unsigned int component = chroma && layer->order == 1 ? 1 - c : c;

        for (uint32_t y = 0; y < height; y++) {
            if (!fb_hvs_read_line(s, layer, base, pitch, y,
                                  line, width * step)) {
                g_free(image.data);
                image.data = NULL;
                return image;
            }
            for (uint32_t x = 0; x < width; x++) {
                uint8_t value = line[x * step + (step == 2 ? c : 0)];

                image.data[((size_t)y * width + x) * image.components +
                           component] = fb_hvs_expand(value, 8);
            }
        }
    }
    return image;
}

static uint16_t fb_hvs_filter(const uint16_t *source, uint32_t size,
                              size_t stride, uint32_t output,
                              const BCM2835FBHVSScale *scale)
{
    if (scale->mode == BCM2835_FB_HVS_SCALE_PPF) {
        int64_t position = (int64_t)sextract32(scale->param, 0, 7) * 1024 +
                           (int64_t)output * ((scale->param >> 8) & 0x1ffff);
        int base = position >> 16;
        unsigned int phase = (position & 0xffff) >> 13;
        int sub = scale->param & BIT(31) ? 0 :
                           (position >> 10) & 7;
        int coefficient[4];
        int sum = 0, largest = 0;
        int64_t accumulator = 512;

        for (unsigned int tap = 0; tap < 4; tap++) {
            unsigned int at = 8 * (3 - tap) + phase;

            coefficient[tap] = (scale->kernel[at] * (8 - sub) +
                                scale->kernel[at + 1] * sub + 1) >> 1;
            sum += coefficient[tap];
            if (coefficient[tap] > coefficient[largest]) {
                largest = tap;
            }
        }
        if (scale->param & BIT(30)) {
            coefficient[largest] += 1024 - sum;
        }
        for (unsigned int tap = 0; tap < 4; tap++) {
            uint32_t index = MAX(0, MIN(base - 1 + (int)tap, (int)size - 1));

            accumulator += (int64_t)coefficient[tap] * source[index * stride];
        }
        return fb_hvs_clamp(accumulator >> 10);
    } else if (scale->mode == BCM2835_FB_HVS_SCALE_TPZ) {
        uint32_t step = (scale->param >> 8) & 0x1fffff;
        uint64_t first, last, accumulator = 0;

        step = step ? step : 1 << 21;
        first = (uint64_t)output * step;
        last = first + step;
        for (uint64_t index = first >> 16; (index << 16) < last; index++) {
            uint64_t lo = MAX(first, index << 16);
            uint64_t hi = MIN(last, (index + 1) << 16);

            accumulator += (hi - lo) * source[MIN(index, size - 1) * stride];
        }
        return fb_hvs_clamp((accumulator * (scale->reciprocal & 0xffff) +
                             (1ULL << 31)) >> 32);
    }
    return source[MIN(output, size - 1) * stride];
}

static void fb_hvs_scale_axis(HVSImage *image, uint32_t output_size,
                               const BCM2835FBHVSScale *scale, bool horizontal)
{
    uint32_t input_size = horizontal ? image->width : image->height;
    HVSImage out;

    if (!scale->mode && input_size == output_size) {
        return;
    }
    out = fb_hvs_image(horizontal ? output_size : image->width,
                       horizontal ? image->height : output_size,
                       image->components);
    for (uint32_t y = 0; y < out.height; y++) {
        for (uint32_t x = 0; x < out.width; x++) {
            uint32_t index = horizontal ? x : y;
            size_t start = horizontal ? (size_t)y * image->width : x;
            size_t stride = horizontal ? 1 : image->width;

            if (!scale->mode) {
                index = (uint64_t)index * input_size / output_size;
            }
            for (uint32_t c = 0; c < image->components; c++) {
                out.data[((size_t)y * out.width + x) * out.components + c] =
                    fb_hvs_filter(image->data + start * image->components + c,
                                   input_size, stride * image->components,
                                   index, scale);
            }
        }
    }
    g_free(image->data);
    *image = out;
}

static void fb_hvs_scale(HVSImage *image, const BCM2835FBHVSLayer *layer,
                          unsigned int channel)
{
    const BCM2835FBHVSScale *horizontal = &layer->scale[channel][0];
    const BCM2835FBHVSScale *vertical = &layer->scale[channel][1];
    bool horizontal_first = horizontal->mode == BCM2835_FB_HVS_SCALE_TPZ ||
        (horizontal->mode == BCM2835_FB_HVS_SCALE_PPF &&
         ((horizontal->param >> 8) & 0x1ffff) >= 0x10000);

    /* The HVS keeps the narrower image in the line buffer. */
    if (horizontal_first) {
        fb_hvs_scale_axis(image, layer->dest_width, horizontal, true);
        fb_hvs_scale_axis(image, layer->dest_height, vertical, false);
    } else {
        fb_hvs_scale_axis(image, layer->dest_height, vertical, false);
        fb_hvs_scale_axis(image, layer->dest_width, horizontal, true);
    }
}

static HVSImage fb_hvs_convert_yuv(BCM2835FBState *s,
                                   const BCM2835FBHVSLayer *layer)
{
    HVSImage luma = fb_hvs_read_yuv(s, layer, false);
    HVSImage chroma = fb_hvs_read_yuv(s, layer, true);
    HVSImage out = { 0 };
    uint32_t c0 = layer->csc[0], c1 = layer->csc[1], c2 = layer->csc[2];
    int yofs = sextract32(c0, 16, 8) * 16;
    int cbofs = sextract32(c0, 8, 8) * 16 - 2048;
    int crofs = sextract32(c0, 0, 8) * 16 - 2048;
    int yy = extract32(c1, 2, 10);
    int cb_red = sextract32(c2, 20, 10);
    int cr_red = extract32(c2, 10, 10);
    int cb_green = sextract32(c1, 22, 10);
    int cr_green = sextract32(c1, 12, 10);
    int cb_blue = extract32(c2, 0, 10);
    int cr_blue = sextract32(((c0 >> 24) << 2) | (c1 & 3), 0, 10);

    if (!luma.data || !chroma.data) {
        goto done;
    }
    fb_hvs_scale(&luma, layer, 1);
    fb_hvs_scale(&chroma, layer, 0);
    out = fb_hvs_image(layer->dest_width, layer->dest_height, 4);
    for (size_t i = 0; i < (size_t)out.width * out.height; i++) {
        int y = MAX(0, luma.data[i] + yofs);
        int cb = chroma.data[2 * i] + cbofs;
        int cr = chroma.data[2 * i + 1] + crofs;

        out.data[4 * i] = fb_hvs_clamp((yy * y + cb_red * cb +
                                      cr_red * cr + 128) >> 8);
        out.data[4 * i + 1] = fb_hvs_clamp((yy * y + cb_green * cb +
                                          cr_green * cr + 128) >> 8);
        out.data[4 * i + 2] = fb_hvs_clamp((yy * y + cb_blue * cb +
                                          cr_blue * cr + 128) >> 8);
        out.data[4 * i + 3] = 4095;
    }
done:
    g_free(luma.data);
    g_free(chroma.data);
    return out;
}

static unsigned int fb_hvs_multiply(unsigned int value, unsigned int alpha)
{
    return (value * alpha + 2047) / 4095;
}

static uint64_t fb_hvs_blend_pixel(uint64_t destination,
                                   const BCM2835FBHVSLayer *layer,
                                   const uint16_t *rgba)
{
    unsigned int alpha = layer->alpha_mode == 0 ? rgba[3] : layer->alpha;
    uint64_t result = 0;

    if (layer->alpha_mix) {
        alpha = fb_hvs_multiply(alpha, layer->alpha);
    }
    for (unsigned int c = 0; c < 3; c++) {
        unsigned int value = rgba[c];
        unsigned int dst = (destination >> (c * 12)) & 4095;

        if (layer->alpha_mode == 0 && layer->alpha_premult) {
            if (layer->alpha_mix) {
                value = fb_hvs_multiply(value, layer->alpha);
            }
        } else {
            value = fb_hvs_multiply(value, alpha);
        }
        value += fb_hvs_multiply(dst, 4095 - alpha);
        result |= (uint64_t)MIN(value, 4095) << (c * 12);
    }
    return result;
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
    uint32_t width = s->config.xres, height = s->config.yres;
    size_t count = (size_t)width * height;
    int bpp = surface_bits_per_pixel(surface);
    unsigned int bytes = DIV_ROUND_UP(bpp, 8);

    if (!width || !height || !bpp) {
        return true;
    }
    if (s->hvs_pixels_count < count) {
        s->hvs_pixels = g_renew(uint64_t, s->hvs_pixels, count);
        s->hvs_pixels_count = count;
    }
    memset(s->hvs_pixels, 0, count * sizeof(*s->hvs_pixels));
    for (unsigned int i = 0; i < s->hvs_layer_count; i++) {
        const BCM2835FBHVSLayer *layer = &s->hvs_layers[i];
        uint32_t right = MIN((uint64_t)layer->dest_x + layer->dest_width,
                              width);
        uint32_t bottom = MIN((uint64_t)layer->dest_y + layer->dest_height,
                               height);
        bool yuv = layer->format >= 8 && layer->format <= 11;
        HVSImage image;

        if (layer->dest_x >= right || layer->dest_y >= bottom) {
            continue;
        }
        image = yuv ? fb_hvs_convert_yuv(s, layer) : fb_hvs_read_rgb(s, layer);
        if (!image.data) {
            continue;
        }
        if (!yuv) {
            fb_hvs_scale(&image, layer, 0);
        }
        for (uint32_t y = layer->dest_y; y < bottom; y++) {
            for (uint32_t x = layer->dest_x; x < right; x++) {
                uint32_t sx = x - layer->dest_x;
                size_t at = (size_t)y * width + x;
                const uint16_t *rgba;

                if (layer->hflip) {
                    sx = image.width - 1 - sx;
                }
                rgba = image.data +
                       ((size_t)(y - layer->dest_y) * image.width + sx) * 4;
                s->hvs_pixels[at] = fb_hvs_blend_pixel(s->hvs_pixels[at],
                                                     layer, rgba);
            }
        }
        g_free(image.data);
    }
    for (uint32_t y = 0; y < height; y++) {
        uint8_t *dst = surface_data(surface) +
                       (size_t)y * surface_stride(surface);

        for (uint32_t x = 0; x < width; x++) {
            uint64_t pixel = s->hvs_pixels[(size_t)y * width + x];
            uint32_t rgb = ((pixel >> 4) & 255) << 16 |
                           ((pixel >> 16) & 255) << 8 | ((pixel >> 28) & 255);

            fb_hvs_store_pixel(dst + (size_t)x * bytes, bpp, rgb);
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
