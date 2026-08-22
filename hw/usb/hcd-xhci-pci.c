/*
 * USB xHCI controller with PCI bus emulation
 *
 * SPDX-FileCopyrightText: 2011 Securiforest
 * SPDX-FileContributor: Hector Martin <hector@marcansoft.com>
 * SPDX-sourceInfo: Based on usb-ohci.c, emulates Renesas NEC USB 3.0
 * SPDX-FileCopyrightText: 2020 Xilinx
 * SPDX-FileContributor: Sai Pavan Boddu <sai.pavan.boddu@xilinx.com>
 * SPDX-sourceInfo: Moved the pci specific content for hcd-xhci.c to
 *                  hcd-xhci-pci.c
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
 */
#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_ids.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/pci/pcie_aer.h"
#include "hw/pci/pcie_regs.h"
#include "hcd-xhci-pci.h"
#include "trace.h"
#include "qapi/error.h"

#define OFF_MSIX_TABLE  0x3000
#define OFF_MSIX_PBA    0x3800

#define VL805_PM_OFFSET      0x80
#define VL805_MSI_OFFSET     0x90
#define VL805_PCIE_OFFSET    0xc4
#define VL805_AER_OFFSET     0x100
#define VL805_AER_SIZE       0x40

static bool xhci_pci_is_vl805(PCIDevice *dev)
{
    return object_dynamic_cast(OBJECT(dev), TYPE_VL805_XHCI) != NULL;
}

static bool xhci_pci_init_msi(PCIDevice *dev, XHCIPciState *s,
                              uint8_t offset, Error **errp)
{
    Error *err = NULL;
    int ret;

    if (s->msi == ON_OFF_AUTO_OFF) {
        return true;
    }

    ret = msi_init(dev, offset, s->xhci.numintrs, true, false, &err);
    /*
     * Any error other than -ENOTSUP (the board has broken MSI support)
     * is a programming error.
     */
    assert(!ret || ret == -ENOTSUP);
    if (ret && s->msi == ON_OFF_AUTO_ON) {
        error_append_hint(&err, "You have to use msi=auto (default) or "
                          "msi=off with this machine type.\n");
        error_propagate(errp, err);
        return false;
    }
    assert(!err || s->msi == ON_OFF_AUTO_AUTO);
    /* With msi=auto, fall back to MSI off silently. */
    error_free(err);
    return true;
}

static bool vl805_pci_init_caps(PCIDevice *dev, Error **errp)
{
    uint8_t *exp;
    uint8_t *aer;
    int ret;

    if (!pci_bus_is_express(pci_get_bus(dev))) {
        error_setg(errp, "VL805 requires a PCI Express bus");
        return false;
    }

    /* Add conventional capabilities in reverse linked-list order. */
    ret = pcie_endpoint_cap_init(dev, VL805_PCIE_OFFSET);
    if (ret < 0) {
        error_setg(errp, "failed to initialize VL805 PCIe capability");
        return false;
    }

    exp = dev->config + VL805_PCIE_OFFSET;
    pci_set_long(exp + PCI_EXP_DEVCAP, 0x00008001);
    pci_set_long(exp + PCI_EXP_LNKCAP, 0x00065c12);
    pci_set_word(exp + PCI_EXP_LNKSTA,
                 QEMU_PCI_EXP_LNKSTA_NLW(QEMU_PCI_EXP_LNK_X1) |
                 QEMU_PCI_EXP_LNKSTA_CLS(QEMU_PCI_EXP_LNK_5GT) |
                 PCI_EXP_LNKSTA_SLC);
    pci_set_long(exp + PCI_EXP_DEVCAP2, 0x00000012);
    pci_set_long(exp + PCI_EXP_LNKCAP2, 0);
    pci_set_word(exp + PCI_EXP_LNKCTL2,
                 PCI_EXP_LNKCTL2_TLS_5_0GT | PCI_EXP_LNKCTL2_HASD);
    pci_set_word(exp + PCI_EXP_LNKSTA2, 1);

    pcie_cap_deverr_init(dev);
    pci_word_test_and_set_mask(dev->wmask + VL805_PCIE_OFFSET +
                               PCI_EXP_DEVCTL,
                               PCI_EXP_DEVCTL_RELAX_EN |
                               PCI_EXP_DEVCTL_PAYLOAD |
                               PCI_EXP_DEVCTL_NOSNOOP_EN |
                               PCI_EXP_DEVCTL_READRQ);
    pci_word_test_and_set_mask(dev->wmask + VL805_PCIE_OFFSET +
                               PCI_EXP_LNKCTL,
                               PCI_EXP_LNKCTL_ASPMC |
                               PCI_EXP_LNKCTL_RCB |
                               PCI_EXP_LNKCTL_CCC |
                               PCI_EXP_LNKCTL_ES |
                               PCI_EXP_LNKCTL_CLKREQ_EN |
                               PCI_EXP_LNKCTL_HAWD |
                               PCI_EXP_LNKCTL_LBMIE |
                               PCI_EXP_LNKCTL_LABIE);
    pci_set_word(dev->wmask + VL805_PCIE_OFFSET + PCI_EXP_DEVCTL2,
                 PCI_EXP_DEVCTL2_COMP_TIMEOUT |
                 PCI_EXP_DEVCTL2_COMP_TMOUT_DIS);
    pci_set_word(dev->wmask + VL805_PCIE_OFFSET + PCI_EXP_LNKCTL2,
                 PCI_EXP_LNKCTL2_TLS);

    dev->cap_present |= QEMU_PCIE_ERR_UNC_MASK;
    ret = pcie_aer_init(dev, PCI_ERR_VER, VL805_AER_OFFSET,
                        VL805_AER_SIZE, errp);
    if (ret < 0) {
        return false;
    }
    aer = dev->config + VL805_AER_OFFSET;
    pci_set_long(aer + PCI_ERR_UNCOR_MASK, 0);
    pci_set_long(aer + PCI_ERR_UNCOR_SEVER, 0x00062031);
    pci_set_long(aer + PCI_ERR_COR_MASK, PCI_ERR_COR_ADV_NONFATAL);
    pci_set_long(aer + PCI_ERR_CAP, 0);
    pci_set_long(dev->wmask + VL805_AER_OFFSET + PCI_ERR_CAP, 0);

    return true;
}

static bool vl805_pci_init_pm(PCIDevice *dev, Error **errp)
{
    int ret = pci_pm_init(dev, VL805_PM_OFFSET, errp);

    if (ret < 0) {
        return false;
    }

    /* PM v1.2, 375 mA auxiliary current, PME from D0 and D3cold. */
    pci_set_word(dev->config + VL805_PM_OFFSET + PCI_PM_PMC, 0x89c3);
    pci_set_word(dev->wmask + VL805_PM_OFFSET + PCI_PM_CTRL,
                 PCI_PM_CTRL_STATE_MASK |
                 PCI_PM_CTRL_PME_ENABLE |
                 PCI_PM_CTRL_DATA_SEL_MASK);
    pci_set_word(dev->w1cmask + VL805_PM_OFFSET + PCI_PM_CTRL,
                 PCI_PM_CTRL_PME_STATUS);
    return true;
}

static void xhci_pci_intr_update(XHCIState *xhci, int n, bool enable)
{
    XHCIPciState *s = container_of(xhci, XHCIPciState, xhci);
    PCIDevice *pci_dev = PCI_DEVICE(s);

    if (!msix_enabled(pci_dev)) {
        return;
    }
    if (enable == !!xhci->intr[n].msix_used) {
        return;
    }
    if (enable) {
        trace_usb_xhci_irq_msix_use(n);
        msix_vector_use(pci_dev, n);
        xhci->intr[n].msix_used = true;
    } else {
        trace_usb_xhci_irq_msix_unuse(n);
        msix_vector_unuse(pci_dev, n);
        xhci->intr[n].msix_used = false;
    }
}

static bool xhci_pci_intr_raise(XHCIState *xhci, int n, bool level)
{
    XHCIPciState *s = container_of(xhci, XHCIPciState, xhci);
    PCIDevice *pci_dev = PCI_DEVICE(s);

    if (n == 0 &&
        !(msix_enabled(pci_dev) ||
         msi_enabled(pci_dev))) {
        pci_set_irq(pci_dev, level);
    }

    if (msix_enabled(pci_dev) && level) {
        msix_notify(pci_dev, n);
        return true;
    }

    if (msi_enabled(pci_dev) && level) {
        n %= msi_nr_vectors_allocated(pci_dev);
        msi_notify(pci_dev, n);
        return true;
    }

    return false;
}

static bool xhci_pci_intr_mapping_conditional(XHCIState *xhci)
{
    XHCIPciState *s = container_of(xhci, XHCIPciState, xhci);
    PCIDevice *pci_dev = PCI_DEVICE(s);

    /*
     * Implementation of the "conditional-intr-mapping" property, which only
     * enables interrupter mapping if MSI or MSI-X is available and active.
     * Forces all events onto interrupter/event ring 0 in pin-based IRQ mode.
     * Provides compatibility with macOS guests on machine types where MSI(-X)
     * is not available.
     */
    return msix_enabled(pci_dev) || msi_enabled(pci_dev);
}

static void xhci_pci_reset(DeviceState *dev)
{
    XHCIPciState *s = XHCI_PCI(dev);

    device_cold_reset(DEVICE(&s->xhci));
}

static int xhci_pci_vmstate_post_load(void *opaque, int version_id)
{
    XHCIPciState *s = XHCI_PCI(opaque);
    PCIDevice *pci_dev = PCI_DEVICE(s);
    int intr;

    if (!msix_present(pci_dev)) {
        return 0;
    }

    for (intr = 0; intr < s->xhci.numintrs; intr++) {
        if (s->xhci.intr[intr].msix_used) {
            msix_vector_use(pci_dev, intr);
        } else {
            msix_vector_unuse(pci_dev, intr);
        }
    }
   return 0;
}

static void usb_xhci_pci_realize(struct PCIDevice *dev, Error **errp)
{
    int ret;
    bool vl805 = xhci_pci_is_vl805(dev);
    XHCIPciState *s = XHCI_PCI(dev);

    dev->config[PCI_CLASS_PROG] = 0x30;    /* xHCI */
    dev->config[PCI_INTERRUPT_PIN] = 0x01; /* interrupt pin 1 */
    dev->config[PCI_CACHE_LINE_SIZE] = 0x10;
    dev->config[0x60] = 0x30; /* release number */

    object_property_set_link(OBJECT(&s->xhci), "host", OBJECT(s), NULL);
    s->xhci.intr_update = xhci_pci_intr_update;
    s->xhci.intr_raise = xhci_pci_intr_raise;
    if (s->conditional_intr_mapping) {
        s->xhci.intr_mapping_supported = xhci_pci_intr_mapping_conditional;
    }
    if (!qdev_realize(DEVICE(&s->xhci), NULL, errp)) {
        return;
    }
    if (strcmp(object_get_typename(OBJECT(dev)), TYPE_NEC_XHCI) == 0) {
        s->xhci.nec_quirks = true;
    }

    if (vl805) {
        if (!vl805_pci_init_caps(dev, errp) ||
            !xhci_pci_init_msi(dev, s, VL805_MSI_OFFSET, errp) ||
            !vl805_pci_init_pm(dev, errp)) {
            return;
        }
    } else if (!xhci_pci_init_msi(dev, s, 0x70, errp)) {
        return;
    }
    pci_register_bar(dev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64,
                     &s->xhci.mem);

    if (!vl805 && pci_bus_is_express(pci_get_bus(dev))) {
        ret = pcie_endpoint_cap_init(dev, 0xa0);
        assert(ret > 0);
    }

    if (s->msix != ON_OFF_AUTO_OFF) {
        ret = msix_init(dev, s->xhci.numintrs,
                        &s->xhci.mem, 0, OFF_MSIX_TABLE,
                        &s->xhci.mem, 0, OFF_MSIX_PBA,
                        0x90, s->msix == ON_OFF_AUTO_ON ? errp : NULL);

        if (ret < 0) {
            if (s->msix == ON_OFF_AUTO_ON) {
                return;
            }
            s->msix = ON_OFF_AUTO_OFF;
        }
    }
    s->xhci.as = pci_get_address_space(dev);
}

static void usb_xhci_pci_exit(PCIDevice *dev)
{
    XHCIPciState *s = XHCI_PCI(dev);
    /* destroy msix memory region */
    if (dev->msix_table && dev->msix_pba
        && dev->msix_entry_used) {
        msix_uninit(dev, &s->xhci.mem, &s->xhci.mem);
    }
    msi_uninit(dev);
    if (xhci_pci_is_vl805(dev)) {
        pcie_aer_exit(dev);
        pcie_cap_exit(dev);
    }
    /*
     * The embedded xhci-core child holds a strong "host" link back to this
     * PCI device (set in usb_xhci_pci_realize()), forming a refcount cycle:
     * the PCI device owns the child, and the child's strong link pins the PCI
     * device. On unplug, object_unparent() only drops the parent/bus refs, so
     * the link ref keeps this device at refcount 1 forever and
     * device_finalize() never runs. Unrealize the child first (so the
     * realized-check in set_link passes), then clear the link to break the
     * cycle.
     */
    qdev_unrealize(DEVICE(&s->xhci));
    object_property_set_link(OBJECT(&s->xhci), "host", NULL, &error_abort);
}

static const VMStateDescription vmstate_xhci_pci = {
    .name = "xhci",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = xhci_pci_vmstate_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, XHCIPciState),
        VMSTATE_MSIX(parent_obj, XHCIPciState),
        VMSTATE_STRUCT(xhci, XHCIPciState, 1, vmstate_xhci, XHCIState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_vl805_xhci = {
    .name = "vl805-xhci",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = xhci_pci_vmstate_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, XHCIPciState),
        VMSTATE_MSIX(parent_obj, XHCIPciState),
        VMSTATE_STRUCT(xhci, XHCIPciState, 1, vmstate_xhci, XHCIState),
        VMSTATE_STRUCT(parent_obj.exp.aer_log, XHCIPciState, 0,
                       vmstate_pcie_aer_log, PCIEAERLog),
        VMSTATE_END_OF_LIST()
    }
};

static void xhci_instance_init(Object *obj)
{
    XHCIPciState *s = XHCI_PCI(obj);
    /*
     * QEMU_PCI_CAP_EXPRESS initialization does not depend on QEMU command
     * line, therefore, no need to wait to realize like other devices
     */
    PCI_DEVICE(obj)->cap_present |= QEMU_PCI_CAP_EXPRESS;
    object_initialize_child(obj, "xhci-core", &s->xhci, TYPE_XHCI);
    qdev_alias_all_properties(DEVICE(&s->xhci), obj);
}

static const Property xhci_pci_properties[] = {
    DEFINE_PROP_ON_OFF_AUTO("msi", XHCIPciState, msi, ON_OFF_AUTO_AUTO),
    DEFINE_PROP_ON_OFF_AUTO("msix", XHCIPciState, msix, ON_OFF_AUTO_AUTO),
    DEFINE_PROP_BOOL("conditional-intr-mapping", XHCIPciState,
                     conditional_intr_mapping, false),
};

static void xhci_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, xhci_pci_reset);
    dc->vmsd    = &vmstate_xhci_pci;
    set_bit(DEVICE_CATEGORY_USB, dc->categories);
    k->realize      = usb_xhci_pci_realize;
    k->exit         = usb_xhci_pci_exit;
    k->class_id     = PCI_CLASS_SERIAL_USB;
    device_class_set_props(dc, xhci_pci_properties);
    object_class_property_set_description(klass, "conditional-intr-mapping",
        "When true, disables interrupter mapping for pin-based IRQ mode. "
        "Intended to be used with guest drivers with questionable behaviour, "
        "such as macOS's.");
}

static const TypeInfo xhci_pci_info = {
    .name          = TYPE_XHCI_PCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(XHCIPciState),
    .class_init    = xhci_class_init,
    .instance_init = xhci_instance_init,
    .abstract      = true,
    .interfaces = (const InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { }
    },
};

static void qemu_xhci_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id    = PCI_VENDOR_ID_REDHAT;
    k->device_id    = PCI_DEVICE_ID_REDHAT_XHCI;
    k->revision     = 0x01;
}

static void qemu_xhci_instance_init(Object *obj)
{
    XHCIPciState *s = XHCI_PCI(obj);
    XHCIState *xhci = &s->xhci;

    s->msi      = ON_OFF_AUTO_OFF;
    s->msix     = ON_OFF_AUTO_AUTO;
    xhci->numintrs = XHCI_MAXINTRS;
    xhci->numslots = XHCI_MAXSLOTS;
}

static const TypeInfo qemu_xhci_info = {
    .name          = TYPE_QEMU_XHCI,
    .parent        = TYPE_XHCI_PCI,
    .class_init    = qemu_xhci_class_init,
    .instance_init = qemu_xhci_instance_init,
};

static void vl805_xhci_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_VIA;
    k->device_id = PCI_DEVICE_ID_VIA_VL805;
    k->revision = 0x01;
    k->subsystem_vendor_id = PCI_VENDOR_ID_VIA;
    k->subsystem_id = PCI_DEVICE_ID_VIA_VL805;
    dc->desc = "VIA Labs VL805 xHCI USB 3.0 controller";
    dc->vmsd = &vmstate_vl805_xhci;
}

static void vl805_xhci_instance_init(Object *obj)
{
    XHCIPciState *s = XHCI_PCI(obj);
    XHCIState *xhci = &s->xhci;

    s->msi = ON_OFF_AUTO_ON;
    s->msix = ON_OFF_AUTO_OFF;
    xhci->numintrs = 4;
    xhci->numslots = 32;
    xhci->numports_2 = 1;
    xhci->numports_3 = 4;
    xhci->register_model = XHCI_REGISTER_MODEL_VL805;
    xhci->bus_name = "vl805.0";
}

static const TypeInfo vl805_xhci_info = {
    .name = TYPE_VL805_XHCI,
    .parent = TYPE_XHCI_PCI,
    .class_init = vl805_xhci_class_init,
    .instance_init = vl805_xhci_instance_init,
};

static void xhci_register_types(void)
{
    type_register_static(&xhci_pci_info);
    type_register_static(&qemu_xhci_info);
    type_register_static(&vl805_xhci_info);
}

type_init(xhci_register_types)
