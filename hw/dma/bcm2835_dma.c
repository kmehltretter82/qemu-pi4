/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/dma/bcm2835_dma.h"
#include "hw/core/irq.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "system/dma.h"
#include "system/runstate.h"

/* DMA CS Control and Status bits */
#define BCM2708_DMA_ACTIVE      (1 << 0)
#define BCM2708_DMA_END         (1 << 1) /* GE */
#define BCM2708_DMA_INT         (1 << 2)
#define BCM2708_DMA_DREQ        (1 << 3)
#define BCM2708_DMA_ISPAUSED    (1 << 4)  /* Pause requested or not active */
#define BCM2708_DMA_ISHELD      (1 << 5)  /* Is held by DREQ flow control */
#define BCM2708_DMA_ERR         (1 << 8)
#define BCM2708_DMA_ABORT       (1 << 30) /* stop current CB, go to next, WO */
#define BCM2708_DMA_RESET       (1 << 31) /* WO, self clearing */

/* DMA control block "info" field bits */
#define BCM2708_DMA_INT_EN      (1 << 0)
#define BCM2708_DMA_TDMODE      (1 << 1)
#define BCM2708_DMA_WAIT_RESP   (1 << 3)
#define BCM2708_DMA_D_INC       (1 << 4)
#define BCM2708_DMA_D_WIDTH     (1 << 5)
#define BCM2708_DMA_D_DREQ      (1 << 6)
#define BCM2708_DMA_D_IGNORE    (1 << 7)
#define BCM2708_DMA_S_INC       (1 << 8)
#define BCM2708_DMA_S_WIDTH     (1 << 9)
#define BCM2708_DMA_S_DREQ      (1 << 10)
#define BCM2708_DMA_S_IGNORE    (1 << 11)
#define BCM2708_DMA_PERMAP_SHIFT 16
#define BCM2708_DMA_PERMAP_MASK  0x1f

/* Register offsets */
#define BCM2708_DMA_CS          0x00 /* Control and Status */
#define BCM2708_DMA_ADDR        0x04 /* Control block address */
/* the current control block appears in the following registers - read only */
#define BCM2708_DMA_INFO        0x08
#define BCM2708_DMA_SOURCE_AD   0x0c
#define BCM2708_DMA_DEST_AD     0x10
#define BCM2708_DMA_TXFR_LEN    0x14
#define BCM2708_DMA_STRIDE      0x18
#define BCM2708_DMA_NEXTCB      0x1C
#define BCM2708_DMA_DEBUG       0x20

#define BCM2708_DMA_INT_STATUS  0xfe0 /* Interrupt status of each channel */
#define BCM2708_DMA_ENABLE      0xff0 /* Global enable bits for each channel */

#define BCM2708_DMA_CS_RW_MASK  0x30ff0001 /* All RW bits in DMA_CS */
#define BCM2708_DMA_CB_ALIGN_MASK (~0x1fU)

#define BCM2708_DMA_DEBUG_READ_ERROR (1 << 2)

#define BCM2835_DMA_SLICE_WORDS 256
#define BCM2835_DMA_SLICE_NS    1000

static void bcm2835_dma_schedule(BCM2835DMAChan *ch)
{
    timer_mod(ch->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                         BCM2835_DMA_SLICE_NS);
}

static void bcm2835_dma_stop(BCM2835DMAChan *ch)
{
    timer_del(ch->timer);
    ch->cs &= ~(BCM2708_DMA_ACTIVE | BCM2708_DMA_ISHELD);
    ch->cs |= BCM2708_DMA_ISPAUSED;
    ch->cb_loaded = false;
}

static bool bcm2835_dma_load_cb(BCM2835DMAState *s, unsigned c)
{
    BCM2835DMAChan *ch = &s->chan[c];

    /* CB fetch */
    ch->ti = ldl_le_phys(&s->dma_as, ch->conblk_ad);
    ch->source_ad = ldl_le_phys(&s->dma_as, ch->conblk_ad + 4);
    ch->dest_ad = ldl_le_phys(&s->dma_as, ch->conblk_ad + 8);
    ch->txfr_len = ldl_le_phys(&s->dma_as, ch->conblk_ad + 12);
    ch->stride = ldl_le_phys(&s->dma_as, ch->conblk_ad + 16);
    ch->nextconbk = ldl_le_phys(&s->dma_as, ch->conblk_ad + 20) &
                    BCM2708_DMA_CB_ALIGN_MASK;

    ch->ylen = 1;
    if (ch->ti & BCM2708_DMA_TDMODE) {
        ch->ylen += (ch->txfr_len >> 16) & 0x3fff;
        ch->xlen = ch->txfr_len & 0xffff;
        ch->dst_stride = (int16_t)(ch->stride >> 16);
        ch->src_stride = (int16_t)ch->stride;
    } else {
        ch->xlen = ch->txfr_len;
        ch->dst_stride = 0;
        ch->src_stride = 0;
    }
    ch->xlen_td = ch->xlen;

    ch->cb_loaded = true;
    ch->dreq_valid = true;
    return true;
}

static unsigned bcm2835_dma_permap(BCM2835DMAChan *ch)
{
    return (ch->ti >> BCM2708_DMA_PERMAP_SHIFT) &
           BCM2708_DMA_PERMAP_MASK;
}

static bool bcm2835_dma_dreq_level(BCM2835DMAState *s,
                                   BCM2835DMAChan *ch)
{
    unsigned permap;

    if (!ch->dreq_valid) {
        return false;
    }

    permap = bcm2835_dma_permap(ch);
    return permap == 0 || (s->dreq & (1U << permap));
}

static bool bcm2835_dma_dreq_ready(BCM2835DMAState *s,
                                   BCM2835DMAChan *ch)
{
    if (!(ch->ti & (BCM2708_DMA_S_DREQ | BCM2708_DMA_D_DREQ))) {
        return true;
    }

    return bcm2835_dma_dreq_level(s, ch);
}

static void bcm2835_dma_complete_cb(BCM2835DMAState *s, unsigned c)
{
    BCM2835DMAChan *ch = &s->chan[c];

    ch->cs |= BCM2708_DMA_END;
    if (ch->ti & BCM2708_DMA_INT_EN) {
        ch->cs |= BCM2708_DMA_INT;
        s->int_status |= 1 << c;
        qemu_set_irq(ch->irq, 1);
    }

    ch->conblk_ad = ch->nextconbk;
    ch->cb_loaded = false;
}

static bool bcm2835_dma_advance(BCM2835DMAState *s, unsigned c)
{
    BCM2835DMAChan *ch = &s->chan[c];

    assert(ch->xlen == 0);
    if (--ch->ylen != 0) {
        ch->source_ad += ch->src_stride;
        ch->dest_ad += ch->dst_stride;
        ch->xlen = ch->xlen_td;
        ch->txfr_len = ((ch->ylen - 1) << 16) | ch->xlen;
        return true;
    }

    bcm2835_dma_complete_cb(s, c);
    if (ch->conblk_ad == 0) {
        bcm2835_dma_stop(ch);
        return false;
    }
    return true;
}

static void bcm2835_dma_bus_error(BCM2835DMAState *s, unsigned c,
                                  bool is_read)
{
    BCM2835DMAChan *ch = &s->chan[c];

    if (is_read) {
        ch->debug |= BCM2708_DMA_DEBUG_READ_ERROR;
    }
    ch->cs |= BCM2708_DMA_ERR | BCM2708_DMA_INT;
    s->int_status |= 1 << c;
    qemu_set_irq(ch->irq, 1);
    timer_del(ch->timer);
    ch->cs &= ~(BCM2708_DMA_ACTIVE | BCM2708_DMA_ISHELD);
    ch->cs |= BCM2708_DMA_ISPAUSED;
}

static void bcm2835_dma_update(BCM2835DMAState *s, unsigned c)
{
    BCM2835DMAChan *ch = &s->chan[c];
    unsigned budget = BCM2835_DMA_SLICE_WORDS;

    timer_del(ch->timer);

    if (!(ch->cs & BCM2708_DMA_ACTIVE)) {
        ch->cs &= ~BCM2708_DMA_ISHELD;
        ch->cs |= BCM2708_DMA_ISPAUSED;
        return;
    }
    if (!(s->enable & (1 << c))) {
        ch->cs &= ~BCM2708_DMA_ISHELD;
        ch->cs |= BCM2708_DMA_ISPAUSED;
        return;
    }

    ch->cs &= ~(BCM2708_DMA_ISPAUSED | BCM2708_DMA_ISHELD);

    while (budget > 0 && (ch->cs & BCM2708_DMA_ACTIVE) &&
           (s->enable & (1 << c))) {
        uint8_t data[sizeof(uint32_t)] = { 0 };
        uint32_t transfer_len;

        if (!ch->cb_loaded) {
            if (ch->conblk_ad == 0) {
                bcm2835_dma_stop(ch);
                return;
            }
            if (!bcm2835_dma_load_cb(s, c)) {
                bcm2835_dma_stop(ch);
                return;
            }
        }

        if (!bcm2835_dma_dreq_ready(s, ch)) {
            ch->cs |= BCM2708_DMA_ISHELD;
            return;
        }

        if (ch->xlen == 0) {
            budget--;
            if (!bcm2835_dma_advance(s, c)) {
                return;
            }
            continue;
        }

        transfer_len = MIN(ch->xlen, (uint32_t)sizeof(data));
        if (!(ch->ti & BCM2708_DMA_S_IGNORE) &&
            dma_memory_read(&s->dma_as, ch->source_ad, data, transfer_len,
                            MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: DMA read failed at 0x%08x\n",
                          __func__, ch->source_ad);
            bcm2835_dma_bus_error(s, c, true);
            return;
        }
        if (!(ch->ti & BCM2708_DMA_D_IGNORE) &&
            dma_memory_write(&s->dma_as, ch->dest_ad, data, transfer_len,
                             MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: DMA write failed at 0x%08x\n",
                          __func__, ch->dest_ad);
            bcm2835_dma_bus_error(s, c, false);
            return;
        }
        if (ch->ti & BCM2708_DMA_S_INC) {
            ch->source_ad += transfer_len;
        }
        if (ch->ti & BCM2708_DMA_D_INC) {
            ch->dest_ad += transfer_len;
        }

        ch->xlen -= transfer_len;
        if (ch->ti & BCM2708_DMA_TDMODE) {
            ch->txfr_len = ((ch->ylen - 1) << 16) | ch->xlen;
        } else {
            ch->txfr_len = ch->xlen;
        }
        budget--;

        if (ch->xlen == 0 && !bcm2835_dma_advance(s, c)) {
            return;
        }
    }

    if ((ch->cs & BCM2708_DMA_ACTIVE) && (s->enable & (1 << c))) {
        if (ch->cb_loaded && !bcm2835_dma_dreq_ready(s, ch)) {
            ch->cs |= BCM2708_DMA_ISHELD;
            return;
        }
        bcm2835_dma_schedule(ch);
    } else {
        ch->cs |= BCM2708_DMA_ISPAUSED;
    }
}

static void bcm2835_dma_timer(void *opaque)
{
    BCM2835DMAChan *ch = opaque;

    bcm2835_dma_update(ch->dma, ch->channel);
}

static void bcm2835_dma_set_dreq(void *opaque, int n, int level)
{
    BCM2835DMAState *s = opaque;
    unsigned c;

    assert(n >= 0 && n < 32);

    if (!!(s->dreq & (1U << n)) == !!level) {
        return;
    }

    if (level) {
        s->dreq |= 1U << n;
    } else {
        s->dreq &= ~(1U << n);
    }

    for (c = 0; c < BCM2835_DMA_NCHANS; c++) {
        BCM2835DMAChan *ch = &s->chan[c];

        if (!(ch->cs & BCM2708_DMA_ACTIVE) ||
            !(s->enable & (1 << c)) || !ch->cb_loaded ||
            !(ch->ti & (BCM2708_DMA_S_DREQ | BCM2708_DMA_D_DREQ)) ||
            bcm2835_dma_permap(ch) != n) {
            continue;
        }

        if (level) {
            ch->cs &= ~BCM2708_DMA_ISHELD;
            /*
             * A DREQ edge is already a hardware pacing point.  Service one
             * bounded slice immediately so a streaming peripheral can refill
             * inside its catch-up batch.  Persistent requests still yield via
             * the channel timer when the slice budget is exhausted.
             */
            if (runstate_is_running()) {
                bcm2835_dma_update(s, c);
            } else {
                /* Do not mutate guest memory while the VM is stopped. */
                bcm2835_dma_schedule(ch);
            }
        } else {
            timer_del(ch->timer);
            ch->cs &= ~BCM2708_DMA_ISPAUSED;
            ch->cs |= BCM2708_DMA_ISHELD;
        }
    }
}

static void bcm2835_dma_chan_reset(BCM2835DMAState *s, unsigned c)
{
    BCM2835DMAChan *ch = &s->chan[c];

    timer_del(ch->timer);
    qemu_set_irq(ch->irq, 0);
    s->int_status &= ~(1 << c);
    ch->cs = 0;
    ch->conblk_ad = 0;
    ch->ti = 0;
    ch->source_ad = 0;
    ch->dest_ad = 0;
    ch->txfr_len = 0;
    ch->stride = 0;
    ch->nextconbk = 0;
    ch->debug = 0;
    ch->cb_loaded = false;
    ch->dreq_valid = false;
    ch->xlen = 0;
    ch->xlen_td = 0;
    ch->ylen = 0;
    ch->dst_stride = 0;
    ch->src_stride = 0;
}

static void bcm2835_dma_abort(BCM2835DMAState *s, unsigned c, bool was_active)
{
    BCM2835DMAChan *ch = &s->chan[c];
    uint32_t nextconbk = 0;

    if (!was_active) {
        return;
    }

    if (ch->cb_loaded) {
        nextconbk = ch->nextconbk;
    } else if (ch->conblk_ad != 0) {
        nextconbk = ldl_le_phys(&s->dma_as, ch->conblk_ad + 20) &
                    BCM2708_DMA_CB_ALIGN_MASK;
    }

    ch->conblk_ad = nextconbk;
    ch->cb_loaded = false;
    ch->xlen = 0;
    ch->xlen_td = 0;
    ch->ylen = 0;

    if (nextconbk == 0) {
        bcm2835_dma_stop(ch);
    }
}

static uint64_t bcm2835_dma_read(BCM2835DMAState *s, hwaddr offset,
                                 unsigned size, unsigned c)
{
    BCM2835DMAChan *ch;
    uint32_t res = 0;

    assert(size == 4);
    assert(c < BCM2835_DMA_NCHANS);

    ch = &s->chan[c];

    switch (offset) {
    case BCM2708_DMA_CS:
        res = ch->cs & ~BCM2708_DMA_DREQ;
        if (bcm2835_dma_dreq_level(s, ch)) {
            res |= BCM2708_DMA_DREQ;
        }
        break;
    case BCM2708_DMA_ADDR:
        res = ch->conblk_ad;
        break;
    case BCM2708_DMA_INFO:
        res = ch->ti;
        break;
    case BCM2708_DMA_SOURCE_AD:
        res = ch->source_ad;
        break;
    case BCM2708_DMA_DEST_AD:
        res = ch->dest_ad;
        break;
    case BCM2708_DMA_TXFR_LEN:
        res = ch->txfr_len;
        break;
    case BCM2708_DMA_STRIDE:
        res = ch->stride;
        break;
    case BCM2708_DMA_NEXTCB:
        res = ch->nextconbk;
        break;
    case BCM2708_DMA_DEBUG:
        res = ch->debug;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                      __func__, offset);
        break;
    }
    return res;
}

static void bcm2835_dma_write(BCM2835DMAState *s, hwaddr offset,
                              uint64_t value, unsigned size, unsigned c)
{
    BCM2835DMAChan *ch;
    uint32_t oldcs;

    assert(size == 4);
    assert(c < BCM2835_DMA_NCHANS);

    ch = &s->chan[c];

    switch (offset) {
    case BCM2708_DMA_CS:
        oldcs = ch->cs;
        if (value & BCM2708_DMA_RESET) {
            bcm2835_dma_chan_reset(s, c);
            oldcs = 0;
        }
        if (value & BCM2708_DMA_END) {
            ch->cs &= ~BCM2708_DMA_END;
        }
        if (value & BCM2708_DMA_INT) {
            ch->cs &= ~BCM2708_DMA_INT;
            s->int_status &= ~(1 << c);
            qemu_set_irq(ch->irq, 0);
        }
        ch->cs &= ~BCM2708_DMA_CS_RW_MASK;
        ch->cs |= (value & BCM2708_DMA_CS_RW_MASK);

        if (value & BCM2708_DMA_ABORT) {
            bcm2835_dma_abort(s, c, oldcs & BCM2708_DMA_ACTIVE);
        }

        if (!(ch->cs & BCM2708_DMA_ACTIVE)) {
            timer_del(ch->timer);
            ch->cs &= ~BCM2708_DMA_ISHELD;
            ch->cs |= BCM2708_DMA_ISPAUSED;
        } else if (!(oldcs & BCM2708_DMA_ACTIVE) ||
                   (value & BCM2708_DMA_ABORT)) {
            bcm2835_dma_update(s, c);
        }
        break;
    case BCM2708_DMA_ADDR:
        ch->conblk_ad = value & BCM2708_DMA_CB_ALIGN_MASK;
        ch->cb_loaded = false;
        break;
    case BCM2708_DMA_DEBUG:
        ch->debug &= ~(value & 0x7);
        if (!(ch->debug & 0x7)) {
            ch->cs &= ~BCM2708_DMA_ERR;
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                      __func__, offset);
        break;
    }
}

static uint64_t bcm2835_dma0_read(void *opaque, hwaddr offset, unsigned size)
{
    BCM2835DMAState *s = opaque;

    if (offset < 0xf00) {
        return bcm2835_dma_read(s, (offset & 0xff), size, (offset >> 8) & 0xf);
    } else {
        switch (offset) {
        case BCM2708_DMA_INT_STATUS:
            return s->int_status;
        case BCM2708_DMA_ENABLE:
            return s->enable;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                          __func__, offset);
            return 0;
        }
    }
}

static uint64_t bcm2835_dma15_read(void *opaque, hwaddr offset, unsigned size)
{
    return bcm2835_dma_read(opaque, (offset & 0xff), size, 15);
}

static void bcm2835_dma0_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned size)
{
    BCM2835DMAState *s = opaque;

    if (offset < 0xf00) {
        bcm2835_dma_write(s, (offset & 0xff), value, size, (offset >> 8) & 0xf);
    } else {
        switch (offset) {
        case BCM2708_DMA_INT_STATUS:
            break;
        case BCM2708_DMA_ENABLE: {
            uint32_t old_enable = s->enable;
            unsigned c;

            s->enable = value & 0xffff;
            for (c = 0; c < BCM2835_DMA_NCHANS; c++) {
                BCM2835DMAChan *ch = &s->chan[c];
                uint32_t mask = 1 << c;

                if ((old_enable & mask) && !(s->enable & mask)) {
                    timer_del(ch->timer);
                    if (ch->cs & BCM2708_DMA_ACTIVE) {
                        ch->cs &= ~BCM2708_DMA_ISHELD;
                        ch->cs |= BCM2708_DMA_ISPAUSED;
                    }
                } else if (!(old_enable & mask) && (s->enable & mask) &&
                           (ch->cs & BCM2708_DMA_ACTIVE)) {
                    bcm2835_dma_update(s, c);
                }
            }
            break;
        }
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n",
                          __func__, offset);
        }
    }

}

static void bcm2835_dma15_write(void *opaque, hwaddr offset, uint64_t value,
                                unsigned size)
{
    bcm2835_dma_write(opaque, (offset & 0xff), value, size, 15);
}

static const MemoryRegionOps bcm2835_dma0_ops = {
    .read = bcm2835_dma0_read,
    .write = bcm2835_dma0_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const MemoryRegionOps bcm2835_dma15_ops = {
    .read = bcm2835_dma15_read,
    .write = bcm2835_dma15_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const VMStateDescription vmstate_bcm2835_dma_chan = {
    .name = TYPE_BCM2835_DMA "-chan",
    .version_id = 2,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(cs, BCM2835DMAChan),
        VMSTATE_UINT32(conblk_ad, BCM2835DMAChan),
        VMSTATE_UINT32(ti, BCM2835DMAChan),
        VMSTATE_UINT32(source_ad, BCM2835DMAChan),
        VMSTATE_UINT32(dest_ad, BCM2835DMAChan),
        VMSTATE_UINT32(txfr_len, BCM2835DMAChan),
        VMSTATE_UINT32(stride, BCM2835DMAChan),
        VMSTATE_UINT32(nextconbk, BCM2835DMAChan),
        VMSTATE_UINT32(debug, BCM2835DMAChan),
        VMSTATE_BOOL_V(cb_loaded, BCM2835DMAChan, 2),
        VMSTATE_BOOL_V(dreq_valid, BCM2835DMAChan, 2),
        VMSTATE_UINT32_V(xlen, BCM2835DMAChan, 2),
        VMSTATE_UINT32_V(xlen_td, BCM2835DMAChan, 2),
        VMSTATE_UINT32_V(ylen, BCM2835DMAChan, 2),
        VMSTATE_INT32_V(dst_stride, BCM2835DMAChan, 2),
        VMSTATE_INT32_V(src_stride, BCM2835DMAChan, 2),
        VMSTATE_TIMER_PTR_V(timer, BCM2835DMAChan, 2),
        VMSTATE_END_OF_LIST()
    }
};

static int bcm2835_dma_post_load(void *opaque, int version_id)
{
    BCM2835DMAState *s = opaque;
    unsigned c;

    s->int_status = 0;
    for (c = 0; c < BCM2835_DMA_NCHANS; c++) {
        BCM2835DMAChan *ch = &s->chan[c];

        if (version_id < 2) {
            timer_del(ch->timer);
            ch->cb_loaded = false;
            ch->dreq_valid = false;
            ch->xlen = 0;
            ch->xlen_td = 0;
            ch->ylen = 0;
            ch->dst_stride = 0;
            ch->src_stride = 0;
        } else if (ch->cb_loaded &&
                   (ch->xlen > ch->xlen_td || ch->ylen == 0)) {
            return -EINVAL;
        }

        if (ch->cs & BCM2708_DMA_INT) {
            s->int_status |= 1 << c;
            qemu_set_irq(ch->irq, 1);
        } else {
            qemu_set_irq(ch->irq, 0);
        }

        if (!(ch->cs & BCM2708_DMA_ACTIVE)) {
            timer_del(ch->timer);
            ch->cs &= ~BCM2708_DMA_ISHELD;
            ch->cs |= BCM2708_DMA_ISPAUSED;
        } else if (!(s->enable & (1 << c))) {
            timer_del(ch->timer);
            ch->cs &= ~BCM2708_DMA_ISHELD;
            ch->cs |= BCM2708_DMA_ISPAUSED;
        } else if (ch->cb_loaded &&
                   !bcm2835_dma_dreq_ready(s, ch)) {
            timer_del(ch->timer);
            ch->cs &= ~BCM2708_DMA_ISPAUSED;
            ch->cs |= BCM2708_DMA_ISHELD;
        } else {
            ch->cs &= ~(BCM2708_DMA_ISPAUSED | BCM2708_DMA_ISHELD);
            if (!timer_pending(ch->timer)) {
                bcm2835_dma_schedule(ch);
            }
        }
    }

    return 0;
}

static const VMStateDescription vmstate_bcm2835_dma = {
    .name = TYPE_BCM2835_DMA,
    .version_id = 2,
    .minimum_version_id = 1,
    .post_load = bcm2835_dma_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_STRUCT_ARRAY(chan, BCM2835DMAState, BCM2835_DMA_NCHANS, 1,
                             vmstate_bcm2835_dma_chan, BCM2835DMAChan),
        VMSTATE_UINT32(int_status, BCM2835DMAState),
        VMSTATE_UINT32(enable, BCM2835DMAState),
        VMSTATE_UINT32_V(dreq, BCM2835DMAState, 2),
        VMSTATE_END_OF_LIST()
    }
};

static void bcm2835_dma_init(Object *obj)
{
    BCM2835DMAState *s = BCM2835_DMA(obj);
    int n;

    /* DMA channels 0-14 occupy a contiguous block of IO memory, along
     * with the global enable and interrupt status bits. Channel 15
     * has the same register map, but is mapped at a discontiguous
     * address in a separate IO block.
     */
    memory_region_init_io(&s->iomem0, OBJECT(s), &bcm2835_dma0_ops, s,
                          TYPE_BCM2835_DMA, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem0);

    memory_region_init_io(&s->iomem15, OBJECT(s), &bcm2835_dma15_ops, s,
                          TYPE_BCM2835_DMA "-chan15", 0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem15);

    for (n = 0; n < 16; n++) {
        sysbus_init_irq(SYS_BUS_DEVICE(s), &s->chan[n].irq);
    }
    qdev_init_gpio_in_named(DEVICE(s), bcm2835_dma_set_dreq, "dreq", 32);
}

static void bcm2835_dma_reset(DeviceState *dev)
{
    BCM2835DMAState *s = BCM2835_DMA(dev);
    int n;

    s->enable = 0xffff;
    s->int_status = 0;
    s->dreq = 0;
    for (n = 0; n < BCM2835_DMA_NCHANS; n++) {
        bcm2835_dma_chan_reset(s, n);
    }
}

static void bcm2835_dma_realize(DeviceState *dev, Error **errp)
{
    BCM2835DMAState *s = BCM2835_DMA(dev);
    Object *obj;
    unsigned c;

    obj = object_property_get_link(OBJECT(dev), "dma-mr", &error_abort);
    s->dma_mr = MEMORY_REGION(obj);
    address_space_init(&s->dma_as, s->dma_mr, TYPE_BCM2835_DMA "-memory");

    for (c = 0; c < BCM2835_DMA_NCHANS; c++) {
        BCM2835DMAChan *ch = &s->chan[c];

        ch->dma = s;
        ch->channel = c;
        ch->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, bcm2835_dma_timer, ch);
    }

    bcm2835_dma_reset(dev);
}

static void bcm2835_dma_unrealize(DeviceState *dev)
{
    BCM2835DMAState *s = BCM2835_DMA(dev);
    unsigned c;

    for (c = 0; c < BCM2835_DMA_NCHANS; c++) {
        timer_free(s->chan[c].timer);
        s->chan[c].timer = NULL;
    }
    address_space_destroy(&s->dma_as);
}

static void bcm2835_dma_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = bcm2835_dma_realize;
    dc->unrealize = bcm2835_dma_unrealize;
    device_class_set_legacy_reset(dc, bcm2835_dma_reset);
    dc->vmsd = &vmstate_bcm2835_dma;
}

static const TypeInfo bcm2835_dma_info = {
    .name          = TYPE_BCM2835_DMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2835DMAState),
    .class_init    = bcm2835_dma_class_init,
    .instance_init = bcm2835_dma_init,
};

static void bcm2835_dma_register_types(void)
{
    type_register_static(&bcm2835_dma_info);
}

type_init(bcm2835_dma_register_types)
