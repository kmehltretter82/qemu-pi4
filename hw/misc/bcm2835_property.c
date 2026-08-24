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
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"
#include "hw/arm/raspi4_platform.h"

#define VCHI_BUSADDR_SIZE       sizeof(uint32_t)
#define RPI_EXP_GPIO_BASE       128
#define RPI4_VL805_PCI_DEV_ADDR (1U << 20)
#define RPI_FIRMWARE_DEFAULT_BOARD_SERIAL 0x51454d55

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

static void bcm2835_property_mbox_push(BCM2835PropertyState *s, uint32_t value)
{
    uint32_t tot_len;

    /*
     * Copy the current state of the framebuffer config; we will update
     * this copy as we process tags and then ask the framebuffer to use
     * it at the end.
     */
    BCM2835FBConfig fbconfig = s->fbdev->config;
    bool fbconfig_updated = false;

    value &= ~0xf;

    s->addr = value;

    tot_len = ldl_le_phys(&s->dma_as, value);

    /* @(addr + 4) : Buffer response code */
    value = s->addr + 8;
    while (value + 8 <= s->addr + tot_len) {
        uint32_t tag = ldl_le_phys(&s->dma_as, value);
        uint32_t bufsize = ldl_le_phys(&s->dma_as, value + 4);
        /* @(value + 8) : Request/response indicator */
        size_t resplen = 0;
        switch (tag) {
        case RPI_FWREQ_PROPERTY_END:
            break;
        case RPI_FWREQ_GET_FIRMWARE_REVISION:
            stl_le_phys(&s->dma_as, value + 12, 346337);
            resplen = 4;
            break;
        case RPI_FWREQ_GET_BOARD_MODEL:
            qemu_log_mask(LOG_UNIMP,
                          "bcm2835_property: 0x%08x get board model NYI\n",
                          tag);
            resplen = 4;
            break;
        case RPI_FWREQ_GET_BOARD_REVISION:
            if (bufsize < sizeof(uint32_t)) {
                break;
            }
            stl_le_phys(&s->dma_as, value + 12,
                        bcm2835_otp_get_row(s->otp,
                                            BCM2835_OTP_BOARD_REVISION));
            resplen = 4;
            break;
        case RPI_FWREQ_GET_BOARD_MAC_ADDRESS:
            resplen = sizeof(s->macaddr.a);
            dma_memory_write(&s->dma_as, value + 12, s->macaddr.a, resplen,
                             MEMTXATTRS_UNSPECIFIED);
            break;
        case RPI_FWREQ_GET_BOARD_SERIAL:
            if (bufsize < sizeof(uint64_t)) {
                break;
            }
            stq_le_phys(&s->dma_as, value + 12,
                        bcm2835_otp_get_row(s->otp,
                                            BCM2835_OTP_SERIAL_NUMBER));
            resplen = 8;
            break;
        case RPI_FWREQ_GET_ARM_MEMORY:
            /* base */
            stl_le_phys(&s->dma_as, value + 12, 0);
            /* size */
            stl_le_phys(&s->dma_as, value + 16, s->fbdev->vcram_base);
            resplen = 8;
            break;
        case RPI_FWREQ_GET_VC_MEMORY:
            /* base */
            stl_le_phys(&s->dma_as, value + 12, s->fbdev->vcram_base);
            /* size */
            stl_le_phys(&s->dma_as, value + 16, s->fbdev->vcram_size);
            resplen = 8;
            break;
        case RPI_FWREQ_SET_POWER_STATE:
        {
            /*
             * Assume that whatever device they asked for exists,
             * and we'll just claim we set it to the desired state.
             */
            uint32_t state = ldl_le_phys(&s->dma_as, value + 16);
            stl_le_phys(&s->dma_as, value + 16, (state & 1));
            resplen = 8;
            break;
        }

        /* Clocks */

        case RPI_FWREQ_GET_CLOCK_STATE:
        case RPI_FWREQ_SET_CLOCK_STATE:
        {
            uint32_t id;
            uint32_t state;

            if (bufsize < 8) {
                break;
            }

            id = ldl_le_phys(&s->dma_as, value + 12);
            if (!bcm2835_property_clock_valid(id)) {
                state = RPI_FIRMWARE_STATE_NOT_EXIST;
            } else {
                if (tag == RPI_FWREQ_SET_CLOCK_STATE) {
                    state = ldl_le_phys(&s->dma_as, value + 16);
                    s->clock_states = deposit32(s->clock_states, id, 1,
                                                state &
                                                RPI_FIRMWARE_STATE_ENABLE);
                }
                state = extract32(s->clock_states, id, 1);
            }
            stl_le_phys(&s->dma_as, value + 16, state);
            resplen = 8;
            break;
        }

        case RPI_FWREQ_GET_CLOCK_RATE:
        case RPI_FWREQ_GET_MAX_CLOCK_RATE:
        case RPI_FWREQ_GET_MIN_CLOCK_RATE:
        {
            uint32_t id = ldl_le_phys(&s->dma_as, value + 12);
            uint32_t rate = 0;

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
            stl_le_phys(&s->dma_as, value + 16, rate);
            resplen = 8;
            break;
        }

        case RPI_FWREQ_GET_CLOCKS:
        {
            size_t entries = bufsize / (2 * sizeof(uint32_t));
            unsigned int id;

            /* Each response entry is a parent/clock-ID pair. */
            for (id = 1; id < RPI_FIRMWARE_DISP_CLK_ID && entries; id++) {
                stl_le_phys(&s->dma_as, value + 12 + resplen, 0);
                stl_le_phys(&s->dma_as, value + 16 + resplen, id);
                resplen += 2 * sizeof(uint32_t);
                entries--;
            }
            break;
        }

        case RPI_FWREQ_SET_CLOCK_RATE:
        {
            uint32_t id = ldl_le_phys(&s->dma_as, value + 12);
            uint32_t rate = 0;

            if (bufsize < 8) {
                break;
            }
            if (bcm2835_property_clock_valid(id)) {
                rate = ldl_le_phys(&s->dma_as, value + 16);
                rate = MIN(rate, rpi4_clock_max_rates[id]);
                rate = MAX(rate, rpi4_clock_min_rates[id]);
                s->clock_rates[id] = rate;
            }
            stl_le_phys(&s->dma_as, value + 16, rate);
            resplen = 8;
            break;
        }

        case RPI_FWREQ_SET_MAX_CLOCK_RATE:
        case RPI_FWREQ_SET_MIN_CLOCK_RATE:
            qemu_log_mask(LOG_UNIMP,
                          "bcm2835_property: 0x%08x set clock rate NYI\n",
                          tag);
            resplen = 8;
            break;

        /* Temperature */

        case RPI_FWREQ_GET_TEMPERATURE:
            stl_le_phys(&s->dma_as, value + 16, 25000);
            resplen = 8;
            break;

        case RPI_FWREQ_GET_MAX_TEMPERATURE:
            stl_le_phys(&s->dma_as, value + 16, 99000);
            resplen = 8;
            break;

        /* Frame buffer */

        case RPI_FWREQ_FRAMEBUFFER_ALLOCATE:
            stl_le_phys(&s->dma_as, value + 12, fbconfig.base);
            stl_le_phys(&s->dma_as, value + 16,
                        bcm2835_fb_get_size(&fbconfig));
            resplen = 8;
            break;
        case RPI_FWREQ_FRAMEBUFFER_RELEASE:
            resplen = 0;
            break;
        case RPI_FWREQ_FRAMEBUFFER_BLANK:
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_TEST_PHYSICAL_WIDTH_HEIGHT:
        case RPI_FWREQ_FRAMEBUFFER_TEST_VIRTUAL_WIDTH_HEIGHT:
            resplen = 8;
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT:
            fbconfig.xres = ldl_le_phys(&s->dma_as, value + 12);
            fbconfig.yres = ldl_le_phys(&s->dma_as, value + 16);
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            /* fall through */
        case RPI_FWREQ_FRAMEBUFFER_GET_PHYSICAL_WIDTH_HEIGHT:
            stl_le_phys(&s->dma_as, value + 12, fbconfig.xres);
            stl_le_phys(&s->dma_as, value + 16, fbconfig.yres);
            resplen = 8;
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT:
            fbconfig.xres_virtual = ldl_le_phys(&s->dma_as, value + 12);
            fbconfig.yres_virtual = ldl_le_phys(&s->dma_as, value + 16);
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            /* fall through */
        case RPI_FWREQ_FRAMEBUFFER_GET_VIRTUAL_WIDTH_HEIGHT:
            stl_le_phys(&s->dma_as, value + 12, fbconfig.xres_virtual);
            stl_le_phys(&s->dma_as, value + 16, fbconfig.yres_virtual);
            resplen = 8;
            break;
        case RPI_FWREQ_FRAMEBUFFER_TEST_DEPTH:
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_DEPTH:
            fbconfig.bpp = ldl_le_phys(&s->dma_as, value + 12);
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            /* fall through */
        case RPI_FWREQ_FRAMEBUFFER_GET_DEPTH:
            stl_le_phys(&s->dma_as, value + 12, fbconfig.bpp);
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_TEST_PIXEL_ORDER:
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_PIXEL_ORDER:
            fbconfig.pixo = ldl_le_phys(&s->dma_as, value + 12);
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            /* fall through */
        case RPI_FWREQ_FRAMEBUFFER_GET_PIXEL_ORDER:
            stl_le_phys(&s->dma_as, value + 12, fbconfig.pixo);
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_TEST_ALPHA_MODE:
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_ALPHA_MODE:
            fbconfig.alpha = ldl_le_phys(&s->dma_as, value + 12);
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            /* fall through */
        case RPI_FWREQ_FRAMEBUFFER_GET_ALPHA_MODE:
            stl_le_phys(&s->dma_as, value + 12, fbconfig.alpha);
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_GET_PITCH:
            stl_le_phys(&s->dma_as, value + 12,
                        bcm2835_fb_get_pitch(&fbconfig));
            resplen = 4;
            break;
        case RPI_FWREQ_FRAMEBUFFER_TEST_VIRTUAL_OFFSET:
            resplen = 8;
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_OFFSET:
            fbconfig.xoffset = ldl_le_phys(&s->dma_as, value + 12);
            fbconfig.yoffset = ldl_le_phys(&s->dma_as, value + 16);
            bcm2835_fb_validate_config(&fbconfig);
            fbconfig_updated = true;
            /* fall through */
        case RPI_FWREQ_FRAMEBUFFER_GET_VIRTUAL_OFFSET:
            stl_le_phys(&s->dma_as, value + 12, fbconfig.xoffset);
            stl_le_phys(&s->dma_as, value + 16, fbconfig.yoffset);
            resplen = 8;
            break;
        case RPI_FWREQ_FRAMEBUFFER_GET_OVERSCAN:
        case RPI_FWREQ_FRAMEBUFFER_TEST_OVERSCAN:
        case RPI_FWREQ_FRAMEBUFFER_SET_OVERSCAN:
            stl_le_phys(&s->dma_as, value + 12, 0);
            stl_le_phys(&s->dma_as, value + 16, 0);
            stl_le_phys(&s->dma_as, value + 20, 0);
            stl_le_phys(&s->dma_as, value + 24, 0);
            resplen = 16;
            break;
        case RPI_FWREQ_FRAMEBUFFER_SET_PALETTE:
        {
            uint32_t offset = ldl_le_phys(&s->dma_as, value + 12);
            uint32_t length = ldl_le_phys(&s->dma_as, value + 16);
            int resp;

            if (offset > 255 || length < 1 || length > 256) {
                resp = 1; /* invalid request */
            } else {
                for (uint32_t e = 0; e < length; e++) {
                    uint32_t color = ldl_le_phys(&s->dma_as, value + 20 + (e << 2));
                    stl_le_phys(&s->dma_as,
                                s->fbdev->vcram_base + ((offset + e) << 2), color);
                }
                resp = 0;
            }
            stl_le_phys(&s->dma_as, value + 12, resp);
            resplen = 4;
            break;
        }
        case RPI_FWREQ_FRAMEBUFFER_GET_NUM_DISPLAYS:
            stl_le_phys(&s->dma_as, value + 12, 1);
            resplen = 4;
            break;

        case RPI_FWREQ_GET_DMA_CHANNELS:
            stl_le_phys(&s->dma_as, value + 12, s->dma_channels);
            resplen = 4;
            break;

        case RPI_FWREQ_NOTIFY_XHCI_RESET:
        {
            uint32_t dev_addr;

            if (bufsize < sizeof(dev_addr)) {
                break;
            }

            /*
             * Linux passes the hard-wired VL805 address using the firmware
             * encoding PCI_BUS << 20 | PCI_SLOT << 15 | PCI_FUNC << 12.
             * Despite its reset-controller API, this property call notifies
             * the firmware after PCI reset so it can initialize the VL805;
             * the call does not itself reset the xHCI register file.
             */
            dev_addr = ldl_le_phys(&s->dma_as, value + 12);
            if (s->has_vl805 && dev_addr == RPI4_VL805_PCI_DEV_ADDR) {
                stl_le_phys(&s->dma_as, value + 12, 0);
                qemu_irq_pulse(s->xhci_notify);
            } else {
                stl_le_phys(&s->dma_as, value + 12, UINT32_MAX);
            }
            resplen = sizeof(dev_addr);
            break;
        }

        case RPI_FWREQ_GET_COMMAND_LINE:
            /*
             * We follow the firmware behaviour: no NUL terminator is
             * written to the buffer, and if the buffer is too short
             * we report the required length in the response header
             * and copy nothing to the buffer.
             */
            resplen = strlen(s->command_line);
            if (bufsize >= resplen)
                address_space_write(&s->dma_as, value + 12,
                                    MEMTXATTRS_UNSPECIFIED, s->command_line,
                                    resplen);
            break;

        case RPI_FWREQ_GET_THROTTLED:
            stl_le_phys(&s->dma_as, value + 12, 0);
            resplen = 4;
            break;

        /* Firmware-managed power domains */

        case RPI_FWREQ_GET_DOMAIN_STATE:
        case RPI_FWREQ_SET_DOMAIN_STATE:
        {
            uint32_t id;
            uint32_t state;

            if (bufsize < 8) {
                break;
            }

            id = ldl_le_phys(&s->dma_as, value + 12);
            if (id == 0 || id >= RPI_FIRMWARE_NUM_DOMAIN_ID) {
                state = 0;
            } else {
                if (tag == RPI_FWREQ_SET_DOMAIN_STATE) {
                    state = ldl_le_phys(&s->dma_as, value + 16);
                    s->domain_states = deposit32(s->domain_states, id, 1,
                                                 !!state);
                }
                state = extract32(s->domain_states, id, 1);
            }
            stl_le_phys(&s->dma_as, value + 16, state);
            resplen = 8;
            break;
        }

        case RPI_FWREQ_NOTIFY_REBOOT:
            /* There is no VideoCore firmware state to quiesce in QEMU. */
            resplen = 0;
            break;

        /* Firmware-controlled GPIO expander */

        case RPI_FWREQ_GET_GPIO_CONFIG:
        {
            uint32_t gpio = ldl_le_phys(&s->dma_as, value + 12);
            unsigned int index;

            if (bufsize < 20) {
                break;
            }
            resplen = 20;
            if (!bcm2835_property_gpio_index(gpio, &index)) {
                break;
            }
            stl_le_phys(&s->dma_as, value + 12, 0);
            stl_le_phys(&s->dma_as, value + 16, s->gpio_direction[index]);
            stl_le_phys(&s->dma_as, value + 20, s->gpio_polarity[index]);
            stl_le_phys(&s->dma_as, value + 24, s->gpio_term_en[index]);
            stl_le_phys(&s->dma_as, value + 28, s->gpio_term_pull_up[index]);
            break;
        }

        case RPI_FWREQ_SET_GPIO_CONFIG:
        {
            uint32_t gpio = ldl_le_phys(&s->dma_as, value + 12);
            unsigned int index;

            if (bufsize < 24) {
                break;
            }
            resplen = 24;
            if (!bcm2835_property_gpio_index(gpio, &index)) {
                break;
            }
            s->gpio_direction[index] = ldl_le_phys(&s->dma_as, value + 16);
            s->gpio_polarity[index] = ldl_le_phys(&s->dma_as, value + 20);
            s->gpio_term_en[index] = ldl_le_phys(&s->dma_as, value + 24);
            s->gpio_term_pull_up[index] = ldl_le_phys(&s->dma_as, value + 28);
            s->gpio_state[index] = ldl_le_phys(&s->dma_as, value + 32);
            stl_le_phys(&s->dma_as, value + 12, 0);
            break;
        }

        case RPI_FWREQ_GET_GPIO_STATE:
        {
            uint32_t gpio = ldl_le_phys(&s->dma_as, value + 12);
            unsigned int index;

            if (bufsize < 8) {
                break;
            }
            resplen = 8;
            if (!bcm2835_property_gpio_index(gpio, &index)) {
                break;
            }
            stl_le_phys(&s->dma_as, value + 12, 0);
            stl_le_phys(&s->dma_as, value + 16, s->gpio_state[index]);
            break;
        }

        case RPI_FWREQ_SET_GPIO_STATE:
        {
            uint32_t gpio = ldl_le_phys(&s->dma_as, value + 12);
            unsigned int index;

            if (bufsize < 8) {
                break;
            }
            resplen = 8;
            if (!bcm2835_property_gpio_index(gpio, &index)) {
                break;
            }
            s->gpio_state[index] = ldl_le_phys(&s->dma_as, value + 16);
            stl_le_phys(&s->dma_as, value + 12, 0);
            break;
        }

        case RPI_FWREQ_VCHIQ_INIT:
            stl_le_phys(&s->dma_as,
                        value + offsetof(rpi_firmware_prop_request_t, payload),
                        0);
            resplen = VCHI_BUSADDR_SIZE;
            break;

        /* Customer OTP */

        case RPI_FWREQ_GET_CUSTOMER_OTP:
        {
            uint32_t start_num = ldl_le_phys(&s->dma_as, value + 12);
            uint32_t number = ldl_le_phys(&s->dma_as, value + 16);

            resplen = 8 + 4 * number;

            for (uint32_t n = start_num; n < start_num + number &&
                 n < BCM2835_OTP_CUSTOMER_OTP_LEN; n++) {
                uint32_t otp_row = bcm2835_otp_get_row(s->otp,
                                              BCM2835_OTP_CUSTOMER_OTP + n);
                stl_le_phys(&s->dma_as,
                            value + 20 + ((n - start_num) << 2), otp_row);
            }
            break;
        }
        case RPI_FWREQ_SET_CUSTOMER_OTP:
        {
            uint32_t start_num = ldl_le_phys(&s->dma_as, value + 12);
            uint32_t number = ldl_le_phys(&s->dma_as, value + 16);

            resplen = 4;

            /* Magic numbers to permanently lock customer OTP */
            if (start_num == BCM2835_OTP_LOCK_NUM1 &&
                number == BCM2835_OTP_LOCK_NUM2) {
                bcm2835_otp_set_row(s->otp,
                                    BCM2835_OTP_ROW_32,
                                    BCM2835_OTP_ROW_32_LOCK);
                break;
            }

            /* If row 32 has the lock bit, don't allow further writes */
            if (bcm2835_otp_get_row(s->otp, BCM2835_OTP_ROW_32) &
                                    BCM2835_OTP_ROW_32_LOCK) {
                break;
            }

            for (uint32_t n = start_num; n < start_num + number &&
                 n < BCM2835_OTP_CUSTOMER_OTP_LEN; n++) {
                uint32_t otp_row = ldl_le_phys(&s->dma_as,
                                      value + 20 + ((n - start_num) << 2));
                bcm2835_otp_set_row(s->otp,
                                    BCM2835_OTP_CUSTOMER_OTP + n, otp_row);
            }
            break;
        }

        /* Device-specific private key */
        case RPI_FWREQ_GET_PRIVATE_KEY:
        {
            uint32_t start_num = ldl_le_phys(&s->dma_as, value + 12);
            uint32_t number = ldl_le_phys(&s->dma_as, value + 16);

            resplen = 8 + 4 * number;

            for (uint32_t n = start_num; n < start_num + number &&
                 n < BCM2835_OTP_PRIVATE_KEY_LEN; n++) {
                uint32_t otp_row = bcm2835_otp_get_row(s->otp,
                                              BCM2835_OTP_PRIVATE_KEY + n);
                stl_le_phys(&s->dma_as,
                            value + 20 + ((n - start_num) << 2), otp_row);
            }
            break;
        }
        case RPI_FWREQ_SET_PRIVATE_KEY:
        {
            uint32_t start_num = ldl_le_phys(&s->dma_as, value + 12);
            uint32_t number = ldl_le_phys(&s->dma_as, value + 16);

            resplen = 4;

            /* If row 32 has the lock bit, don't allow further writes */
            if (bcm2835_otp_get_row(s->otp, BCM2835_OTP_ROW_32) &
                                    BCM2835_OTP_ROW_32_LOCK) {
                break;
            }

            for (uint32_t n = start_num; n < start_num + number &&
                 n < BCM2835_OTP_PRIVATE_KEY_LEN; n++) {
                uint32_t otp_row = ldl_le_phys(&s->dma_as,
                                      value + 20 + ((n - start_num) << 2));
                bcm2835_otp_set_row(s->otp,
                                    BCM2835_OTP_PRIVATE_KEY + n, otp_row);
            }
            break;
        }
        default:
            qemu_log_mask(LOG_UNIMP,
                          "bcm2835_property: unhandled tag 0x%08x\n", tag);
            break;
        }

        trace_bcm2835_mbox_property(tag, bufsize, resplen);
        if (tag == 0) {
            break;
        }

        stl_le_phys(&s->dma_as, value + 8, (1 << 31) | resplen);
        value += bufsize + 12;
    }

    /* Reconfigure framebuffer if required */
    if (fbconfig_updated) {
        bcm2835_fb_reconfigure(s->fbdev, &fbconfig);
    }

    /* Buffer response code */
    stl_le_phys(&s->dma_as, s->addr + 4, (1 << 31));
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
    .endianness = DEVICE_NATIVE_ENDIAN,
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
