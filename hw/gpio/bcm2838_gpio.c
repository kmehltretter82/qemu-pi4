/*
 * Raspberry Pi (BCM2838) GPIO Controller
 * This implementation is based on the BCM2835 GPIO programming model.
 *
 * Copyright (c) 2022 Auriga LLC
 *
 * Authors:
 *  Lotosh, Aleksey <aleksey.lotosh@auriga.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/core/sysbus.h"
#include "migration/vmstate.h"
#include "hw/sd/sd.h"
#include "hw/gpio/bcm2838_gpio.h"
#include "hw/core/irq.h"

#define GPFSEL0   0x00
#define GPFSEL1   0x04
#define GPFSEL2   0x08
#define GPFSEL3   0x0C
#define GPFSEL4   0x10
#define GPFSEL5   0x14
#define GPSET0    0x1C
#define GPSET1    0x20
#define GPCLR0    0x28
#define GPCLR1    0x2C
#define GPLEV0    0x34
#define GPLEV1    0x38
#define GPEDS0    0x40
#define GPEDS1    0x44
#define GPREN0    0x4C
#define GPREN1    0x50
#define GPFEN0    0x58
#define GPFEN1    0x5C
#define GPHEN0    0x64
#define GPHEN1    0x68
#define GPLEN0    0x70
#define GPLEN1    0x74
#define GPAREN0   0x7C
#define GPAREN1   0x80
#define GPAFEN0   0x88
#define GPAFEN1   0x8C

#define GPIO_PUP_PDN_CNTRL_REG0 0xE4
#define GPIO_PUP_PDN_CNTRL_REG1 0xE8
#define GPIO_PUP_PDN_CNTRL_REG2 0xEC
#define GPIO_PUP_PDN_CNTRL_REG3 0xF0

#define RESET_VAL_CNTRL_REG0 0xAAA95555
#define RESET_VAL_CNTRL_REG1 0xA0AAAAAA
#define RESET_VAL_CNTRL_REG2 0x50AAA95A
#define RESET_VAL_CNTRL_REG3 0x00055555

#define GPIO_REG_BANK1_VALID_MASK 0x03ffffff
#define GPIO_IRQ_BANK0_MASK       0x0fffffff
#define GPIO_IRQ_BANK1_LOW_MASK   0x00003fff
#define GPIO_IRQ_BANK2_MASK       0x03ffc000
#define GPIO_PUP_REG3_VALID_MASK  0x000fffff

#define NUM_FSELN_IN_GPFSELN 10
#define NUM_BITS_FSELN       3
#define MASK_FSELN           0x7

#define BYTES_IN_WORD        4

/* bcm,function property */
#define BCM2838_FSEL_GPIO_IN    0
#define BCM2838_FSEL_GPIO_OUT   1
#define BCM2838_FSEL_ALT5       2
#define BCM2838_FSEL_ALT4       3
#define BCM2838_FSEL_ALT0       4
#define BCM2838_FSEL_ALT1       5
#define BCM2838_FSEL_ALT2       6
#define BCM2838_FSEL_ALT3       7

static uint32_t gpfsel_get(BCM2838GpioState *s, uint8_t reg)
{
    int i;
    uint32_t value = 0;
    for (i = 0; i < NUM_FSELN_IN_GPFSELN; i++) {
        uint32_t index = NUM_FSELN_IN_GPFSELN * reg + i;
        if (index < sizeof(s->fsel)) {
            value |= (s->fsel[index] & MASK_FSELN) << (NUM_BITS_FSELN * i);
        }
    }
    return value;
}

static uint32_t bcm2838_gpio_valid_mask(unsigned int bank)
{
    return bank ? GPIO_REG_BANK1_VALID_MASK : UINT32_MAX;
}

static uint32_t *bcm2838_gpio_output_latch(BCM2838GpioState *s,
                                           unsigned int bank)
{
    return bank ? &s->lev1 : &s->lev0;
}

static uint32_t bcm2838_gpio_output_mask(BCM2838GpioState *s,
                                         unsigned int bank)
{
    unsigned int first = bank * 32;
    unsigned int last = MIN(first + 32, BCM2838_GPIO_NUM);
    uint32_t mask = 0;

    for (unsigned int pin = first; pin < last; pin++) {
        if (s->fsel[pin] == BCM2838_FSEL_GPIO_OUT) {
            mask |= BIT(pin - first);
        }
    }
    return mask;
}

static uint32_t bcm2838_gpio_level(BCM2838GpioState *s, unsigned int bank)
{
    uint32_t output_mask = bcm2838_gpio_output_mask(s, bank);
    uint32_t output = *bcm2838_gpio_output_latch(s, bank);

    return ((output & output_mask) |
            (s->input_level[bank] & ~output_mask)) &
           bcm2838_gpio_valid_mask(bank);
}

static void bcm2838_gpio_update_irqs(BCM2838GpioState *s)
{
    uint32_t events0 = s->event_status[0];
    uint32_t events1 = s->event_status[1] & GPIO_REG_BANK1_VALID_MASK;

    qemu_set_irq(s->irq[0], !!(events0 & GPIO_IRQ_BANK0_MASK));
    qemu_set_irq(s->irq[1],
                 !!((events0 & ~GPIO_IRQ_BANK0_MASK) |
                    (events1 & GPIO_IRQ_BANK1_LOW_MASK)));
    qemu_set_irq(s->irq[2], !!(events1 & GPIO_IRQ_BANK2_MASK));
    qemu_set_irq(s->irq[3], !!(events0 | events1));
}

static void bcm2838_gpio_latch_events(BCM2838GpioState *s,
                                      unsigned int bank,
                                      uint32_t old_level,
                                      uint32_t new_level)
{
    uint32_t rising = ~old_level & new_level;
    uint32_t falling = old_level & ~new_level;
    uint32_t events;

    events = rising & (s->rising_detect[bank] |
                       s->async_rising_detect[bank]);
    events |= falling & (s->falling_detect[bank] |
                         s->async_falling_detect[bank]);
    events |= new_level & s->high_detect[bank];
    events |= ~new_level & s->low_detect[bank];
    s->event_status[bank] |= events & bcm2838_gpio_valid_mask(bank);
    bcm2838_gpio_update_irqs(s);
}

static void bcm2838_gpio_refresh_level_events(BCM2838GpioState *s,
                                              unsigned int bank)
{
    uint32_t level = bcm2838_gpio_level(s, bank);
    uint32_t events = (level & s->high_detect[bank]) |
                      (~level & s->low_detect[bank]);

    s->event_status[bank] |= events & bcm2838_gpio_valid_mask(bank);
    bcm2838_gpio_update_irqs(s);
}

static void bcm2838_gpio_drive_outputs(BCM2838GpioState *s)
{
    for (unsigned int pin = 0; pin < BCM2838_GPIO_NUM; pin++) {
        unsigned int bank = pin / 32;
        unsigned int bit = pin % 32;
        int level = 0;

        if (s->fsel[pin] == BCM2838_FSEL_GPIO_OUT) {
            level = !!(*bcm2838_gpio_output_latch(s, bank) & BIT(bit));
        }
        qemu_set_irq(s->out[pin], level);
    }
}

static void gpfsel_set(BCM2838GpioState *s, uint8_t reg, uint32_t value)
{
    uint32_t old_level[BCM2838_GPIO_REG_BANKS];
    int i;

    for (i = 0; i < BCM2838_GPIO_REG_BANKS; i++) {
        old_level[i] = bcm2838_gpio_level(s, i);
    }

    for (i = 0; i < NUM_FSELN_IN_GPFSELN; i++) {
        uint32_t index = NUM_FSELN_IN_GPFSELN * reg + i;
        if (index < sizeof(s->fsel)) {
            int fsel = (value >> (NUM_BITS_FSELN * i)) & MASK_FSELN;
            s->fsel[index] = fsel;
        }
    }

    /* SD controller selection (48-53) */
    if (s->sd_fsel != BCM2838_FSEL_GPIO_IN
        && (s->fsel[48] == BCM2838_FSEL_GPIO_IN)
        && (s->fsel[49] == BCM2838_FSEL_GPIO_IN)
        && (s->fsel[50] == BCM2838_FSEL_GPIO_IN)
        && (s->fsel[51] == BCM2838_FSEL_GPIO_IN)
        && (s->fsel[52] == BCM2838_FSEL_GPIO_IN)
        && (s->fsel[53] == BCM2838_FSEL_GPIO_IN)
       ) {
        /* SDHCI controller selected */
        sdbus_reparent_card(s->sdbus_sdhost, s->sdbus_sdhci);
        s->sd_fsel = BCM2838_FSEL_GPIO_IN;
    } else if (s->sd_fsel != BCM2838_FSEL_ALT0
               && (s->fsel[48] == BCM2838_FSEL_ALT0) /* SD_CLK_R */
               && (s->fsel[49] == BCM2838_FSEL_ALT0) /* SD_CMD_R */
               && (s->fsel[50] == BCM2838_FSEL_ALT0) /* SD_DATA0_R */
               && (s->fsel[51] == BCM2838_FSEL_ALT0) /* SD_DATA1_R */
               && (s->fsel[52] == BCM2838_FSEL_ALT0) /* SD_DATA2_R */
               && (s->fsel[53] == BCM2838_FSEL_ALT0) /* SD_DATA3_R */
              ) {
        /* SDHost controller selected */
        sdbus_reparent_card(s->sdbus_sdhci, s->sdbus_sdhost);
        s->sd_fsel = BCM2838_FSEL_ALT0;
    }

    bcm2838_gpio_drive_outputs(s);
    for (i = 0; i < BCM2838_GPIO_REG_BANKS; i++) {
        bcm2838_gpio_latch_events(s, i, old_level[i],
                                  bcm2838_gpio_level(s, i));
    }
}

static int gpfsel_is_out(BCM2838GpioState *s, int index)
{
    if (index >= 0 && index < BCM2838_GPIO_NUM) {
        return s->fsel[index] == 1;
    }
    return 0;
}

static void bcm2838_gpio_update_output_latch(BCM2838GpioState *s,
                                             unsigned int bank,
                                             uint32_t value, bool set)
{
    uint32_t *latch = bcm2838_gpio_output_latch(s, bank);
    uint32_t old_level = bcm2838_gpio_level(s, bank);
    uint32_t mask = value & bcm2838_gpio_valid_mask(bank);
    unsigned int first = bank * 32;
    unsigned int last = MIN(first + 32, BCM2838_GPIO_NUM);

    if (set) {
        *latch |= mask;
    } else {
        *latch &= ~mask;
    }

    for (unsigned int pin = first; pin < last; pin++) {
        if ((mask & BIT(pin - first)) && gpfsel_is_out(s, pin)) {
            qemu_set_irq(s->out[pin], set);
        }
    }

    bcm2838_gpio_latch_events(s, bank, old_level,
                              bcm2838_gpio_level(s, bank));
}

static void bcm2838_gpio_set(void *opaque, int irq, int level)
{
    BCM2838GpioState *s = BCM2838_GPIO(opaque);
    unsigned int bank = irq / 32;
    unsigned int bit = irq % 32;
    uint32_t old_level = bcm2838_gpio_level(s, bank);

    if (level > 0) {
        s->input_level[bank] |= BIT(bit);
    } else {
        s->input_level[bank] &= ~BIT(bit);
    }

    bcm2838_gpio_latch_events(s, bank, old_level,
                              bcm2838_gpio_level(s, bank));
}

static uint64_t bcm2838_gpio_read(void *opaque, hwaddr offset, unsigned size)
{
    BCM2838GpioState *s = (BCM2838GpioState *)opaque;
    uint64_t value = 0;

    switch (offset) {
    case GPFSEL0:
    case GPFSEL1:
    case GPFSEL2:
    case GPFSEL3:
    case GPFSEL4:
    case GPFSEL5:
        value = gpfsel_get(s, offset / BYTES_IN_WORD);
        break;
    case GPSET0:
    case GPSET1:
    case GPCLR0:
    case GPCLR1:
        /* Write Only */
        qemu_log_mask(LOG_GUEST_ERROR, "%s: %s: Attempt reading from write only"
                      " register. 0x%"PRIx64" will be returned."
                      " Address 0x%"HWADDR_PRIx", size %u\n",
                      TYPE_BCM2838_GPIO, __func__, value, offset, size);
        break;
    case GPLEV0:
        value = bcm2838_gpio_level(s, 0);
        break;
    case GPLEV1:
        value = bcm2838_gpio_level(s, 1);
        break;
    case GPEDS0:
    case GPEDS1:
        value = s->event_status[(offset - GPEDS0) / BYTES_IN_WORD];
        break;
    case GPREN0:
    case GPREN1:
        value = s->rising_detect[(offset - GPREN0) / BYTES_IN_WORD];
        break;
    case GPFEN0:
    case GPFEN1:
        value = s->falling_detect[(offset - GPFEN0) / BYTES_IN_WORD];
        break;
    case GPHEN0:
    case GPHEN1:
        value = s->high_detect[(offset - GPHEN0) / BYTES_IN_WORD];
        break;
    case GPLEN0:
    case GPLEN1:
        value = s->low_detect[(offset - GPLEN0) / BYTES_IN_WORD];
        break;
    case GPAREN0:
    case GPAREN1:
        value = s->async_rising_detect[
            (offset - GPAREN0) / BYTES_IN_WORD];
        break;
    case GPAFEN0:
    case GPAFEN1:
        value = s->async_falling_detect[
            (offset - GPAFEN0) / BYTES_IN_WORD];
        break;
    case GPIO_PUP_PDN_CNTRL_REG0:
    case GPIO_PUP_PDN_CNTRL_REG1:
    case GPIO_PUP_PDN_CNTRL_REG2:
    case GPIO_PUP_PDN_CNTRL_REG3:
        value = s->pup_cntrl_reg[(offset - GPIO_PUP_PDN_CNTRL_REG0)
                                 / sizeof(s->pup_cntrl_reg[0])];
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: %s: bad offset %"HWADDR_PRIx"\n",
                      TYPE_BCM2838_GPIO, __func__, offset);
        break;
    }

    return value;
}

static void bcm2838_gpio_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned size)
{
    BCM2838GpioState *s = (BCM2838GpioState *)opaque;

    switch (offset) {
    case GPFSEL0:
    case GPFSEL1:
    case GPFSEL2:
    case GPFSEL3:
    case GPFSEL4:
    case GPFSEL5:
        gpfsel_set(s, offset / BYTES_IN_WORD, value);
        break;
    case GPSET0:
        bcm2838_gpio_update_output_latch(s, 0, value, true);
        break;
    case GPSET1:
        bcm2838_gpio_update_output_latch(s, 1, value, true);
        break;
    case GPCLR0:
        bcm2838_gpio_update_output_latch(s, 0, value, false);
        break;
    case GPCLR1:
        bcm2838_gpio_update_output_latch(s, 1, value, false);
        break;
    case GPLEV0:
    case GPLEV1:
        /* Read Only */
        qemu_log_mask(LOG_GUEST_ERROR, "%s: %s: Attempt writing 0x%"PRIx64""
                      " to read only register. Ignored."
                      " Address 0x%"HWADDR_PRIx", size %u\n",
                      TYPE_BCM2838_GPIO, __func__, value, offset, size);
        break;
    case GPEDS0:
    case GPEDS1:
    {
        unsigned int bank = (offset - GPEDS0) / BYTES_IN_WORD;

        s->event_status[bank] &= ~value;
        bcm2838_gpio_refresh_level_events(s, bank);
        break;
    }
    case GPREN0:
    case GPREN1:
    {
        unsigned int bank = (offset - GPREN0) / BYTES_IN_WORD;

        s->rising_detect[bank] = value & bcm2838_gpio_valid_mask(bank);
        break;
    }
    case GPFEN0:
    case GPFEN1:
    {
        unsigned int bank = (offset - GPFEN0) / BYTES_IN_WORD;

        s->falling_detect[bank] = value & bcm2838_gpio_valid_mask(bank);
        break;
    }
    case GPHEN0:
    case GPHEN1:
    {
        unsigned int bank = (offset - GPHEN0) / BYTES_IN_WORD;

        s->high_detect[bank] = value & bcm2838_gpio_valid_mask(bank);
        bcm2838_gpio_refresh_level_events(s, bank);
        break;
    }
    case GPLEN0:
    case GPLEN1:
    {
        unsigned int bank = (offset - GPLEN0) / BYTES_IN_WORD;

        s->low_detect[bank] = value & bcm2838_gpio_valid_mask(bank);
        bcm2838_gpio_refresh_level_events(s, bank);
        break;
    }
    case GPAREN0:
    case GPAREN1:
    {
        unsigned int bank = (offset - GPAREN0) / BYTES_IN_WORD;

        s->async_rising_detect[bank] =
            value & bcm2838_gpio_valid_mask(bank);
        break;
    }
    case GPAFEN0:
    case GPAFEN1:
    {
        unsigned int bank = (offset - GPAFEN0) / BYTES_IN_WORD;

        s->async_falling_detect[bank] =
            value & bcm2838_gpio_valid_mask(bank);
        break;
    }
    case GPIO_PUP_PDN_CNTRL_REG0:
    case GPIO_PUP_PDN_CNTRL_REG1:
    case GPIO_PUP_PDN_CNTRL_REG2:
    case GPIO_PUP_PDN_CNTRL_REG3:
    {
        unsigned int reg = (offset - GPIO_PUP_PDN_CNTRL_REG0) /
                           sizeof(s->pup_cntrl_reg[0]);

        s->pup_cntrl_reg[reg] = value;
        if (reg == GPIO_PUP_PDN_CNTRL_NUM - 1) {
            s->pup_cntrl_reg[reg] &= GPIO_PUP_REG3_VALID_MASK;
        }
        break;
    }
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: %s: bad offset %"HWADDR_PRIx"\n",
                  TYPE_BCM2838_GPIO, __func__, offset);
    }
}

static void bcm2838_gpio_reset(DeviceState *dev)
{
    BCM2838GpioState *s = BCM2838_GPIO(dev);

    memset(s->fsel, 0, sizeof(s->fsel));
    s->sd_fsel = 0;

    /* SDHCI is selected by default */
    sdbus_reparent_card(&s->sdbus, s->sdbus_sdhci);

    s->lev0 = 0;
    s->lev1 = 0;
    /* External pin levels are not controller state and survive reset. */
    memset(s->event_status, 0, sizeof(s->event_status));
    memset(s->rising_detect, 0, sizeof(s->rising_detect));
    memset(s->falling_detect, 0, sizeof(s->falling_detect));
    memset(s->high_detect, 0, sizeof(s->high_detect));
    memset(s->low_detect, 0, sizeof(s->low_detect));
    memset(s->async_rising_detect, 0, sizeof(s->async_rising_detect));
    memset(s->async_falling_detect, 0, sizeof(s->async_falling_detect));

    s->pup_cntrl_reg[0] = RESET_VAL_CNTRL_REG0;
    s->pup_cntrl_reg[1] = RESET_VAL_CNTRL_REG1;
    s->pup_cntrl_reg[2] = RESET_VAL_CNTRL_REG2;
    s->pup_cntrl_reg[3] = RESET_VAL_CNTRL_REG3;

    bcm2838_gpio_drive_outputs(s);
    bcm2838_gpio_update_irqs(s);
}

static const MemoryRegionOps bcm2838_gpio_ops = {
    .read = bcm2838_gpio_read,
    .write = bcm2838_gpio_write,
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

static int bcm2838_gpio_post_load(void *opaque, int version_id)
{
    BCM2838GpioState *s = BCM2838_GPIO(opaque);
    uint32_t invalid = s->input_level[1] | s->event_status[1] |
                       s->rising_detect[1] | s->falling_detect[1] |
                       s->high_detect[1] | s->low_detect[1] |
                       s->async_rising_detect[1] |
                       s->async_falling_detect[1];

    if (version_id >= 2 && (invalid & ~GPIO_REG_BANK1_VALID_MASK)) {
        return -EINVAL;
    }

    /* Version-one streams could retain writes to reserved GPSET1 bits. */
    s->lev1 &= GPIO_REG_BANK1_VALID_MASK;
    s->input_level[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->event_status[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->rising_detect[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->falling_detect[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->high_detect[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->low_detect[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->async_rising_detect[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->async_falling_detect[1] &= GPIO_REG_BANK1_VALID_MASK;
    s->pup_cntrl_reg[3] &= GPIO_PUP_REG3_VALID_MASK;

    bcm2838_gpio_drive_outputs(s);
    for (unsigned int bank = 0; bank < BCM2838_GPIO_REG_BANKS; bank++) {
        bcm2838_gpio_refresh_level_events(s, bank);
    }
    return 0;
}

static const VMStateDescription vmstate_bcm2838_gpio = {
    .name = "bcm2838_gpio",
    .version_id = 2,
    .minimum_version_id = 1,
    .post_load = bcm2838_gpio_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8_ARRAY(fsel, BCM2838GpioState, BCM2838_GPIO_NUM),
        VMSTATE_UINT32(lev0, BCM2838GpioState),
        VMSTATE_UINT32(lev1, BCM2838GpioState),
        VMSTATE_UINT8(sd_fsel, BCM2838GpioState),
        VMSTATE_UINT32_ARRAY(pup_cntrl_reg, BCM2838GpioState,
                             GPIO_PUP_PDN_CNTRL_NUM),
        VMSTATE_UINT32_ARRAY_V(input_level, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_UINT32_ARRAY_V(event_status, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_UINT32_ARRAY_V(rising_detect, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_UINT32_ARRAY_V(falling_detect, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_UINT32_ARRAY_V(high_detect, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_UINT32_ARRAY_V(low_detect, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_UINT32_ARRAY_V(async_rising_detect, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_UINT32_ARRAY_V(async_falling_detect, BCM2838GpioState,
                               BCM2838_GPIO_REG_BANKS, 2),
        VMSTATE_END_OF_LIST()
    }
};

static void bcm2838_gpio_init(Object *obj)
{
    BCM2838GpioState *s = BCM2838_GPIO(obj);
    DeviceState *dev = DEVICE(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    qbus_init(&s->sdbus, sizeof(s->sdbus), TYPE_SD_BUS, DEVICE(s), "sd-bus");

    memory_region_init_io(&s->iomem, obj, &bcm2838_gpio_ops, s,
                          "bcm2838_gpio", BCM2838_GPIO_REGS_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    for (unsigned int i = 0; i < BCM2838_GPIO_IRQS; i++) {
        sysbus_init_irq(sbd, &s->irq[i]);
    }
    qdev_init_gpio_in(dev, bcm2838_gpio_set, BCM2838_GPIO_NUM);
    qdev_init_gpio_out(dev, s->out, BCM2838_GPIO_NUM);
}

static void bcm2838_gpio_realize(DeviceState *dev, Error **errp)
{
    BCM2838GpioState *s = BCM2838_GPIO(dev);
    Object *obj;

    obj = object_property_get_link(OBJECT(dev), "sdbus-sdhci", &error_abort);
    s->sdbus_sdhci = SD_BUS(obj);

    obj = object_property_get_link(OBJECT(dev), "sdbus-sdhost", &error_abort);
    s->sdbus_sdhost = SD_BUS(obj);
}

static void bcm2838_gpio_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_bcm2838_gpio;
    dc->realize = &bcm2838_gpio_realize;
    device_class_set_legacy_reset(dc, bcm2838_gpio_reset);
}

static const TypeInfo bcm2838_gpio_info = {
    .name          = TYPE_BCM2838_GPIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2838GpioState),
    .instance_init = bcm2838_gpio_init,
    .class_init    = bcm2838_gpio_class_init,
};

static void bcm2838_gpio_register_types(void)
{
    type_register_static(&bcm2838_gpio_info);
}

type_init(bcm2838_gpio_register_types)
