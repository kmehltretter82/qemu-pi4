/*
 * BCM2838 RNG200 random number generator
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/misc/bcm2838_rng200.h"
#include "migration/vmstate.h"
#include "qemu/bswap.h"
#include "qemu/guest-random.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define RNG200_MMIO_SIZE                  0x28

#define RNG_CTRL                          0x00
#define RNG_CTRL_RBG_ENABLE_MASK          0x00001fff
#define RNG_CTRL_RBG_RATE_SHIFT           13
#define RNG_CTRL_RBG_RATE_MASK            0x00006000
#define RNG_CTRL_WRITABLE_MASK            0x00007fff

#define RNG_SOFT_RESET                    0x04
#define RBG_SOFT_RESET                    0x08
#define RNG_TOTAL_BIT_COUNT               0x0c
#define RNG_TOTAL_BIT_COUNT_THRESHOLD     0x10
#define RNG_REVISION                      0x14
#define RNG_REVISION_BCM2711              0x00040001
#define RNG_INT_STATUS                    0x18
#define RNG_INT_ENABLE                    0x1c
#define RNG_FIFO_DATA                     0x20
#define RNG_FIFO_COUNT                    0x24

#define RNG_INT_TOTAL_BITS_THRESHOLD      BIT(0)
#define RNG_INT_TOTAL_BITS_MAX            BIT(1)
#define RNG_INT_FIFO_FULL                 BIT(2)
#define RNG_INT_FIFO_OVERRUN              BIT(3)
#define RNG_INT_FIFO_UNDERRUN             BIT(4)
#define RNG_INT_NIST_FAIL                 BIT(5)
#define RNG_INT_STARTUP_TRANSITIONS       BIT(17)
#define RNG_INT_MASTER_FAIL_LOCKOUT       BIT(31)
#define RNG_INT_MASK                      (RNG_INT_TOTAL_BITS_THRESHOLD | \
                                           RNG_INT_TOTAL_BITS_MAX | \
                                           RNG_INT_FIFO_FULL | \
                                           RNG_INT_FIFO_OVERRUN | \
                                           RNG_INT_FIFO_UNDERRUN | \
                                           RNG_INT_NIST_FAIL | \
                                           RNG_INT_STARTUP_TRANSITIONS | \
                                           RNG_INT_MASTER_FAIL_LOCKOUT)

#define RNG_FIFO_WORDS                    16
#define RNG_FIFO_BYTES                    (RNG_FIFO_WORDS * sizeof(uint32_t))
#define RNG_FIFO_COUNT_MASK               0x000000ff
#define RNG_FIFO_THRESHOLD_SHIFT          8
#define RNG_FIFO_THRESHOLD_MASK           0x0000ff00
#define RNG_FIFO_FULL                     BIT(30)
#define RNG_FIFO_EMPTY                    BIT(31)

/* At selector zero RNG200 produces 8 Mbit/s; each step halves that rate. */
#define RNG_REFILL_FASTEST_NS             4000

static bool bcm2838_rng200_enabled(BCM2838Rng200State *s)
{
    return s->ctrl & RNG_CTRL_RBG_ENABLE_MASK;
}

static int64_t bcm2838_rng200_refill_period_ns(BCM2838Rng200State *s)
{
    unsigned int selector =
        (s->ctrl & RNG_CTRL_RBG_RATE_MASK) >> RNG_CTRL_RBG_RATE_SHIFT;

    return RNG_REFILL_FASTEST_NS << selector;
}

static void bcm2838_rng200_update_irq(BCM2838Rng200State *s)
{
    qemu_set_irq(s->irq, !!(s->int_status & s->int_enable));
}

static void bcm2838_rng200_schedule_refill(BCM2838Rng200State *s)
{
    if (!bcm2838_rng200_enabled(s) || fifo8_is_full(&s->fifo) ||
        timer_pending(&s->refill_timer)) {
        return;
    }

    timer_mod(&s->refill_timer,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
              bcm2838_rng200_refill_period_ns(s));
}

static void bcm2838_rng200_increment_bit_count(BCM2838Rng200State *s)
{
    uint32_t old_count = s->total_bit_count;

    if (old_count >= UINT32_MAX - 32) {
        s->total_bit_count = UINT32_MAX;
        s->int_status |= RNG_INT_TOTAL_BITS_MAX;
    } else {
        s->total_bit_count = old_count + 32;
    }

    if (s->total_bit_count_threshold &&
        old_count < s->total_bit_count_threshold &&
        s->total_bit_count >= s->total_bit_count_threshold) {
        s->int_status |= RNG_INT_TOTAL_BITS_THRESHOLD;
    }
}

static void bcm2838_rng200_refill(void *opaque)
{
    BCM2838Rng200State *s = BCM2838_RNG200(opaque);
    uint8_t data[sizeof(uint32_t)];

    if (!bcm2838_rng200_enabled(s) || fifo8_num_free(&s->fifo) < sizeof(data)) {
        return;
    }

    qemu_guest_getrandom_nofail(data, sizeof(data));
    fifo8_push_all(&s->fifo, data, sizeof(data));
    bcm2838_rng200_increment_bit_count(s);
    s->int_status |= RNG_INT_STARTUP_TRANSITIONS;
    if (fifo8_is_full(&s->fifo)) {
        s->int_status |= RNG_INT_FIFO_FULL;
    }
    bcm2838_rng200_update_irq(s);
    bcm2838_rng200_schedule_refill(s);
}

static uint32_t bcm2838_rng200_fifo_count(BCM2838Rng200State *s)
{
    uint32_t words = fifo8_num_used(&s->fifo) / sizeof(uint32_t);
    uint32_t value = (s->fifo_threshold << RNG_FIFO_THRESHOLD_SHIFT) |
                     (words & RNG_FIFO_COUNT_MASK);

    if (!words) {
        value |= RNG_FIFO_EMPTY;
    } else if (words == RNG_FIFO_WORDS) {
        value |= RNG_FIFO_FULL;
    }
    return value;
}

static uint32_t bcm2838_rng200_fifo_read(BCM2838Rng200State *s)
{
    uint8_t data[sizeof(uint32_t)];

    if (fifo8_num_used(&s->fifo) < sizeof(data)) {
        s->int_status |= RNG_INT_FIFO_UNDERRUN;
        bcm2838_rng200_update_irq(s);
        bcm2838_rng200_schedule_refill(s);
        return s->last_fifo_data;
    }

    g_assert(fifo8_pop_buf(&s->fifo, data, sizeof(data)) == sizeof(data));
    s->last_fifo_data = ldl_le_p(data);
    bcm2838_rng200_schedule_refill(s);
    return s->last_fifo_data;
}

static uint64_t bcm2838_rng200_read(void *opaque, hwaddr offset,
                                    unsigned int size)
{
    BCM2838Rng200State *s = BCM2838_RNG200(opaque);

    switch (offset) {
    case RNG_CTRL:
        return s->ctrl;
    case RNG_SOFT_RESET:
    case RBG_SOFT_RESET:
        return 0;
    case RNG_TOTAL_BIT_COUNT:
        return s->total_bit_count;
    case RNG_TOTAL_BIT_COUNT_THRESHOLD:
        return s->total_bit_count_threshold;
    case RNG_REVISION:
        return RNG_REVISION_BCM2711;
    case RNG_INT_STATUS:
        return s->int_status;
    case RNG_INT_ENABLE:
        return s->int_enable;
    case RNG_FIFO_DATA:
        return bcm2838_rng200_fifo_read(s);
    case RNG_FIFO_COUNT:
        return bcm2838_rng200_fifo_count(s);
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2838_RNG200, offset);
        return 0;
    }
}

static void bcm2838_rng200_rng_soft_reset(BCM2838Rng200State *s)
{
    timer_del(&s->refill_timer);
    fifo8_reset(&s->fifo);
    s->ctrl = 0;
    s->total_bit_count = 0;
    s->total_bit_count_threshold = 0;
    s->int_status = 0;
    s->int_enable = 0;
    s->last_fifo_data = 0;
    s->fifo_threshold = RNG_FIFO_WORDS;
    bcm2838_rng200_update_irq(s);
}

static void bcm2838_rng200_rbg_soft_reset(BCM2838Rng200State *s)
{
    timer_del(&s->refill_timer);
    fifo8_reset(&s->fifo);
    s->int_status &= ~(RNG_INT_FIFO_FULL |
                       RNG_INT_FIFO_OVERRUN |
                       RNG_INT_FIFO_UNDERRUN |
                       RNG_INT_STARTUP_TRANSITIONS);
    s->last_fifo_data = 0;
    bcm2838_rng200_update_irq(s);
    bcm2838_rng200_schedule_refill(s);
}

static void bcm2838_rng200_write(void *opaque, hwaddr offset,
                                 uint64_t value, unsigned int size)
{
    BCM2838Rng200State *s = BCM2838_RNG200(opaque);

    switch (offset) {
    case RNG_CTRL:
        s->ctrl = value & RNG_CTRL_WRITABLE_MASK;
        timer_del(&s->refill_timer);
        bcm2838_rng200_schedule_refill(s);
        break;
    case RNG_SOFT_RESET:
        if (value & 1) {
            bcm2838_rng200_rng_soft_reset(s);
        }
        break;
    case RBG_SOFT_RESET:
        if (value & 1) {
            bcm2838_rng200_rbg_soft_reset(s);
        }
        break;
    case RNG_TOTAL_BIT_COUNT:
    case RNG_REVISION:
    case RNG_FIFO_DATA:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to read-only offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2838_RNG200, offset);
        break;
    case RNG_TOTAL_BIT_COUNT_THRESHOLD:
        s->total_bit_count_threshold = value;
        break;
    case RNG_INT_STATUS:
        s->int_status &= ~(value & RNG_INT_MASK);
        bcm2838_rng200_update_irq(s);
        break;
    case RNG_INT_ENABLE:
        s->int_enable = value & RNG_INT_MASK;
        bcm2838_rng200_update_irq(s);
        break;
    case RNG_FIFO_COUNT:
        s->fifo_threshold =
            (value & RNG_FIFO_THRESHOLD_MASK) >> RNG_FIFO_THRESHOLD_SHIFT;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2838_RNG200, offset);
        break;
    }
}

static const MemoryRegionOps bcm2838_rng200_ops = {
    .read = bcm2838_rng200_read,
    .write = bcm2838_rng200_write,
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

static int bcm2838_rng200_post_load(void *opaque, int version_id)
{
    BCM2838Rng200State *s = BCM2838_RNG200(opaque);

    if (fifo8_num_used(&s->fifo) % sizeof(uint32_t)) {
        return -EINVAL;
    }
    bcm2838_rng200_update_irq(s);
    return 0;
}

static const VMStateDescription vmstate_bcm2838_rng200 = {
    .name = TYPE_BCM2838_RNG200,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2838_rng200_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(ctrl, BCM2838Rng200State),
        VMSTATE_UINT32(total_bit_count, BCM2838Rng200State),
        VMSTATE_UINT32(total_bit_count_threshold, BCM2838Rng200State),
        VMSTATE_UINT32(int_status, BCM2838Rng200State),
        VMSTATE_UINT32(int_enable, BCM2838Rng200State),
        VMSTATE_UINT32(last_fifo_data, BCM2838Rng200State),
        VMSTATE_UINT8(fifo_threshold, BCM2838Rng200State),
        VMSTATE_FIFO8(fifo, BCM2838Rng200State),
        VMSTATE_TIMER(refill_timer, BCM2838Rng200State),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2838_rng200_reset_hold(Object *obj, ResetType type)
{
    bcm2838_rng200_rng_soft_reset(BCM2838_RNG200(obj));
}

static void bcm2838_rng200_init(Object *obj)
{
    BCM2838Rng200State *s = BCM2838_RNG200(obj);

    fifo8_create(&s->fifo, RNG_FIFO_BYTES);
    timer_init_ns(&s->refill_timer, QEMU_CLOCK_VIRTUAL,
                  bcm2838_rng200_refill, s);
    memory_region_init_io(&s->iomem, obj, &bcm2838_rng200_ops, s,
                          TYPE_BCM2838_RNG200, RNG200_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
}

static void bcm2838_rng200_finalize(Object *obj)
{
    BCM2838Rng200State *s = BCM2838_RNG200(obj);

    timer_deinit(&s->refill_timer);
    fifo8_destroy(&s->fifo);
}

static void bcm2838_rng200_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->desc = "BCM2838 RNG200 random number generator";
    dc->vmsd = &vmstate_bcm2838_rng200;
    rc->phases.hold = bcm2838_rng200_reset_hold;
}

static const TypeInfo bcm2838_rng200_info = {
    .name = TYPE_BCM2838_RNG200,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2838Rng200State),
    .instance_init = bcm2838_rng200_init,
    .instance_finalize = bcm2838_rng200_finalize,
    .class_init = bcm2838_rng200_class_init,
};

static void bcm2838_rng200_register_types(void)
{
    type_register_static(&bcm2838_rng200_info);
}

type_init(bcm2838_rng200_register_types)
