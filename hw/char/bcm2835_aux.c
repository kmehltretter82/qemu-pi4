/*
 * BCM2835 (Raspberry Pi / Pi 2) Aux block (mini UART and SPI).
 * Copyright (c) 2015, Microsoft
 * Written by Andrew Baumann
 * Based on pl011.c, copyright terms below:
 *
 * Arm PrimeCell PL011 UART
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licensed under the GPL.
 *
 * The following features/registers are unimplemented:
 *  - Line control
 *  - Extra control
 *  - Baudrate
 *  - AUX SPI DMA, clock timing and GPIO chip-select wiring
 */

#include "qemu/osdep.h"
#include "chardev/char-serial.h"
#include "hw/char/bcm2835_aux.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/ssi/ssi.h"
#include "migration/vmstate.h"
#include "qemu/fifo8.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define AUX_IRQ         0x0
#define AUX_ENABLES     0x4
#define AUX_MU_IO_REG   0x40
#define AUX_MU_IER_REG  0x44
#define AUX_MU_IIR_REG  0x48
#define AUX_MU_LCR_REG  0x4c
#define AUX_MU_MCR_REG  0x50
#define AUX_MU_LSR_REG  0x54
#define AUX_MU_MSR_REG  0x58
#define AUX_MU_SCRATCH  0x5c
#define AUX_MU_CNTL_REG 0x60
#define AUX_MU_STAT_REG 0x64
#define AUX_MU_BAUD_REG 0x68

#define AUX_SPI1_BASE 0x80
#define AUX_SPI2_BASE 0xc0
#define AUX_SPI_REG_SIZE 0x40
#define AUX_SPI_CNTL0 0x00
#define AUX_SPI_CNTL1 0x04
#define AUX_SPI_STAT 0x08
#define AUX_SPI_PEEK 0x0c
#define AUX_SPI_IO 0x20
#define AUX_SPI_TXHOLD 0x30

#define AUX_SPI_CNTL0_VAR_WIDTH 0x00004000
#define AUX_SPI_CNTL0_CS_MASK 0x000e0000
#define AUX_SPI_CNTL0_CS_SHIFT 17
#define AUX_SPI_CNTL0_ENABLE 0x00000800
#define AUX_SPI_CNTL0_CLEARFIFO 0x00000200
#define AUX_SPI_CNTL0_MSBF_OUT 0x00000040

#define AUX_SPI_CNTL1_TXEMPTY 0x00000080
#define AUX_SPI_CNTL1_IDLE 0x00000040
#define AUX_SPI_CNTL1_MSBF_IN 0x00000002

#define AUX_SPI_STAT_RX_LVL_SHIFT 16
#define AUX_SPI_STAT_TX_EMPTY 0x00000200
#define AUX_SPI_STAT_RX_FULL 0x00000100
#define AUX_SPI_STAT_RX_EMPTY 0x00000080

#define AUX_SPI_CS_COUNT 8

/* bits in IER/IIR registers */
#define RX_INT  0x1
#define TX_INT  0x2

/* supported bits in the modem control and status registers */
#define MCR_RTS  0x2
#define MSR_CTS  0x10

/* Bits in the shared enable and interrupt-status registers. */
#define ENABLE_UART 0x1
#define ENABLE_SPI1 0x2
#define ENABLE_SPI2 0x4
#define AUX_IRQ_UART 0x1
#define AUX_IRQ_SPI1 0x2
#define AUX_IRQ_SPI2 0x4

static bool bcm2835_aux_uart_enabled(BCM2835AuxState *s)
{
    return s->enables & ENABLE_UART;
}

static bool bcm2835_aux_spi_enabled(BCM2835AuxState *s, unsigned int index)
{
    return s->enables & (ENABLE_SPI1 << index);
}

static uint32_t bcm2835_aux_spi_status(BCM2835AuxSPIState *spi)
{
    uint32_t status = AUX_SPI_STAT_TX_EMPTY;
    uint32_t rx_level = fifo8_num_used(&spi->rx_fifo);

    if (rx_level == 0) {
        status |= AUX_SPI_STAT_RX_EMPTY;
    }
    if (fifo8_is_full(&spi->rx_fifo)) {
        status |= AUX_SPI_STAT_RX_FULL;
    }
    return status | (rx_level << AUX_SPI_STAT_RX_LVL_SHIFT);
}

static bool bcm2835_aux_spi_irq_pending(BCM2835AuxSPIState *spi)
{
    /* Transfers complete synchronously in this bounded PIO model. */
    return spi->cntl1 & (AUX_SPI_CNTL1_TXEMPTY | AUX_SPI_CNTL1_IDLE);
}

static uint32_t bcm2835_aux_irq_status(BCM2835AuxState *s)
{
    uint32_t status = 0;

    if (s->iir) {
        status |= AUX_IRQ_UART;
    }
    if (bcm2835_aux_spi_irq_pending(&s->spi[0])) {
        status |= AUX_IRQ_SPI1;
    }
    if (bcm2835_aux_spi_irq_pending(&s->spi[1])) {
        status |= AUX_IRQ_SPI2;
    }
    return status;
}

static void bcm2835_aux_update(BCM2835AuxState *s)
{
    /* signal an interrupt if either:
     * 1. rx interrupt is enabled and we have a non-empty rx fifo, or
     * 2. the tx interrupt is enabled (since we instantly drain the tx fifo)
     */
    s->iir = 0;
    if ((s->ier & RX_INT) && s->read_count != 0) {
        s->iir |= RX_INT;
    }
    if (s->ier & TX_INT) {
        s->iir |= TX_INT;
    }
    qemu_set_irq(s->irq, bcm2835_aux_irq_status(s) != 0);
}

static void bcm2835_aux_update_rts(BCM2835AuxState *s)
{
    int flags;

    if (qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_GET_TIOCM, &flags)) {
        return;
    }

    flags &= ~CHR_TIOCM_RTS;
    if (s->mcr & MCR_RTS) {
        flags |= CHR_TIOCM_RTS;
    }
    qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_SET_TIOCM, &flags);
}

static BCM2835AuxSPIState *bcm2835_aux_spi_from_offset(BCM2835AuxState *s,
                                                         hwaddr offset,
                                                         hwaddr *reg)
{
    unsigned int index;
    hwaddr base;

    if (offset >= AUX_SPI1_BASE &&
        offset < AUX_SPI1_BASE + AUX_SPI_REG_SIZE) {
        index = 0;
        base = AUX_SPI1_BASE;
    } else if (offset >= AUX_SPI2_BASE &&
               offset < AUX_SPI2_BASE + AUX_SPI_REG_SIZE) {
        index = 1;
        base = AUX_SPI2_BASE;
    } else {
        return NULL;
    }

    *reg = offset - base;
    return &s->spi[index];
}

/*
 * AUX SPI has native chip-select signals, but the board GPIO pin routing is
 * not modeled.  For an explicitly attached QEMU SSI peripheral, preserve the
 * PIO driver's transaction boundary: TXHOLD keeps the selected SSI target
 * active and the final IO word deasserts it.  This is a virtual-bus aid, not
 * a claim that the board's GPIO chip-select wiring is emulated.
 */
static void bcm2835_aux_spi_drive_cs(BCM2835AuxSPIState *spi,
                                     unsigned int cs, bool select)
{
    DeviceState *dev;
    SSIPeripheral *peripheral;
    qemu_irq cs_line;
    int level;

    if (!spi->bus) {
        return;
    }
    dev = ssi_get_cs(spi->bus, cs);
    if (!dev) {
        return;
    }
    peripheral = SSI_PERIPHERAL(dev);
    if (peripheral->spc->cs_polarity == SSI_CS_NONE) {
        return;
    }

    cs_line = qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0);
    level = peripheral->spc->cs_polarity == SSI_CS_LOW ? !select : select;
    qemu_set_irq(cs_line, level);
}

static void bcm2835_aux_spi_deselect_all(BCM2835AuxSPIState *spi)
{
    for (unsigned int cs = 0; cs < AUX_SPI_CS_COUNT; cs++) {
        bcm2835_aux_spi_drive_cs(spi, cs, false);
    }
}

static void bcm2835_aux_spi_select(BCM2835AuxSPIState *spi)
{
    unsigned int selected =
        (spi->cntl0 & AUX_SPI_CNTL0_CS_MASK) >> AUX_SPI_CNTL0_CS_SHIFT;

    for (unsigned int cs = 0; cs < AUX_SPI_CS_COUNT; cs++) {
        if (cs != selected) {
            bcm2835_aux_spi_drive_cs(spi, cs, false);
        }
    }
    bcm2835_aux_spi_drive_cs(spi, selected, true);
}

static uint32_t bcm2835_aux_spi_fifo_word(BCM2835AuxSPIState *spi,
                                           bool pop)
{
    uint8_t bytes[3];
    uint32_t value = 0;
    uint32_t count = MIN(fifo8_num_used(&spi->rx_fifo), ARRAY_SIZE(bytes));

    if (count == 0) {
        return 0;
    }

    if (pop) {
        fifo8_pop_buf(&spi->rx_fifo, bytes, count);
    } else {
        fifo8_peek_buf(&spi->rx_fifo, bytes, count);
    }

    /*
     * Transmit and receive alignment differ.  A variable-width word is sent
     * from the most significant bits, so the write path takes byte i from
     * bit 8 * (2 - i).  Received bits shift in from the bottom, so an
     * n-byte word is returned right-aligned with the first byte received in
     * the most significant position of that n-byte field.  Linux's
     * spi-bcm2835aux reads it back as data >> (8 * (count - i - 1)).
     */
    for (unsigned int i = 0; i < count; i++) {
        value |= bytes[i] << (8 * (count - 1 - i));
    }
    return value;
}

static void bcm2835_aux_spi_transfer(BCM2835AuxSPIState *spi,
                                      uint32_t value)
{
    unsigned int count;

    if (!(spi->cntl0 & AUX_SPI_CNTL0_ENABLE)) {
        return;
    }

    count = (value >> 24) & 0x3f;
    if (!(spi->cntl0 & AUX_SPI_CNTL0_VAR_WIDTH) ||
        !(spi->cntl0 & AUX_SPI_CNTL0_MSBF_OUT) ||
        !(spi->cntl1 & AUX_SPI_CNTL1_MSBF_IN) ||
        count == 0 || count > 24 || count % 8) {
        qemu_log_mask(LOG_UNIMP,
                      "%s: unsupported AUX SPI transfer configuration\n",
                      __func__);
        return;
    }

    count /= 8;
    if (fifo8_num_free(&spi->rx_fifo) < count) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: AUX SPI receive FIFO full\n", __func__);
        return;
    }

    bcm2835_aux_spi_select(spi);

    for (unsigned int i = 0; i < count; i++) {
        uint8_t tx = value >> (8 * (2 - i));
        uint8_t rx = ssi_transfer(spi->bus, tx);

        fifo8_push(&spi->rx_fifo, rx);
    }
}

static uint32_t bcm2835_aux_spi_read(BCM2835AuxSPIState *spi, hwaddr reg)
{
    switch (reg) {
    case AUX_SPI_CNTL0:
        return spi->cntl0;
    case AUX_SPI_CNTL1:
        return spi->cntl1;
    case AUX_SPI_STAT:
        return bcm2835_aux_spi_status(spi);
    case AUX_SPI_PEEK:
        return bcm2835_aux_spi_fifo_word(spi, false);
    case AUX_SPI_IO:
        return bcm2835_aux_spi_fifo_word(spi, true);
    case AUX_SPI_TXHOLD:
        return 0;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: bad AUX SPI offset 0x%" HWADDR_PRIx "\n",
                      __func__, reg);
        return 0;
    }
}

static void bcm2835_aux_spi_write(BCM2835AuxSPIState *spi, hwaddr reg,
                                  uint32_t value)
{
    switch (reg) {
    case AUX_SPI_CNTL0:
        spi->cntl0 = value & ~AUX_SPI_CNTL0_CLEARFIFO;
        if (value & AUX_SPI_CNTL0_CLEARFIFO) {
            fifo8_reset(&spi->rx_fifo);
        }
        if (!(spi->cntl0 & AUX_SPI_CNTL0_ENABLE)) {
            bcm2835_aux_spi_deselect_all(spi);
        }
        break;
    case AUX_SPI_CNTL1:
        spi->cntl1 = value;
        break;
    case AUX_SPI_IO:
        bcm2835_aux_spi_transfer(spi, value);
        bcm2835_aux_spi_deselect_all(spi);
        break;
    case AUX_SPI_TXHOLD:
        bcm2835_aux_spi_transfer(spi, value);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: bad AUX SPI offset 0x%" HWADDR_PRIx "\n",
                      __func__, reg);
    }
}

static uint64_t bcm2835_aux_read(void *opaque, hwaddr offset, unsigned size)
{
    BCM2835AuxState *s = opaque;
    BCM2835AuxSPIState *spi;
    uint32_t c, res;
    hwaddr spi_reg;
    int flags;

    spi = bcm2835_aux_spi_from_offset(s, offset, &spi_reg);
    if (spi) {
        unsigned int index = (unsigned int)(spi - s->spi);

        if (!bcm2835_aux_spi_enabled(s, index)) {
            return 0;
        }
        res = bcm2835_aux_spi_read(spi, spi_reg);
        bcm2835_aux_update(s);
        return res;
    }

    /* The disabled mini UART's register bank reads as zero. */
    if (!bcm2835_aux_uart_enabled(s) &&
        offset >= AUX_MU_IO_REG && offset <= AUX_MU_BAUD_REG) {
        return 0;
    }

    switch (offset) {
    case AUX_IRQ:
        return bcm2835_aux_irq_status(s);

    case AUX_ENABLES:
        return s->enables;

    case AUX_MU_IO_REG:
        /* "DLAB bit set means access baudrate register" is NYI */
        c = s->read_fifo[s->read_pos];
        if (s->read_count > 0) {
            s->read_count--;
            if (++s->read_pos == BCM2835_AUX_RX_FIFO_LEN) {
                s->read_pos = 0;
            }
        }
        qemu_chr_fe_accept_input(&s->chr);
        bcm2835_aux_update(s);
        return c;

    case AUX_MU_IER_REG:
        /* "DLAB bit set means access baudrate register" is NYI */
        return s->ier;

    case AUX_MU_IIR_REG:
        res = 0xc0; /* FIFO enables */
        /* The spec is unclear on what happens when both tx and rx
         * interrupts are active, besides that this cannot occur. At
         * present, we choose to prioritise the rx interrupt, since
         * the tx fifo is always empty. */
        if ((s->iir & RX_INT) && s->read_count != 0) {
            res |= 0x4;
        } else {
            res |= 0x2;
        }
        if (s->iir == 0) {
            res |= 0x1;
        }
        return res;

    case AUX_MU_LCR_REG:
        qemu_log_mask(LOG_UNIMP, "%s: AUX_MU_LCR_REG unsupported\n", __func__);
        return 0;

    case AUX_MU_MCR_REG:
        return s->mcr;

    case AUX_MU_LSR_REG:
        res = 0x60; /* tx idle, empty */
        if (s->read_count != 0) {
            res |= 0x1;
        }
        return res;

    case AUX_MU_MSR_REG:
        if (qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_GET_TIOCM,
                              &flags) == 0) {
            return (flags & CHR_TIOCM_CTS) ? MSR_CTS : 0;
        }
        return MSR_CTS;

    case AUX_MU_SCRATCH:
        return s->scratch;

    case AUX_MU_CNTL_REG:
        return 0x3; /* tx, rx enabled */

    case AUX_MU_STAT_REG:
        res = 0x30e; /* space in the output buffer, empty tx fifo, idle tx/rx */
        if (s->read_count > 0) {
            res |= 0x1; /* data in input buffer */
            assert(s->read_count <= BCM2835_AUX_RX_FIFO_LEN);
            res |= ((uint32_t)s->read_count) << 16; /* rx fifo fill level */
        }
        return res;

    case AUX_MU_BAUD_REG:
        qemu_log_mask(LOG_UNIMP, "%s: AUX_MU_BAUD_REG unsupported\n", __func__);
        return 0;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %"HWADDR_PRIx"\n",
                      __func__, offset);
        return 0;
    }
}

static void bcm2835_aux_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned size)
{
    BCM2835AuxState *s = opaque;
    BCM2835AuxSPIState *spi;
    unsigned char ch;
    hwaddr spi_reg;
    bool uart_was_enabled;
    uint8_t old_enables;

    spi = bcm2835_aux_spi_from_offset(s, offset, &spi_reg);
    if (spi) {
        unsigned int index = (unsigned int)(spi - s->spi);

        /* Pi 400 hardware ignores AUX SPI-bank writes while gated off. */
        if (bcm2835_aux_spi_enabled(s, index)) {
            bcm2835_aux_spi_write(spi, spi_reg, value);
        }
        bcm2835_aux_update(s);
        return;
    }

    switch (offset) {
    case AUX_ENABLES:
        uart_was_enabled = bcm2835_aux_uart_enabled(s);
        old_enables = s->enables;
        s->enables = (uint8_t)value;
        for (unsigned int i = 0; i < BCM2835_AUX_SPI_COUNT; i++) {
            if ((old_enables & (ENABLE_SPI1 << i)) &&
                !(s->enables & (ENABLE_SPI1 << i))) {
                bcm2835_aux_spi_deselect_all(&s->spi[i]);
            }
        }
        if (!uart_was_enabled && bcm2835_aux_uart_enabled(s)) {
            qemu_chr_fe_accept_input(&s->chr);
        }
        break;

    case AUX_MU_IO_REG:
        /* "DLAB bit set means access baudrate register" is NYI */
        if (!bcm2835_aux_uart_enabled(s)) {
            break;
        }
        ch = value;
        /* XXX this blocks entire thread. Rewrite to use
         * qemu_chr_fe_write and background I/O callbacks */
        qemu_chr_fe_write_all(&s->chr, &ch, 1);
        break;

    case AUX_MU_IER_REG:
        /* "DLAB bit set means access baudrate register" is NYI */
        s->ier = value & (TX_INT | RX_INT);
        bcm2835_aux_update(s);
        break;

    case AUX_MU_IIR_REG:
        if (value & 0x2) {
            s->read_pos = 0;
            s->read_count = 0;
            qemu_chr_fe_accept_input(&s->chr);
        }
        break;

    case AUX_MU_LCR_REG:
        qemu_log_mask(LOG_UNIMP, "%s: AUX_MU_LCR_REG unsupported\n", __func__);
        break;

    case AUX_MU_MCR_REG:
        s->mcr = value & MCR_RTS;
        bcm2835_aux_update_rts(s);
        break;

    case AUX_MU_SCRATCH:
        s->scratch = (uint8_t)value;
        break;

    case AUX_MU_CNTL_REG:
        qemu_log_mask(LOG_UNIMP, "%s: AUX_MU_CNTL_REG unsupported\n", __func__);
        break;

    case AUX_MU_BAUD_REG:
        qemu_log_mask(LOG_UNIMP, "%s: AUX_MU_BAUD_REG unsupported\n", __func__);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %"HWADDR_PRIx"\n",
                      __func__, offset);
    }

    bcm2835_aux_update(s);
}

static int bcm2835_aux_can_receive(void *opaque)
{
    BCM2835AuxState *s = opaque;

    if (!bcm2835_aux_uart_enabled(s)) {
        return 0;
    }
    return BCM2835_AUX_RX_FIFO_LEN - s->read_count;
}

static void bcm2835_aux_put_fifo(void *opaque, uint8_t value)
{
    BCM2835AuxState *s = opaque;
    int slot;

    slot = s->read_pos + s->read_count;
    if (slot >= BCM2835_AUX_RX_FIFO_LEN) {
        slot -= BCM2835_AUX_RX_FIFO_LEN;
    }
    s->read_fifo[slot] = value;
    s->read_count++;
    if (s->read_count == BCM2835_AUX_RX_FIFO_LEN) {
        /* buffer full */
    }
    bcm2835_aux_update(s);
}

static void bcm2835_aux_receive(void *opaque, const uint8_t *buf, int size)
{
    BCM2835AuxState *s = opaque;

    if (!bcm2835_aux_uart_enabled(s)) {
        return;
    }
    for (int i = 0; i < size; i++) {
        bcm2835_aux_put_fifo(s, buf[i]);
    }
}

static const MemoryRegionOps bcm2835_aux_ops = {
    .read = bcm2835_aux_read,
    .write = bcm2835_aux_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

static int bcm2835_aux_post_load(void *opaque, int version_id)
{
    BCM2835AuxState *s = opaque;

    if (version_id < 3) {
        /* Older versions modeled the mini UART as permanently enabled. */
        s->enables = ENABLE_UART;
    }
    if (version_id < 4) {
        for (unsigned int i = 0; i < BCM2835_AUX_SPI_COUNT; i++) {
            fifo8_reset(&s->spi[i].rx_fifo);
            s->spi[i].cntl0 = 0;
            s->spi[i].cntl1 = 0;
        }
    }
    bcm2835_aux_update(s);
    bcm2835_aux_update_rts(s);
    if (bcm2835_aux_uart_enabled(s)) {
        qemu_chr_fe_accept_input(&s->chr);
    }
    return 0;
}

static const VMStateDescription vmstate_bcm2835_aux_spi = {
    .name = TYPE_BCM2835_AUX "/spi",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_FIFO8(rx_fifo, BCM2835AuxSPIState),
        VMSTATE_UINT32(cntl0, BCM2835AuxSPIState),
        VMSTATE_UINT32(cntl1, BCM2835AuxSPIState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_bcm2835_aux = {
    .name = TYPE_BCM2835_AUX,
    .version_id = 4,
    .minimum_version_id = 1,
    .post_load = bcm2835_aux_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8_ARRAY(read_fifo, BCM2835AuxState,
                            BCM2835_AUX_RX_FIFO_LEN),
        VMSTATE_UINT8(read_pos, BCM2835AuxState),
        VMSTATE_UINT8(read_count, BCM2835AuxState),
        VMSTATE_UINT8(ier, BCM2835AuxState),
        VMSTATE_UINT8(iir, BCM2835AuxState),
        VMSTATE_UINT8_V(mcr, BCM2835AuxState, 2),
        VMSTATE_UINT8_V(enables, BCM2835AuxState, 3),
        VMSTATE_UINT8_V(scratch, BCM2835AuxState, 3),
        VMSTATE_STRUCT_ARRAY(spi, BCM2835AuxState, BCM2835_AUX_SPI_COUNT,
                             4, vmstate_bcm2835_aux_spi,
                             BCM2835AuxSPIState),
        VMSTATE_END_OF_LIST()
    }
};

static void bcm2835_aux_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    BCM2835AuxState *s = BCM2835_AUX(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &bcm2835_aux_ops, s,
                          TYPE_BCM2835_AUX, 0x100);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    for (unsigned int i = 0; i < BCM2835_AUX_SPI_COUNT; i++) {
        fifo8_create(&s->spi[i].rx_fifo, BCM2835_AUX_SPI_RX_FIFO_LEN);
    }
}

static void bcm2835_aux_realize(DeviceState *dev, Error **errp)
{
    BCM2835AuxState *s = BCM2835_AUX(dev);

    qemu_chr_fe_set_handlers(&s->chr, bcm2835_aux_can_receive,
                             bcm2835_aux_receive, NULL, NULL, s, NULL, true);
    s->spi[0].bus = ssi_create_bus(dev, "spi1");
    s->spi[1].bus = ssi_create_bus(dev, "spi2");
}

static void bcm2835_aux_reset(DeviceState *dev)
{
    BCM2835AuxState *s = BCM2835_AUX(dev);

    memset(s->read_fifo, 0, sizeof(s->read_fifo));
    s->read_pos = 0;
    s->read_count = 0;
    s->ier = 0;
    s->iir = 0;
    s->mcr = 0;
    s->enables = 0;
    s->scratch = 0;
    for (unsigned int i = 0; i < BCM2835_AUX_SPI_COUNT; i++) {
        bcm2835_aux_spi_deselect_all(&s->spi[i]);
        fifo8_reset(&s->spi[i].rx_fifo);
        s->spi[i].cntl0 = 0;
        s->spi[i].cntl1 = 0;
    }
    bcm2835_aux_update(s);
    bcm2835_aux_update_rts(s);
    qemu_chr_fe_accept_input(&s->chr);
}

static void bcm2835_aux_finalize(Object *obj)
{
    BCM2835AuxState *s = BCM2835_AUX(obj);

    for (unsigned int i = 0; i < BCM2835_AUX_SPI_COUNT; i++) {
        fifo8_destroy(&s->spi[i].rx_fifo);
    }
}

static const Property bcm2835_aux_props[] = {
    DEFINE_PROP_CHR("chardev", BCM2835AuxState, chr),
};

static void bcm2835_aux_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = bcm2835_aux_realize;
    device_class_set_legacy_reset(dc, bcm2835_aux_reset);
    dc->vmsd = &vmstate_bcm2835_aux;
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    device_class_set_props(dc, bcm2835_aux_props);
}

static const TypeInfo bcm2835_aux_info = {
    .name          = TYPE_BCM2835_AUX,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2835AuxState),
    .instance_init = bcm2835_aux_init,
    .instance_finalize = bcm2835_aux_finalize,
    .class_init    = bcm2835_aux_class_init,
};

static void bcm2835_aux_register_types(void)
{
    type_register_static(&bcm2835_aux_info);
}

type_init(bcm2835_aux_register_types)
