/*
 * Raspberry Pi 4 family emulation
 *
 * Copyright (C) 2022 Ovchinnikov Vitalii <vitalii.ovchinnikov@auriga.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "hw/arm/machines-qom.h"
#include "hw/arm/raspi4_platform.h"
#include "hw/display/bcm2835_fb.h"
#include "hw/core/registerfields.h"
#include "hw/core/qdev-properties.h"
#include "qemu/error-report.h"
#include "system/device_tree.h"
#include "hw/core/boards.h"
#include "hw/core/loader.h"
#include "hw/arm/boot.h"
#include "qom/object.h"
#include "hw/arm/bcm2838.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_bridge.h"
#include "hw/usb/hid.h"
#include "hw/usb/usb.h"
#include "hw/usb/xhci.h"
#include <libfdt.h>

#define TYPE_RASPI4B_MACHINE MACHINE_TYPE_NAME("raspi4b")

#if HOST_LONG_BITS == 64
#define TYPE_RASPI400_MACHINE MACHINE_TYPE_NAME("raspi400")
#define RASPI400_BOARD_REVISION 0xc03130
#endif

/*
 * Add ARM-visible RAM above the VideoCore window without describing the
 * BCM2711 low-peripheral alias as memory (BCM2711 datasheet, section 1.2).
 */
static void raspi_add_memory_node(void *fdt, hwaddr mem_base, hwaddr mem_len)
{
    uint32_t acells, scells;
    char *nodename = g_strdup_printf("/memory@%" PRIx64, mem_base);

    acells = qemu_fdt_getprop_cell(fdt, "/", "#address-cells",
                                   NULL, &error_fatal);
    scells = qemu_fdt_getprop_cell(fdt, "/", "#size-cells",
                                   NULL, &error_fatal);
    /* validated by arm_load_dtb */
    g_assert(acells && scells);

    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_string(fdt, nodename, "device_type", "memory");
    qemu_fdt_setprop_sized_cells(fdt, nodename, "reg",
                                        acells, mem_base,
                                        scells, mem_len);

    g_free(nodename);
}

static void raspi4_modify_dtb(const struct arm_boot_info *info, void *fdt)
{
    Raspi4BaseMachineState *s_base =
        container_of(info, Raspi4BaseMachineState, binfo);
    char **node_paths;
    uint64_t ram_size;
    uint64_t upper_end;

    /* Temporarily disable following devices until they are implemented. */
    const char *nodes_to_remove[] = {
        "brcm,bcm2711-pixelvalve0",
        "brcm,bcm2711-pixelvalve1",
        "brcm,bcm2711-pixelvalve3",
        "brcm,bcm2711-pixelvalve4",
        "brcm,bcm2711-hdmi1",
        "brcm,2711-v3d",
    };

    for (int i = 0; i < ARRAY_SIZE(nodes_to_remove); i++) {
        const char *dev_str = nodes_to_remove[i];
        int offset;

        while ((offset = fdt_node_offset_by_compatible(fdt, -1,
                                                       dev_str)) >= 0) {
            if (fdt_nop_node(fdt, offset) != 0) {
                break;
            }
            warn_report("bcm2711 dtb: %s has been disabled!", dev_str);
        }
    }

    node_paths = qemu_fdt_node_path(fdt, NULL, "brcm,bcm2711-genet-v5",
                                    &error_fatal);
    for (int i = 0; node_paths && node_paths[i]; i++) {
        qemu_fdt_setprop(fdt, node_paths[i], "local-mac-address",
                         s_base->soc.peripherals.genet.conf.macaddr.a, 6);
    }
    g_strfreev(node_paths);

    ram_size = raspi4_board_ram_size(info->board_id);
    upper_end = MIN(ram_size, (uint64_t)BCM2838_PERI_LOW_BASE);

    if (upper_end > UPPER_RAM_BASE) {
        raspi_add_memory_node(fdt, UPPER_RAM_BASE,
                              upper_end - UPPER_RAM_BASE);
    }
}

static void raspi4_machine_init(MachineState *machine)
{
    Raspi4BaseMachineState *s_base = RASPI4_BASE_MACHINE(machine);
    Raspi4BaseMachineClass *mc = RASPI4_BASE_MACHINE_GET_CLASS(machine);
    BCM2838State *soc = &s_base->soc;
    PCIBus *pcie_bus;
    PCIDevice *vl805;
    USBBus *usb_bus;
    USBDevice *hub;

    s_base->binfo.modify_dtb = raspi4_modify_dtb;
    s_base->binfo.board_id = mc->board_rev;

    object_initialize_child(OBJECT(machine), "soc", soc, TYPE_BCM2838);

    object_property_set_bool(
        OBJECT(&soc->peripherals.parent_obj.property), "has-vl805", true,
        &error_abort);

    raspi4_common_machine_init(machine, soc);

    /* Pi 4 Model B and Pi 400 have a fixed VL805 at downstream BDF 00.0. */
    pcie_bus = pci_bridge_get_sec_bus(
        PCI_BRIDGE(&soc->peripherals.pcie.root_port));
    vl805 = pci_create_simple(pcie_bus, PCI_DEVFN(0, 0), TYPE_VL805_XHCI);
    qdev_connect_gpio_out_named(
        DEVICE(&soc->peripherals.parent_obj.property),
        BCM2835_PROPERTY_XHCI_NOTIFY, 0,
        qdev_get_gpio_in_named(DEVICE(vl805), VL805_XHCI_FIRMWARE_NOTIFY, 0));

    usb_bus = USB_BUS(qdev_get_child_bus(DEVICE(vl805), "vl805.0"));
    hub = USB_DEVICE(qdev_new(TYPE_USB_VIA_3431_HUB));
    qdev_prop_set_uint32(DEVICE(hub), "ports", 4);
    qdev_prop_set_string(DEVICE(hub), "port", "1");
    usb_realize_and_unref(hub, usb_bus, &error_abort);

#if HOST_LONG_BITS == 64
    if (mc->board_rev == RASPI400_BOARD_REVISION) {
        USBDevice *keyboard = USB_DEVICE(qdev_new(TYPE_USB_PI400_KEYBOARD));

        qdev_prop_set_string(DEVICE(keyboard), "port", "1.4");
        usb_realize_and_unref(keyboard, usb_bus, &error_abort);
    }
#endif
}

static void raspi4b_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    Raspi4BaseMachineClass *rmc = RASPI4_BASE_MACHINE_CLASS(oc);

#if HOST_LONG_BITS == 32
    rmc->board_rev = 0xa03111; /* Revision 1.1, 1 Gb RAM */
#else
    rmc->board_rev = 0xb03115; /* Revision 1.5, 2 Gb RAM */
#endif
    raspi4_common_machine_class_init(mc, rmc->board_rev, "4B");
    mc->auto_create_sdcard = true;
    mc->init = raspi4_machine_init;
}

#if HOST_LONG_BITS == 64
static void raspi400_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    Raspi4BaseMachineClass *rmc = RASPI4_BASE_MACHINE_CLASS(oc);

    rmc->board_rev = RASPI400_BOARD_REVISION; /* Revision 1.0, 4 GiB RAM */
    raspi4_common_machine_class_init(mc, rmc->board_rev, "400");
    mc->auto_create_sdcard = true;
    mc->init = raspi4_machine_init;
}
#endif

static const TypeInfo raspi4b_machine_type = {
    .name           = TYPE_RASPI4B_MACHINE,
    .parent         = TYPE_RASPI4_BASE_MACHINE,
    .class_init     = raspi4b_machine_class_init,
    .interfaces     = aarch64_machine_interfaces,
};

#if HOST_LONG_BITS == 64
static const TypeInfo raspi400_machine_type = {
    .name           = TYPE_RASPI400_MACHINE,
    .parent         = TYPE_RASPI4_BASE_MACHINE,
    .class_init     = raspi400_machine_class_init,
    .interfaces     = aarch64_machine_interfaces,
};
#endif

static void raspi4b_machine_register_type(void)
{
    type_register_static(&raspi4b_machine_type);
#if HOST_LONG_BITS == 64
    type_register_static(&raspi400_machine_type);
#endif
}

type_init(raspi4b_machine_register_type)
