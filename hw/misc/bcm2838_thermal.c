/*
 * BCM2838 AVS thermal monitor
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/bcm2838_thermal.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define AVS_MMIO_SIZE                   0xf00
#define AVS_RO_TEMP_STATUS              0x200
#define AVS_RO_TEMP_STATUS_DATA_MASK    0x3ff
#define AVS_RO_TEMP_STATUS_VALID_MASK   (BIT(16) | BIT(10))

/* BCM2711 DT calibration: temperature = slope * raw + offset, in mC. */
#define BCM2711_THERMAL_SLOPE           (-487)
#define BCM2711_THERMAL_OFFSET          410040
#define BCM2711_THERMAL_DEFAULT_RAW     770
#define BCM2711_THERMAL_MIN             \
    (BCM2711_THERMAL_SLOPE * AVS_RO_TEMP_STATUS_DATA_MASK + \
     BCM2711_THERMAL_OFFSET)
#define BCM2711_THERMAL_MAX             BCM2711_THERMAL_OFFSET

static int64_t bcm2838_thermal_temperature(BCM2838ThermalState *s)
{
    return BCM2711_THERMAL_SLOPE * s->raw_temperature +
           BCM2711_THERMAL_OFFSET;
}

static void bcm2838_thermal_get_temperature(Object *obj, Visitor *v,
                                            const char *name, void *opaque,
                                            Error **errp)
{
    BCM2838ThermalState *s = BCM2838_THERMAL(obj);
    int64_t temperature = bcm2838_thermal_temperature(s);

    visit_type_int(v, name, &temperature, errp);
}

static void bcm2838_thermal_set_temperature(Object *obj, Visitor *v,
                                            const char *name, void *opaque,
                                            Error **errp)
{
    BCM2838ThermalState *s = BCM2838_THERMAL(obj);
    int64_t temperature;

    if (!visit_type_int(v, name, &temperature, errp)) {
        return;
    }
    if (temperature < BCM2711_THERMAL_MIN ||
        temperature > BCM2711_THERMAL_MAX) {
        error_setg(errp,
                   "temperature must be between %d and %d millidegrees C",
                   BCM2711_THERMAL_MIN, BCM2711_THERMAL_MAX);
        return;
    }

    s->raw_temperature = (BCM2711_THERMAL_OFFSET - temperature +
                          (-BCM2711_THERMAL_SLOPE / 2)) /
                         -BCM2711_THERMAL_SLOPE;
}

static uint64_t bcm2838_thermal_read(void *opaque, hwaddr offset,
                                     unsigned int size)
{
    BCM2838ThermalState *s = BCM2838_THERMAL(opaque);

    if (offset == AVS_RO_TEMP_STATUS) {
        return AVS_RO_TEMP_STATUS_VALID_MASK | s->raw_temperature;
    }

    qemu_log_mask(LOG_UNIMP,
                  "%s: unimplemented read from offset 0x%" HWADDR_PRIx "\n",
                  TYPE_BCM2838_THERMAL, offset);
    return 0;
}

static void bcm2838_thermal_write(void *opaque, hwaddr offset,
                                  uint64_t value, unsigned int size)
{
    if (offset == AVS_RO_TEMP_STATUS) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to read-only temperature status\n",
                      TYPE_BCM2838_THERMAL);
        return;
    }

    qemu_log_mask(LOG_UNIMP,
                  "%s: unimplemented write to offset 0x%" HWADDR_PRIx "\n",
                  TYPE_BCM2838_THERMAL, offset);
}

static const MemoryRegionOps bcm2838_thermal_ops = {
    .read = bcm2838_thermal_read,
    .write = bcm2838_thermal_write,
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

static int bcm2838_thermal_post_load(void *opaque, int version_id)
{
    BCM2838ThermalState *s = BCM2838_THERMAL(opaque);

    return s->raw_temperature <= AVS_RO_TEMP_STATUS_DATA_MASK ? 0 : -EINVAL;
}

static const VMStateDescription vmstate_bcm2838_thermal = {
    .name = TYPE_BCM2838_THERMAL,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2838_thermal_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT16(raw_temperature, BCM2838ThermalState),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2838_thermal_init(Object *obj)
{
    BCM2838ThermalState *s = BCM2838_THERMAL(obj);

    s->raw_temperature = BCM2711_THERMAL_DEFAULT_RAW;
    object_property_add(obj, "temperature", "int",
                        bcm2838_thermal_get_temperature,
                        bcm2838_thermal_set_temperature, NULL, NULL);
    object_property_set_description(obj, "temperature",
                                    "Sensor temperature in millidegrees C");
    memory_region_init_io(&s->iomem, obj, &bcm2838_thermal_ops, s,
                          TYPE_BCM2838_THERMAL, AVS_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
}

static void bcm2838_thermal_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "BCM2838 AVS thermal monitor";
    dc->vmsd = &vmstate_bcm2838_thermal;
}

static const TypeInfo bcm2838_thermal_info = {
    .name = TYPE_BCM2838_THERMAL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2838ThermalState),
    .instance_init = bcm2838_thermal_init,
    .class_init = bcm2838_thermal_class_init,
};

static void bcm2838_thermal_register_types(void)
{
    type_register_static(&bcm2838_thermal_info);
}

type_init(bcm2838_thermal_register_types)
