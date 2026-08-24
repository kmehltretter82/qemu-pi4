/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/misc/bcm2835_property.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "hw/core/irq.h"
#include "hw/misc/bcm2835_mbox_defs.h"
#include "hw/arm/raspberrypi-fw-defs.h"
#include "system/dma.h"
#include "qemu/bswap.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "trace.h"
#include "hw/arm/raspi4_platform.h"

#define VCHI_BUSADDR_SIZE       sizeof(uint32_t)
#define RPI_EXP_GPIO_BASE       128
#define RPI4_VL805_PCI_DEV_ADDR (1U << 20)
#define RPI_FIRMWARE_DEFAULT_BOARD_SERIAL 0x51454d55
#define RPI_FIRMWARE_MAX_PROPERTY_SIZE MiB

#define RPI_FW_DOMAIN_DEFAULTS \
    (BIT(RPI_FIRMWARE_VIDEO_SCALER_DOMAIN_ID) | \
     BIT(RPI_FIRMWARE_VPU1_DOMAIN_ID) | \
     BIT(RPI_FIRMWARE_USB_DOMAIN_ID) | \
     BIT(RPI_FIRMWARE_TRANSPOSER_DOMAIN_ID) | \
     BIT(RPI_FIRMWARE_ARM_DOMAIN_ID))

/* BCM2711 firmware values captured through /dev/vcio on a Pi 400. */
static const uint32_t rpi4_clock_default_rates[RPI_FIRMWARE_NUM_CLK_ID] = {
    [RPI_FIRMWARE_EMMC_CLK_ID] = 250000000,
    [RPI_FIRMWARE_UART_CLK_ID] = 48000000,
    [RPI_FIRMWARE_ARM_CLK_ID] = 1800000000,
    [RPI_FIRMWARE_CORE_CLK_ID] = 200000000,
    [RPI_FIRMWARE_V3D_CLK_ID] = 250000000,
    [RPI_FIRMWARE_H264_CLK_ID] = 250000000,
    [RPI_FIRMWARE_ISP_CLK_ID] = 250000000,
    [RPI_FIRMWARE_SDRAM_CLK_ID] = 400000000,
    [RPI_FIRMWARE_HEVC_CLK_ID] = 250000000,
    [RPI_FIRMWARE_M2MC_CLK_ID] = 120000000,
    [RPI_FIRMWARE_PIXEL_BVB_CLK_ID] = 75000000,
};

static const uint32_t rpi4_clock_min_rates[RPI_FIRMWARE_NUM_CLK_ID] = {
    [RPI_FIRMWARE_EMMC_CLK_ID] = 250000000,
    [RPI_FIRMWARE_ARM_CLK_ID] = 600000000,
    [RPI_FIRMWARE_CORE_CLK_ID] = 200000000,
    [RPI_FIRMWARE_V3D_CLK_ID] = 250000000,
    [RPI_FIRMWARE_H264_CLK_ID] = 250000000,
    [RPI_FIRMWARE_ISP_CLK_ID] = 250000000,
    [RPI_FIRMWARE_SDRAM_CLK_ID] = 400000000,
    [RPI_FIRMWARE_HEVC_CLK_ID] = 250000000,
    [RPI_FIRMWARE_PIXEL_BVB_CLK_ID] = 75000000,
};

static const uint32_t rpi4_clock_max_rates[RPI_FIRMWARE_NUM_CLK_ID] = {
    [RPI_FIRMWARE_EMMC_CLK_ID] = 250000000,
    [RPI_FIRMWARE_UART_CLK_ID] = 1000000000,
    [RPI_FIRMWARE_ARM_CLK_ID] = 1800000000,
    [RPI_FIRMWARE_CORE_CLK_ID] = 500000000,
    [RPI_FIRMWARE_V3D_CLK_ID] = 500000000,
    [RPI_FIRMWARE_H264_CLK_ID] = 500000000,
    [RPI_FIRMWARE_ISP_CLK_ID] = 500000000,
    [RPI_FIRMWARE_SDRAM_CLK_ID] = 400000000,
    [RPI_FIRMWARE_PIXEL_CLK_ID] = 2400000000,
    [RPI_FIRMWARE_PWM_CLK_ID] = 500000000,
    [RPI_FIRMWARE_HEVC_CLK_ID] = 500000000,
    [RPI_FIRMWARE_EMMC2_CLK_ID] = 500000000,
    [RPI_FIRMWARE_M2MC_CLK_ID] = 600000000,
    [RPI_FIRMWARE_PIXEL_BVB_CLK_ID] = 324000000,
    [RPI_FIRMWARE_VEC_CLK_ID] = 108000000,
};

/* https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface */

static bool bcm2835_property_gpio_index(uint32_t gpio, unsigned int *index)
{
    if (gpio < RPI_EXP_GPIO_BASE ||
        gpio >= RPI_EXP_GPIO_BASE + BCM2835_PROPERTY_GPIO_COUNT) {
        return false;
    }

    *index = gpio - RPI_EXP_GPIO_BASE;
    return true;
}

static bool bcm2835_property_clock_valid(uint32_t id)
{
    /* BCM2711 firmware discovers IDs 1 through 15; DISP (16) is absent. */
    return id > 0 && id < RPI_FIRMWARE_DISP_CLK_ID;
}

static bool bcm2835_property_memory_read(BCM2835PropertyState *s,
                                         hwaddr addr, void *data,
                                         size_t len)
{
    return dma_memory_read(&s->dma_as, addr, data, len,
                           MEMTXATTRS_UNSPECIFIED) == MEMTX_OK;
}

static bool bcm2835_property_memory_write(BCM2835PropertyState *s,
                                          hwaddr addr, const void *data,
                                          size_t len)
{
    return dma_memory_write(&s->dma_as, addr, data, len,
                            MEMTXATTRS_UNSPECIFIED) == MEMTX_OK;
}

static bool bcm2835_property_read_u32(BCM2835PropertyState *s, hwaddr addr,
                                      uint32_t *value)
{
    uint32_t le_value;

    if (!bcm2835_property_memory_read(s, addr, &le_value, sizeof(le_value))) {
        return false;
    }
    *value = le32_to_cpu(le_value);
    return true;
}

static bool bcm2835_property_write_u32(BCM2835PropertyState *s, hwaddr addr,
                                       uint32_t value)
{
    uint32_t le_value = cpu_to_le32(value);

    return bcm2835_property_memory_write(s, addr, &le_value,
                                         sizeof(le_value));
}

static bool bcm2835_property_tag_read(BCM2835PropertyState *s,
                                      hwaddr payload, uint32_t bufsize,
                                      size_t offset, void *data, size_t len)
{
    if (offset > bufsize || len > bufsize - offset) {
        return false;
    }

    return bcm2835_property_memory_read(s, payload + offset, data, len);
}

static bool bcm2835_property_tag_read_u32(BCM2835PropertyState *s,
                                          hwaddr payload, uint32_t bufsize,
                                          size_t offset, uint32_t *value)
{
    uint32_t le_value;

    if (!bcm2835_property_tag_read(s, payload, bufsize, offset, &le_value,
                                   sizeof(le_value))) {
        return false;
    }
    *value = le32_to_cpu(le_value);
    return true;
}

static void bcm2835_property_tag_write(BCM2835PropertyState *s,
                                       hwaddr payload, uint32_t bufsize,
                                       size_t offset, const void *data,
                                       size_t len, bool *error)
{
    size_t writable;

    if (*error || offset >= bufsize || len == 0) {
        return;
    }

    writable = MIN(len, (size_t)bufsize - offset);
    if (!bcm2835_property_memory_write(s, payload + offset, data, writable)) {
        *error = true;
    }
}

static void bcm2835_property_tag_write_u32(BCM2835PropertyState *s,
                                           hwaddr payload, uint32_t bufsize,
                                           size_t offset, uint32_t value,
                                           bool *error)
{
    uint32_t le_value = cpu_to_le32(value);

    bcm2835_property_tag_write(s, payload, bufsize, offset, &le_value,
                               sizeof(le_value), error);
}

static void bcm2835_property_tag_write_u64(BCM2835PropertyState *s,
                                           hwaddr payload, uint32_t bufsize,
                                           size_t offset, uint64_t value,
                                           bool *error)
{
    uint64_t le_value = cpu_to_le64(value);

    bcm2835_property_tag_write(s, payload, bufsize, offset, &le_value,
                               sizeof(le_value), error);
}

static bool bcm2835_property_otp_range_valid(uint32_t start, uint32_t number,
                                             uint32_t row_count)
{
    return start <= row_count && number <= row_count - start;
}

static void bcm2835_property_mbox_push(BCM2835PropertyState *s,
                                       uint32_t mbox_value)
{
    uint32_t tot_len;
    uint32_t request_code;
    uint64_t end;
    hwaddr value;
    bool end_tag_found = false;
    bool parse_error = false;

    /*
     * Copy the current state of the framebuffer config; we will update
     * this copy as we process tags and then ask the framebuffer to use
     * it at the end.
     */
    BCM2835FBConfig fbconfig = s->fbdev->config;
    bool fbconfig_updated = false;

    s->addr = mbox_value & ~0xf;

    if (!bcm2835_property_read_u32(s, s->addr, &tot_len)) {
        return;
    }

    /* Do not write a response code outside the guest-declared header. */
    if (tot_len < 2 * sizeof(uint32_t)) {
        return;
    }
    if (!bcm2835_property_read_u32(s, s->addr + 4, &request_code) ||
        request_code != 0 || tot_len < 3 * sizeof(uint32_t) ||
        tot_len >= RPI_FIRMWARE_MAX_PROPERTY_SIZE ||
        (tot_len & (sizeof(uint32_t) - 1)) ||
        (uint64_t)s->addr + tot_len > (uint64_t)UINT32_MAX + 1) {
        parse_error = true;
        goto response;
    }

    end = (uint64_t)s->addr + tot_len;
    value = s->addr + 8;
    while (!parse_error) {
        uint32_t tag;
        uint32_t bufsize;
        uint32_t tag_code;
        uint32_t resplen = 0;
        uint64_t padded;
        uint64_t next;
        hwaddr payload;
        bool handled = true;
        bool tag_error = false;

        if (value > end || end - value < sizeof(tag) ||
            !bcm2835_property_read_u32(s, value, &tag)) {
            parse_error = true;
            break;
        }
        if (tag == RPI_FWREQ_PROPERTY_END) {
            end_tag_found = true;
            break;
        }
        if (end - value < sizeof(rpi_firmware_prop_request_t) ||
            !bcm2835_property_read_u32(s, value + 4, &bufsize) ||
            !bcm2835_property_read_u32(s, value + 8, &tag_code)) {
            parse_error = true;
            break;
        }

        padded = ((uint64_t)bufsize + sizeof(uint32_t) - 1) &
                 ~(uint64_t)(sizeof(uint32_t) - 1);
        next = value + sizeof(rpi_firmware_prop_request_t) + padded;
        if (tag_code != 0 || next > end - sizeof(uint32_t)) {
            parse_error = true;
            break;
        }
        payload = value + sizeof(rpi_firmware_prop_request_t);

        /* @(value + 8) : Request/response indicator */
        switch (tag) {
        case RPI_FWREQ_PROPERTY_END:
            handled = false;
            break;
        case RPI_FWREQ_GET_FIRMWARE_REVISION:
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 346337,
                                           &tag_error);
            break;
        case RPI_FWREQ_GET_BOARD_MODEL:
            /*
             * Pi 4-family firmware reports model zero; revision identifies
             * the board.
             */
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            break;
        case RPI_FWREQ_GET_BOARD_REVISION:
            resplen = 4;
            bcm2835_property_tag_write_u32(
                s, payload, bufsize, 0,
                bcm2835_otp_get_row(s->otp, BCM2835_OTP_BOARD_REVISION),
                &tag_error);
            break;
        case RPI_FWREQ_GET_BOARD_MAC_ADDRESS:
            resplen = sizeof(s->macaddr.a);
            bcm2835_property_tag_write(s, payload, bufsize, 0, s->macaddr.a,
                                       resplen, &tag_error);
            break;
        case RPI_FWREQ_GET_BOARD_SERIAL:
            resplen = 8;
            bcm2835_property_tag_write_u64(
                s, payload, bufsize, 0,
                bcm2835_otp_get_row(s->otp, BCM2835_OTP_SERIAL_NUMBER),
                &tag_error);
            break;
        case RPI_FWREQ_GET_ARM_MEMORY:
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           s->fbdev->vcram_base,
                                           &tag_error);
            break;
        case RPI_FWREQ_GET_VC_MEMORY:
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           s->fbdev->vcram_base,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           s->fbdev->vcram_size,
                                           &tag_error);
            break;
        case RPI_FWREQ_SET_POWER_STATE:
        {
            uint32_t device;
            uint32_t state;

            /*
             * Assume that whatever device they asked for exists,
             * and we'll just claim we set it to the desired state.
             */
            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &device) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &state)) {
                tag_error = true;
                break;
            }
            state &= RPI_FIRMWARE_STATE_ENABLE;
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, device,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4, state,
                                           &tag_error);
            break;
        }

        /* Clocks */

        case RPI_FWREQ_GET_CLOCK_STATE:
        case RPI_FWREQ_SET_CLOCK_STATE:
        {
            uint32_t id;
            uint32_t state;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &id)) {
                tag_error = true;
                break;
            }
            if (tag == RPI_FWREQ_SET_CLOCK_STATE &&
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &state)) {
                tag_error = true;
                break;
            }
            if (!bcm2835_property_clock_valid(id)) {
                state = RPI_FIRMWARE_STATE_NOT_EXIST;
            } else {
                if (tag == RPI_FWREQ_SET_CLOCK_STATE) {
                    s->clock_states = deposit32(s->clock_states, id, 1,
                                                state &
                                                RPI_FIRMWARE_STATE_ENABLE);
                }
                state = extract32(s->clock_states, id, 1);
            }
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, id,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4, state,
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_GET_CLOCK_RATE:
        case RPI_FWREQ_GET_MAX_CLOCK_RATE:
        case RPI_FWREQ_GET_MIN_CLOCK_RATE:
        {
            uint32_t id;
            uint32_t rate = 0;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &id)) {
                tag_error = true;
                break;
            }
            if (bcm2835_property_clock_valid(id)) {
                switch (tag) {
                case RPI_FWREQ_GET_CLOCK_RATE:
                    rate = s->clock_rates[id];
                    break;
                case RPI_FWREQ_GET_MAX_CLOCK_RATE:
                    rate = rpi4_clock_max_rates[id];
                    break;
                case RPI_FWREQ_GET_MIN_CLOCK_RATE:
                    rate = rpi4_clock_min_rates[id];
                    break;
                default:
                    g_assert_not_reached();
                }
            }
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, id,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4, rate,
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_GET_CLOCKS:
        {
            unsigned int id;

            /* Each response entry is a parent/clock-ID pair. */
            resplen = (RPI_FIRMWARE_DISP_CLK_ID - 1) *
                      2 * sizeof(uint32_t);
            for (id = 1; id < RPI_FIRMWARE_DISP_CLK_ID; id++) {
                size_t offset = (id - 1) * 2 * sizeof(uint32_t);

                bcm2835_property_tag_write_u32(s, payload, bufsize, offset,
                                               0, &tag_error);
                bcm2835_property_tag_write_u32(s, payload, bufsize,
                                               offset + sizeof(uint32_t), id,
                                               &tag_error);
            }
            break;
        }

        case RPI_FWREQ_SET_CLOCK_RATE:
        {
            uint32_t id;
            uint32_t rate = 0;
            uint32_t skip_turbo;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &id) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &rate) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 8,
                                               &skip_turbo)) {
                tag_error = true;
                break;
            }
            (void)skip_turbo;
            if (bcm2835_property_clock_valid(id)) {
                rate = MIN(rate, rpi4_clock_max_rates[id]);
                rate = MAX(rate, rpi4_clock_min_rates[id]);
                s->clock_rates[id] = rate;
            } else {
                rate = 0;
            }
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, id,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4, rate,
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_SET_MAX_CLOCK_RATE:
        case RPI_FWREQ_SET_MIN_CLOCK_RATE:
            qemu_log_mask(LOG_UNIMP,
                          "bcm2835_property: 0x%08x set clock rate NYI\n",
                          tag);
            handled = false;
            break;

        /* Temperature */

        case RPI_FWREQ_GET_TEMPERATURE:
        case RPI_FWREQ_GET_MAX_TEMPERATURE:
        {
            uint32_t id;
            uint32_t temperature = tag == RPI_FWREQ_GET_TEMPERATURE ?
                                   25000 : 99000;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &id)) {
                tag_error = true;
                break;
            }
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, id,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           temperature, &tag_error);
            break;
        }

        /* Frame buffer */

        case RPI_FWREQ_FRAMEBUFFER_ALLOCATE:
        {
            uint32_t alignment;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &alignment)) {
                tag_error = true;
                break;
            }
            (void)alignment;
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.base, &tag_error);
            bcm2835_property_tag_write_u32(
                s, payload, bufsize, 4, bcm2835_fb_get_size(&fbconfig),
                &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_RELEASE:
            resplen = 0;
            break;
        case RPI_FWREQ_FRAMEBUFFER_BLANK:
        case RPI_FWREQ_FRAMEBUFFER_TEST_DEPTH:
        case RPI_FWREQ_FRAMEBUFFER_TEST_PIXEL_ORDER:
        case RPI_FWREQ_FRAMEBUFFER_TEST_ALPHA_MODE:
        {
            uint32_t requested;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &requested)) {
                tag_error = true;
                break;
            }
            (void)requested;
            resplen = 4;
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_TEST_PHYSICAL_WIDTH_HEIGHT:
        case RPI_FWREQ_FRAMEBUFFER_TEST_VIRTUAL_WIDTH_HEIGHT:
        case RPI_FWREQ_FRAMEBUFFER_TEST_VIRTUAL_OFFSET:
        {
            uint32_t first;
            uint32_t second;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &first) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &second)) {
                tag_error = true;
                break;
            }
            (void)first;
            (void)second;
            resplen = 8;
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT:
        {
            uint32_t xres;
            uint32_t yres;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &xres) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &yres)) {
                tag_error = true;
                break;
            }
            fbconfig.xres = xres;
            fbconfig.yres = yres;
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.xres, &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           fbconfig.yres, &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_PHYSICAL_WIDTH_HEIGHT:
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.xres, &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           fbconfig.yres, &tag_error);
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT:
        {
            uint32_t xres;
            uint32_t yres;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &xres) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &yres)) {
                tag_error = true;
                break;
            }
            fbconfig.xres_virtual = xres;
            fbconfig.yres_virtual = yres;
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.xres_virtual,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           fbconfig.yres_virtual,
                                           &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_VIRTUAL_WIDTH_HEIGHT:
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.xres_virtual,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           fbconfig.yres_virtual,
                                           &tag_error);
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_DEPTH:
        {
            uint32_t depth;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &depth)) {
                tag_error = true;
                break;
            }
            fbconfig.bpp = depth;
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.bpp, &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_DEPTH:
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.bpp, &tag_error);
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_PIXEL_ORDER:
        {
            uint32_t pixel_order;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &pixel_order)) {
                tag_error = true;
                break;
            }
            fbconfig.pixo = pixel_order;
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.pixo, &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_PIXEL_ORDER:
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.pixo, &tag_error);
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_ALPHA_MODE:
        {
            uint32_t alpha;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &alpha)) {
                tag_error = true;
                break;
            }
            fbconfig.alpha = alpha;
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.alpha, &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_ALPHA_MODE:
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.alpha, &tag_error);
            break;
        case RPI_FWREQ_FRAMEBUFFER_GET_PITCH:
            resplen = 4;
            bcm2835_property_tag_write_u32(
                s, payload, bufsize, 0, bcm2835_fb_get_pitch(&fbconfig),
                &tag_error);
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_OFFSET:
        {
            uint32_t xoffset;
            uint32_t yoffset;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &xoffset) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &yoffset)) {
                tag_error = true;
                break;
            }
            fbconfig.xoffset = xoffset;
            fbconfig.yoffset = yoffset;
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.xoffset, &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           fbconfig.yoffset, &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_VIRTUAL_OFFSET:
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           fbconfig.xoffset, &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           fbconfig.yoffset, &tag_error);
            break;
        case RPI_FWREQ_FRAMEBUFFER_GET_OVERSCAN:
        case RPI_FWREQ_FRAMEBUFFER_TEST_OVERSCAN:
        case RPI_FWREQ_FRAMEBUFFER_SET_OVERSCAN:
            if (tag != RPI_FWREQ_FRAMEBUFFER_GET_OVERSCAN) {
                uint32_t requested;

                for (size_t i = 0; i < 4; i++) {
                    if (!bcm2835_property_tag_read_u32(
                            s, payload, bufsize, i * sizeof(uint32_t),
                            &requested)) {
                        tag_error = true;
                        break;
                    }
                }
            }
            resplen = 16;
            for (size_t i = 0; i < 4; i++) {
                bcm2835_property_tag_write_u32(
                    s, payload, bufsize, i * sizeof(uint32_t), 0,
                    &tag_error);
            }
            break;
        case RPI_FWREQ_FRAMEBUFFER_GET_PALETTE:
        {
            uint32_t colors[256];

            for (size_t i = 0; i < G_N_ELEMENTS(colors); i++) {
                if (!bcm2835_property_read_u32(
                        s, s->fbdev->vcram_base + i * sizeof(uint32_t),
                        &colors[i])) {
                    tag_error = true;
                    break;
                }
            }
            resplen = sizeof(colors);
            if (!tag_error) {
                for (size_t i = 0; i < G_N_ELEMENTS(colors); i++) {
                    bcm2835_property_tag_write_u32(
                        s, payload, bufsize, i * sizeof(uint32_t), colors[i],
                        &tag_error);
                }
            }
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_TEST_PALETTE:
        case RPI_FWREQ_FRAMEBUFFER_SET_PALETTE:
        {
            uint32_t colors[256];
            uint32_t offset;
            uint32_t length;
            uint32_t resp = 1;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &offset) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &length)) {
                tag_error = true;
                break;
            }
            if (bufsize >= 6 * sizeof(uint32_t) &&
                length > 0 && offset < G_N_ELEMENTS(colors) &&
                length <= G_N_ELEMENTS(colors) - offset &&
                bufsize >= 2 * sizeof(uint32_t) +
                           length * sizeof(uint32_t)) {
                for (size_t i = 0; i < length; i++) {
                    if (!bcm2835_property_tag_read_u32(
                            s, payload, bufsize,
                            (i + 2) * sizeof(uint32_t), &colors[i])) {
                        tag_error = true;
                        break;
                    }
                }
                if (!tag_error) {
                    resp = 0;
                }
            }
            if (resp == 0 && tag == RPI_FWREQ_FRAMEBUFFER_SET_PALETTE) {
                for (size_t i = 0; i < length; i++) {
                    if (!bcm2835_property_write_u32(
                            s, s->fbdev->vcram_base +
                               (offset + i) * sizeof(uint32_t), colors[i])) {
                        tag_error = true;
                        break;
                    }
                }
            }
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, resp,
                                           &tag_error);
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_NUM_DISPLAYS:
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 1,
                                           &tag_error);
            break;

        case RPI_FWREQ_GET_DMA_CHANNELS:
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           s->dma_channels, &tag_error);
            break;

        case RPI_FWREQ_NOTIFY_XHCI_RESET:
        {
            uint32_t dev_addr;
            uint32_t status;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &dev_addr)) {
                tag_error = true;
                break;
            }

            /*
             * Linux passes the hard-wired VL805 address using the firmware
             * encoding PCI_BUS << 20 | PCI_SLOT << 15 | PCI_FUNC << 12.
             * Despite its reset-controller API, this property call notifies
             * the firmware after PCI reset so it can initialize the VL805;
             * the call does not itself reset the xHCI register file.
             */
            if (s->has_vl805 && dev_addr == RPI4_VL805_PCI_DEV_ADDR) {
                status = 0;
            } else {
                status = UINT32_MAX;
            }
            resplen = sizeof(dev_addr);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, status,
                                           &tag_error);
            if (!tag_error && status == 0) {
                qemu_irq_pulse(s->xhci_notify);
            }
            break;
        }

        case RPI_FWREQ_GET_COMMAND_LINE:
        {
            size_t command_line_len = s->command_line ?
                                      strlen(s->command_line) : 0;

            /*
             * We follow the firmware behaviour: no NUL terminator is
             * written to the buffer, and if the buffer is too short
             * we report the required length in the response header
             * and copy nothing to the buffer.
             */
            if (command_line_len > INT32_MAX) {
                tag_error = true;
                break;
            }
            resplen = command_line_len;
            if (bufsize >= resplen) {
                bcm2835_property_tag_write(s, payload, bufsize, 0,
                                           s->command_line, resplen,
                                           &tag_error);
            }
            break;
        }

        case RPI_FWREQ_GET_THROTTLED:
            resplen = 4;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            break;

        /* Firmware-managed power domains */

        case RPI_FWREQ_GET_DOMAIN_STATE:
        case RPI_FWREQ_SET_DOMAIN_STATE:
        {
            uint32_t id;
            uint32_t state;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &id)) {
                tag_error = true;
                break;
            }
            if (tag == RPI_FWREQ_SET_DOMAIN_STATE &&
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &state)) {
                tag_error = true;
                break;
            }
            if (id == 0 || id >= RPI_FIRMWARE_NUM_DOMAIN_ID) {
                state = 0;
            } else {
                if (tag == RPI_FWREQ_SET_DOMAIN_STATE) {
                    s->domain_states = deposit32(s->domain_states, id, 1,
                                                 !!state);
                }
                state = extract32(s->domain_states, id, 1);
            }
            resplen = 8;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, id,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4, state,
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_NOTIFY_REBOOT:
            /* There is no VideoCore firmware state to quiesce in QEMU. */
            resplen = 0;
            break;

        /* Firmware-controlled GPIO expander */

        case RPI_FWREQ_GET_GPIO_CONFIG:
        {
            uint32_t gpio;
            unsigned int index;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &gpio)) {
                tag_error = true;
                break;
            }
            resplen = 20;
            if (!bcm2835_property_gpio_index(gpio, &index)) {
                break;
            }
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           s->gpio_direction[index],
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 8,
                                           s->gpio_polarity[index],
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 12,
                                           s->gpio_term_en[index],
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 16,
                                           s->gpio_term_pull_up[index],
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_SET_GPIO_CONFIG:
        {
            uint32_t values[6];
            unsigned int index;

            for (size_t i = 0; i < G_N_ELEMENTS(values); i++) {
                if (!bcm2835_property_tag_read_u32(
                        s, payload, bufsize, i * sizeof(uint32_t),
                        &values[i])) {
                    tag_error = true;
                    break;
                }
            }
            resplen = 24;
            if (tag_error ||
                !bcm2835_property_gpio_index(values[0], &index)) {
                break;
            }
            s->gpio_direction[index] = values[1];
            s->gpio_polarity[index] = values[2];
            s->gpio_term_en[index] = values[3];
            s->gpio_term_pull_up[index] = values[4];
            s->gpio_state[index] = values[5];
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_GET_GPIO_STATE:
        {
            uint32_t gpio;
            unsigned int index;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &gpio)) {
                tag_error = true;
                break;
            }
            resplen = 8;
            if (!bcm2835_property_gpio_index(gpio, &index)) {
                break;
            }
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4,
                                           s->gpio_state[index],
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_SET_GPIO_STATE:
        {
            uint32_t gpio;
            uint32_t state;
            unsigned int index;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &gpio) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &state)) {
                tag_error = true;
                break;
            }
            resplen = 8;
            if (!bcm2835_property_gpio_index(gpio, &index)) {
                break;
            }
            s->gpio_state[index] = state;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            break;
        }

        case RPI_FWREQ_VCHIQ_INIT:
            resplen = VCHI_BUSADDR_SIZE;
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0, 0,
                                           &tag_error);
            break;

        /* Customer OTP */

        case RPI_FWREQ_GET_CUSTOMER_OTP:
        {
            uint32_t start_num;
            uint32_t number;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &start_num) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &number)) {
                tag_error = true;
                break;
            }
            resplen = 2 * sizeof(uint32_t);
            if (!bcm2835_property_otp_range_valid(
                    start_num, number, BCM2835_OTP_CUSTOMER_OTP_LEN)) {
                break;
            }
            resplen += number * sizeof(uint32_t);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           start_num, &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4, number,
                                           &tag_error);
            for (uint32_t n = 0; n < number; n++) {
                uint32_t otp_row = bcm2835_otp_get_row(s->otp,
                    BCM2835_OTP_CUSTOMER_OTP + start_num + n);

                bcm2835_property_tag_write_u32(
                    s, payload, bufsize, (n + 2) * sizeof(uint32_t), otp_row,
                    &tag_error);
            }
            break;
        }
        case RPI_FWREQ_SET_CUSTOMER_OTP:
        {
            uint32_t rows[BCM2835_OTP_CUSTOMER_OTP_LEN];
            uint32_t start_num;
            uint32_t number;

            resplen = 4;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &start_num) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &number)) {
                tag_error = true;
                break;
            }

            /* Magic numbers to permanently lock customer OTP */
            if (start_num == BCM2835_OTP_LOCK_NUM1 &&
                number == BCM2835_OTP_LOCK_NUM2) {
                bcm2835_otp_set_row(s->otp,
                                    BCM2835_OTP_ROW_32,
                                    BCM2835_OTP_ROW_32_LOCK);
                break;
            }

            if (!bcm2835_property_otp_range_valid(
                    start_num, number, BCM2835_OTP_CUSTOMER_OTP_LEN)) {
                break;
            }

            /* If row 32 has the lock bit, don't allow further writes */
            if (bcm2835_otp_get_row(s->otp, BCM2835_OTP_ROW_32) &
                                    BCM2835_OTP_ROW_32_LOCK) {
                break;
            }

            for (uint32_t n = 0; n < number; n++) {
                if (!bcm2835_property_tag_read_u32(
                        s, payload, bufsize, (n + 2) * sizeof(uint32_t),
                        &rows[n])) {
                    tag_error = true;
                    break;
                }
            }
            if (tag_error) {
                break;
            }
            for (uint32_t n = 0; n < number; n++) {
                bcm2835_otp_set_row(s->otp,
                                    BCM2835_OTP_CUSTOMER_OTP + start_num + n,
                                    rows[n]);
            }
            break;
        }

        /* Device-specific private key */
        case RPI_FWREQ_GET_PRIVATE_KEY:
        {
            uint32_t start_num;
            uint32_t number;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &start_num) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &number)) {
                tag_error = true;
                break;
            }
            resplen = 2 * sizeof(uint32_t);
            if (!bcm2835_property_otp_range_valid(
                    start_num, number, BCM2835_OTP_PRIVATE_KEY_LEN)) {
                break;
            }
            resplen += number * sizeof(uint32_t);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 0,
                                           start_num, &tag_error);
            bcm2835_property_tag_write_u32(s, payload, bufsize, 4, number,
                                           &tag_error);
            for (uint32_t n = 0; n < number; n++) {
                uint32_t otp_row = bcm2835_otp_get_row(s->otp,
                    BCM2835_OTP_PRIVATE_KEY + start_num + n);

                bcm2835_property_tag_write_u32(
                    s, payload, bufsize, (n + 2) * sizeof(uint32_t), otp_row,
                    &tag_error);
            }
            break;
        }
        case RPI_FWREQ_SET_PRIVATE_KEY:
        {
            uint32_t rows[BCM2835_OTP_PRIVATE_KEY_LEN];
            uint32_t start_num;
            uint32_t number;

            resplen = 4;

            if (!bcm2835_property_tag_read_u32(s, payload, bufsize, 0,
                                               &start_num) ||
                !bcm2835_property_tag_read_u32(s, payload, bufsize, 4,
                                               &number)) {
                tag_error = true;
                break;
            }
            if (!bcm2835_property_otp_range_valid(
                    start_num, number, BCM2835_OTP_PRIVATE_KEY_LEN)) {
                break;
            }

            /* If row 32 has the lock bit, don't allow further writes */
            if (bcm2835_otp_get_row(s->otp, BCM2835_OTP_ROW_32) &
                                    BCM2835_OTP_ROW_32_LOCK) {
                break;
            }

            for (uint32_t n = 0; n < number; n++) {
                if (!bcm2835_property_tag_read_u32(
                        s, payload, bufsize, (n + 2) * sizeof(uint32_t),
                        &rows[n])) {
                    tag_error = true;
                    break;
                }
            }
            if (tag_error) {
                break;
            }
            for (uint32_t n = 0; n < number; n++) {
                bcm2835_otp_set_row(s->otp,
                                    BCM2835_OTP_PRIVATE_KEY + start_num + n,
                                    rows[n]);
            }
            break;
        }
        default:
            qemu_log_mask(LOG_UNIMP,
                          "bcm2835_property: unhandled tag 0x%08x\n", tag);
            handled = false;
            break;
        }

        trace_bcm2835_mbox_property(tag, bufsize, resplen);
        if (tag_error) {
            parse_error = true;
            break;
        }
        if (handled &&
            !bcm2835_property_write_u32(s, value + 8,
                                        (1U << 31) | resplen)) {
            parse_error = true;
            break;
        }
        value = next;
    }

    if (!end_tag_found) {
        parse_error = true;
    }

response:
    /* Reconfigure framebuffer if required */
    if (fbconfig_updated) {
        bcm2835_fb_reconfigure(s->fbdev, &fbconfig);
    }

    /* Buffer response code */
    bcm2835_property_write_u32(s, s->addr + 4,
                               parse_error ? 0x80000001 : 0x80000000);
}

static uint64_t bcm2835_property_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    BCM2835PropertyState *s = opaque;
    uint32_t res = 0;

    switch (offset) {
    case MBOX_AS_DATA:
        res = MBOX_CHAN_PROPERTY | s->addr;
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

static void bcm2835_property_write(void *opaque, hwaddr offset,
                                   uint64_t value, unsigned size)
{
    BCM2835PropertyState *s = opaque;

    switch (offset) {
    case MBOX_AS_DATA:
        /* bcm2835_mbox should check our pending status before pushing */
        assert(!s->pending);
        s->pending = true;
        bcm2835_property_mbox_push(s, value);
        qemu_set_irq(s->mbox_irq, 1);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %"HWADDR_PRIx"\n",
                      __func__, offset);
        return;
    }
}

static const MemoryRegionOps bcm2835_property_ops = {
    .read = bcm2835_property_read,
    .write = bcm2835_property_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const VMStateDescription vmstate_bcm2835_property = {
    .name = TYPE_BCM2835_PROPERTY,
    .version_id = 4,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_MACADDR(macaddr, BCM2835PropertyState),
        VMSTATE_UINT32(addr, BCM2835PropertyState),
        VMSTATE_BOOL(pending, BCM2835PropertyState),
        VMSTATE_UINT32_V(clock_states, BCM2835PropertyState, 3),
        VMSTATE_UINT32_ARRAY_V(clock_rates, BCM2835PropertyState,
                               RPI_FIRMWARE_NUM_CLK_ID, 4),
        VMSTATE_UINT32_V(domain_states, BCM2835PropertyState, 3),
        VMSTATE_UINT32_ARRAY_V(gpio_direction, BCM2835PropertyState,
                               BCM2835_PROPERTY_GPIO_COUNT, 2),
        VMSTATE_UINT32_ARRAY_V(gpio_polarity, BCM2835PropertyState,
                               BCM2835_PROPERTY_GPIO_COUNT, 2),
        VMSTATE_UINT32_ARRAY_V(gpio_term_en, BCM2835PropertyState,
                               BCM2835_PROPERTY_GPIO_COUNT, 2),
        VMSTATE_UINT32_ARRAY_V(gpio_term_pull_up, BCM2835PropertyState,
                               BCM2835_PROPERTY_GPIO_COUNT, 2),
        VMSTATE_UINT32_ARRAY_V(gpio_state, BCM2835PropertyState,
                               BCM2835_PROPERTY_GPIO_COUNT, 2),
        VMSTATE_END_OF_LIST()
    }
};

static void bcm2835_property_init(Object *obj)
{
    BCM2835PropertyState *s = BCM2835_PROPERTY(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &bcm2835_property_ops, s,
                          TYPE_BCM2835_PROPERTY, 0x10);

    /*
     * bcm2835_property_ops call into bcm2835_mbox, which in-turn reads from
     * iomem. As such, mark iomem as re-entracy safe.
     */
    s->iomem.disable_reentrancy_guard = true;

    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->mbox_irq);
    qdev_init_gpio_out_named(DEVICE(s), &s->xhci_notify,
                             BCM2835_PROPERTY_XHCI_NOTIFY, 1);
}

static void bcm2835_property_reset(DeviceState *dev)
{
    BCM2835PropertyState *s = BCM2835_PROPERTY(dev);

    s->pending = false;
    qemu_set_irq(s->mbox_irq, 0);
    s->clock_states = MAKE_64BIT_MASK(1, RPI_FIRMWARE_DISP_CLK_ID - 1);
    memcpy(s->clock_rates, rpi4_clock_default_rates,
           sizeof(s->clock_rates));
    s->domain_states = RPI_FW_DOMAIN_DEFAULTS;
    memset(s->gpio_direction, 0, sizeof(s->gpio_direction));
    memset(s->gpio_polarity, 0, sizeof(s->gpio_polarity));
    memset(s->gpio_term_en, 0, sizeof(s->gpio_term_en));
    memset(s->gpio_term_pull_up, 0, sizeof(s->gpio_term_pull_up));
    memset(s->gpio_state, 0, sizeof(s->gpio_state));
}

static void bcm2835_property_realize(DeviceState *dev, Error **errp)
{
    BCM2835PropertyState *s = BCM2835_PROPERTY(dev);
    Object *obj;

    obj = object_property_get_link(OBJECT(dev), "fb", &error_abort);
    s->fbdev = BCM2835_FB(obj);

    obj = object_property_get_link(OBJECT(dev), "dma-mr", &error_abort);
    s->dma_mr = MEMORY_REGION(obj);
    address_space_init(&s->dma_as, s->dma_mr, TYPE_BCM2835_PROPERTY "-memory");

    obj = object_property_get_link(OBJECT(dev), "otp", &error_abort);
    s->otp = BCM2835_OTP(obj);
    bcm2835_otp_set_board_identity(s->otp, s->board_serial, s->board_rev);

    /* TODO: connect to MAC address of USB NIC device, once we emulate it */
    qemu_macaddr_default_if_unset(&s->macaddr);

    bcm2835_property_reset(dev);
}

static const Property bcm2835_property_props[] = {
    DEFINE_PROP_UINT32("board-rev", BCM2835PropertyState, board_rev, 0),
    DEFINE_PROP_UINT32("board-serial", BCM2835PropertyState, board_serial,
                       RPI_FIRMWARE_DEFAULT_BOARD_SERIAL),
    DEFINE_PROP_UINT32("dma-channels", BCM2835PropertyState, dma_channels,
                       0x003c),
    DEFINE_PROP_BOOL("has-vl805", BCM2835PropertyState, has_vl805, false),
    DEFINE_PROP_STRING("command-line", BCM2835PropertyState, command_line),
};

static void bcm2835_property_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, bcm2835_property_props);
    device_class_set_legacy_reset(dc, bcm2835_property_reset);
    dc->realize = bcm2835_property_realize;
    dc->vmsd = &vmstate_bcm2835_property;
}

static const TypeInfo bcm2835_property_info = {
    .name          = TYPE_BCM2835_PROPERTY,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2835PropertyState),
    .class_init    = bcm2835_property_class_init,
    .instance_init = bcm2835_property_init,
};

static void bcm2835_property_register_types(void)
{
    type_register_static(&bcm2835_property_info);
}

type_init(bcm2835_property_register_types)
