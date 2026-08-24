/*
 * BCM2711 HDMI DDC I2C controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/i2c/bcm2711_hdmi_i2c.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define BSC_MMIO_SIZE                  0x100
#define AUTO_I2C_MMIO_SIZE             0x300

#define BSC_CHIP_ADDRESS               0x00
#define BSC_DATA_IN_FIRST              0x04
#define BSC_COUNT                      0x24
#define BSC_CONTROL                    0x28
#define BSC_IIC_ENABLE                 0x2c
#define BSC_DATA_OUT_FIRST             0x30
#define BSC_CONTROL_HIGH               0x50
#define BSC_SCL_PARAM                  0x54

#define BSC_CONTROL_DTF_MASK           0x00000003
#define BSC_CONTROL_DTF_WRITE          0x00000000
#define BSC_CONTROL_DTF_READ           0x00000001
#define BSC_CONTROL_SCL_MASK           0x00000030
#define BSC_CONTROL_INT_ENABLE         0x00000040
#define BSC_CONTROL_DIV_CLOCK          0x00000080
#define BSC_CONTROL_MASK               (BSC_CONTROL_DTF_MASK | \
                                        BSC_CONTROL_SCL_MASK | \
                                        BSC_CONTROL_INT_ENABLE | \
                                        BSC_CONTROL_DIV_CLOCK)

#define BSC_IIC_ENABLE_TRANSFER        0x00000001
#define BSC_IIC_ENABLE_INTERRUPT       0x00000002
#define BSC_IIC_ENABLE_NO_ACK          0x00000004
#define BSC_IIC_ENABLE_NO_STOP         0x00000010
#define BSC_IIC_ENABLE_NO_START        0x00000020
#define BSC_IIC_ENABLE_RESTART         0x00000040
#define BSC_IIC_ENABLE_CONDITION_MASK  (BSC_IIC_ENABLE_NO_STOP | \
                                        BSC_IIC_ENABLE_NO_START | \
                                        BSC_IIC_ENABLE_RESTART)

#define BSC_CONTROL_HIGH_WAIT_DISABLE  0x00000001
#define BSC_CONTROL_HIGH_IGNORE_ACK    0x00000002
#define BSC_CONTROL_HIGH_DATAREG_SIZE  0x00000040
#define BSC_CONTROL_HIGH_INPUT_LEVEL   0x00000080
#define BSC_CONTROL_HIGH_MASK          (BSC_CONTROL_HIGH_WAIT_DISABLE | \
                                        BSC_CONTROL_HIGH_IGNORE_ACK | \
                                        BSC_CONTROL_HIGH_DATAREG_SIZE | \
                                        BSC_CONTROL_HIGH_INPUT_LEVEL)

#define AUTO_I2C_CONTROL0              0x26c
#define AUTO_I2C_RELEASE_BSC           BIT(1)

static void bcm2711_hdmi_i2c_complete(BCM2711HDMII2CState *s, bool no_ack)
{
    s->iic_enable &= BSC_IIC_ENABLE_CONDITION_MASK;
    s->iic_enable |= BSC_IIC_ENABLE_INTERRUPT;
    if (no_ack && !(s->control_high & BSC_CONTROL_HIGH_IGNORE_ACK)) {
        s->iic_enable |= BSC_IIC_ENABLE_NO_ACK;
    }
}

static void bcm2711_hdmi_i2c_transfer(BCM2711HDMII2CState *s)
{
    unsigned int length = s->count & 0x3f;
    unsigned int direction = s->control & BSC_CONTROL_DTF_MASK;
    uint8_t address = (s->chip_address >> 1) & 0x7f;
    bool no_start = s->iic_enable & BSC_IIC_ENABLE_NO_START;
    bool no_stop = s->iic_enable & BSC_IIC_ENABLE_NO_STOP;
    bool no_ack = false;
    bool receive;

    if (!s->released) {
        no_ack = true;
        goto complete;
    }
    if (length > sizeof(s->data_in)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: transfer length %u exceeds the data registers\n",
                      TYPE_BCM2711_HDMI_I2C, length);
        no_ack = true;
        goto complete;
    }

    switch (direction) {
    case BSC_CONTROL_DTF_WRITE:
        receive = false;
        break;
    case BSC_CONTROL_DTF_READ:
        receive = true;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: combined transfer format %u is not implemented\n",
                      TYPE_BCM2711_HDMI_I2C, direction);
        no_ack = true;
        goto complete;
    }

    if (no_start) {
        no_ack = !i2c_bus_busy(s->bus);
    } else {
        no_ack = i2c_start_transfer(s->bus, address, receive);
    }

    if (!no_ack) {
        for (unsigned int byte = 0; byte < length; byte++) {
            unsigned int reg = byte / sizeof(uint32_t);
            unsigned int shift = (byte % sizeof(uint32_t)) * 8;

            if (receive) {
                uint8_t value = i2c_recv(s->bus);

                s->data_out[reg] &= ~(0xffU << shift);
                s->data_out[reg] |= (uint32_t)value << shift;
            } else if (i2c_send(s->bus,
                                extract32(s->data_in[reg], shift, 8))) {
                no_ack = true;
                break;
            }
        }
    }

complete:
    if (!no_stop && i2c_bus_busy(s->bus)) {
        i2c_end_transfer(s->bus);
    }
    bcm2711_hdmi_i2c_complete(s, no_ack);
}

static uint64_t bcm2711_hdmi_i2c_bsc_read(void *opaque, hwaddr offset,
                                          unsigned int size)
{
    BCM2711HDMII2CState *s = opaque;

    if (offset >= BSC_DATA_IN_FIRST && offset < BSC_COUNT) {
        return s->data_in[(offset - BSC_DATA_IN_FIRST) / sizeof(uint32_t)];
    }
    if (offset >= BSC_DATA_OUT_FIRST && offset < BSC_CONTROL_HIGH) {
        return s->data_out[(offset - BSC_DATA_OUT_FIRST) / sizeof(uint32_t)];
    }

    switch (offset) {
    case BSC_CHIP_ADDRESS:
        return s->chip_address;
    case BSC_COUNT:
        return s->count;
    case BSC_CONTROL:
        return s->control;
    case BSC_IIC_ENABLE:
        return s->iic_enable;
    case BSC_CONTROL_HIGH:
        return s->control_high;
    case BSC_SCL_PARAM:
        return s->scl_param;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid BSC offset 0x%" HWADDR_PRIx
                      "\n", TYPE_BCM2711_HDMI_I2C, offset);
        return 0;
    }
}

static void bcm2711_hdmi_i2c_bsc_write(void *opaque, hwaddr offset,
                                       uint64_t value, unsigned int size)
{
    BCM2711HDMII2CState *s = opaque;

    if (offset >= BSC_DATA_IN_FIRST && offset < BSC_COUNT) {
        s->data_in[(offset - BSC_DATA_IN_FIRST) / sizeof(uint32_t)] = value;
        return;
    }
    if (offset >= BSC_DATA_OUT_FIRST && offset < BSC_CONTROL_HIGH) {
        return;
    }

    switch (offset) {
    case BSC_CHIP_ADDRESS:
        s->chip_address = value & 0xff;
        break;
    case BSC_COUNT:
        s->count = value & 0x3f;
        break;
    case BSC_CONTROL:
        s->control = value & BSC_CONTROL_MASK;
        break;
    case BSC_IIC_ENABLE:
        s->iic_enable = value & (BSC_IIC_ENABLE_CONDITION_MASK |
                                 BSC_IIC_ENABLE_TRANSFER);
        if (s->iic_enable & BSC_IIC_ENABLE_TRANSFER) {
            bcm2711_hdmi_i2c_transfer(s);
        }
        break;
    case BSC_CONTROL_HIGH:
        s->control_high = value & BSC_CONTROL_HIGH_MASK;
        break;
    case BSC_SCL_PARAM:
        s->scl_param = value;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid BSC offset 0x%" HWADDR_PRIx
                      "\n", TYPE_BCM2711_HDMI_I2C, offset);
        break;
    }
}

static uint64_t bcm2711_hdmi_i2c_auto_read(void *opaque, hwaddr offset,
                                           unsigned int size)
{
    if (offset == AUTO_I2C_CONTROL0) {
        return 0;
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: read from invalid auto-I2C offset 0x%" HWADDR_PRIx
                  "\n", TYPE_BCM2711_HDMI_I2C, offset);
    return 0;
}

static void bcm2711_hdmi_i2c_auto_write(void *opaque, hwaddr offset,
                                        uint64_t value, unsigned int size)
{
    BCM2711HDMII2CState *s = opaque;

    if (offset == AUTO_I2C_CONTROL0) {
        if (value & AUTO_I2C_RELEASE_BSC) {
            s->released = true;
        }
        return;
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: write to invalid auto-I2C offset 0x%" HWADDR_PRIx
                  "\n", TYPE_BCM2711_HDMI_I2C, offset);
}

static const MemoryRegionOps bcm2711_hdmi_i2c_bsc_ops = {
    .read = bcm2711_hdmi_i2c_bsc_read,
    .write = bcm2711_hdmi_i2c_bsc_write,
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

static const MemoryRegionOps bcm2711_hdmi_i2c_auto_ops = {
    .read = bcm2711_hdmi_i2c_auto_read,
    .write = bcm2711_hdmi_i2c_auto_write,
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

static void bcm2711_hdmi_i2c_reset(DeviceState *dev)
{
    BCM2711HDMII2CState *s = BCM2711_HDMI_I2C(dev);

    if (s->bus && i2c_bus_busy(s->bus)) {
        i2c_end_transfer(s->bus);
    }
    s->chip_address = 0;
    memset(s->data_in, 0, sizeof(s->data_in));
    s->count = 0;
    s->control = 0;
    s->iic_enable = 0;
    memset(s->data_out, 0, sizeof(s->data_out));
    s->control_high = 0;
    s->scl_param = 0;
    s->released = false;
}

static int bcm2711_hdmi_i2c_post_load(void *opaque, int version_id)
{
    BCM2711HDMII2CState *s = BCM2711_HDMI_I2C(opaque);

    if ((s->chip_address & ~0xffU) || (s->count & ~0x3fU) ||
        (s->control & ~BSC_CONTROL_MASK) ||
        (s->iic_enable & ~(BSC_IIC_ENABLE_CONDITION_MASK |
                           BSC_IIC_ENABLE_INTERRUPT |
                           BSC_IIC_ENABLE_NO_ACK)) ||
        (s->control_high & ~BSC_CONTROL_HIGH_MASK)) {
        return -EINVAL;
    }

    return 0;
}

static const VMStateDescription vmstate_bcm2711_hdmi_i2c = {
    .name = TYPE_BCM2711_HDMI_I2C,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2711_hdmi_i2c_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(chip_address, BCM2711HDMII2CState),
        VMSTATE_UINT32_ARRAY(data_in, BCM2711HDMII2CState,
                             BCM2711_HDMI_I2C_DATA_REGS),
        VMSTATE_UINT32(count, BCM2711HDMII2CState),
        VMSTATE_UINT32(control, BCM2711HDMII2CState),
        VMSTATE_UINT32(iic_enable, BCM2711HDMII2CState),
        VMSTATE_UINT32_ARRAY(data_out, BCM2711HDMII2CState,
                             BCM2711_HDMI_I2C_DATA_REGS),
        VMSTATE_UINT32(control_high, BCM2711HDMII2CState),
        VMSTATE_UINT32(scl_param, BCM2711HDMII2CState),
        VMSTATE_BOOL(released, BCM2711HDMII2CState),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2711_hdmi_i2c_realize(DeviceState *dev, Error **errp)
{
    BCM2711HDMII2CState *s = BCM2711_HDMI_I2C(dev);

    s->bus = i2c_init_bus(dev, "i2c");
}

static void bcm2711_hdmi_i2c_init(Object *obj)
{
    BCM2711HDMII2CState *s = BCM2711_HDMI_I2C(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->bsc_iomem, obj, &bcm2711_hdmi_i2c_bsc_ops, s,
                          TYPE_BCM2711_HDMI_I2C ".bsc", BSC_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->bsc_iomem);
    memory_region_init_io(&s->auto_i2c_iomem, obj,
                          &bcm2711_hdmi_i2c_auto_ops, s,
                          TYPE_BCM2711_HDMI_I2C ".auto-i2c",
                          AUTO_I2C_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->auto_i2c_iomem);
}

static void bcm2711_hdmi_i2c_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = bcm2711_hdmi_i2c_realize;
    device_class_set_legacy_reset(dc, bcm2711_hdmi_i2c_reset);
    dc->vmsd = &vmstate_bcm2711_hdmi_i2c;
}

static const TypeInfo bcm2711_hdmi_i2c_info = {
    .name = TYPE_BCM2711_HDMI_I2C,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2711HDMII2CState),
    .instance_init = bcm2711_hdmi_i2c_init,
    .class_init = bcm2711_hdmi_i2c_class_init,
};

static void bcm2711_hdmi_i2c_register_types(void)
{
    type_register_static(&bcm2711_hdmi_i2c_info);
}

type_init(bcm2711_hdmi_i2c_register_types)
