Raspberry Pi 4 family
=====================

``qemu-pi4`` provides two BCM2711 board models in the AArch64 system emulator:

``raspi4b``
  Raspberry Pi 4 Model B revision 1.5 (board revision ``0xb03115``), with
  2 GiB of RAM.

``raspi400``
  Raspberry Pi 400 revision 1.0 (board revision ``0xc03130``), with 4 GiB of
  RAM. This model requires a 64-bit host.

The Pi 400's ARM-visible memory has a VideoCore carveout below 1 GiB and the
BCM2711 peripheral window at ``0xfc000000``. With the default 64 MiB VideoCore
allocation, Linux is therefore presented with 3968 MiB in two ranges rather
than a single contiguous 4 GiB range. This matches the address layout captured
from real hardware; there is no invented RAM relocation above 4 GiB.

Raspberry Pi 5 is not supported: its BCM2712 SoC and RP1 I/O controller
require separate models.

Potential correctness fixes and enhancements suitable for later submission
to QEMU are kept in the evidence-based :doc:`raspi-upstream` tracker.
The implementation evidence and remaining fidelity work for the Pi 4-family
PCIe and external USB path are recorded in :doc:`raspi-pcie`.
The separate :doc:`raspi-gicv2-lab` project will exercise the GICv2
virtualization interface across this fork, Linux KVM, and real Pi 4-family
hardware.  Its first real Pi 400 boot is intentionally deferred until its
minimal EL2 and EL1 paths pass the QEMU safety gate.

Implemented devices
-------------------

 * Four Cortex-A72 CPU cores
 * GIC-400 and legacy VideoCore interrupt controllers
 * DMA controller
 * Clock and reset controller (CPRMAN)
 * System Timer
 * GPIO controller
 * Serial ports (BCM2835 AUX - 16550 based - and PL011)
 * Frame Buffer
 * Arasan eMMC2 SD/MMC host controller and external SD card
 * USB2 host controller (DWC2 and MPHI)
 * Broadcom GENET v5 Gigabit Ethernet controller with an external
   BCM54213PE-compatible PHY
 * MailBox controller (MBOX)
 * VideoCore firmware property interface, including firmware-controlled GPIOs
   and the Pi 4-family VL805 initialization notification
 * Peripheral SPI controller (SPI)
 * Broadcom Serial Controller (I2C)
 * BCM2711 RNG200 random number generator, including its 16-word FIFO,
   four generation rates, status and interrupt registers, soft resets, and
   migratable refill timer and FIFO contents
 * BCM2711 AVS thermal monitor, using the device-tree calibration and a
   migratable, configurable temperature reading
 * BCM2711 PCIe host and root port, including dynamic outbound and inbound
   DMA windows, INTx, and MSI
 * Pi 4-family VIA VL805 PCIe xHCI personality, including the captured PCI and
   xHCI register layout, multi-segment event rings, DMA-backed controller
   events, MSI, PERST, migration state, and a guest-visible PCIe device-tree
   node
 * VIA ``2109:3431`` four-port high-speed USB hub on both boards
 * Raspberry Pi 400 ``04d9:0007`` low-speed integrated keyboard, including
   its keyboard and consumer-control HID interfaces

Missing devices
---------------

 * V3D 4.2 graphics accelerator (its MMIO range is an unimplemented
   placeholder)
 * BCM2711 HDMI/display pipeline and HDMI DDC I2C controllers
 * BCM2711 always-on L2 interrupt controller used by HDMI
 * Pulse Width Modulation (PWM)
 * Remaining BCM2711 PCIe controller-event behavior
 * Consumer-control key-event production for the Pi 400 keyboard's second HID
   interface; its identity, descriptors, enumeration and migration already
   work

Booting Linux from an SD image
------------------------------

The external SD card is connected to the Pi 4 eMMC2 controller.  Attach a raw
card image with ``if=sd``.  QEMU does not emulate the Raspberry Pi boot
firmware, so the kernel and device tree must still be supplied explicitly.
For example::

  qemu-system-aarch64 \
      -machine raspi4b \
      -kernel Image \
      -dtb bcm2711-rpi-4-b.dtb \
      -drive file=raspios.img,if=sd,format=raw,snapshot=on \
      -nic user,model=genet \
      -append 'earlycon=pl011,mmio32,0xfe201000 console=ttyAMA0,115200 root=/dev/mmcblk0p2 rootwait rw' \
      -nographic

With an unpartitioned filesystem image, use ``root=/dev/mmcblk0`` instead.
The SD model requires an image whose size is a valid SD card capacity; a
power-of-two size such as 4 GiB is a convenient choice.  Remove
``snapshot=on`` only when guest writes should persist.

For a Pi 400 guest, change the machine and device tree together::

  -machine raspi400
  -dtb bcm2711-rpi-400.dtb

Supplying a Pi 4 Model B DTB to ``raspi400`` (or the reverse) gives the guest
the wrong board identity and peripherals even though both boards use BCM2711.

Attaching USB storage
---------------------

The focused build includes QEMU's standard USB mass-storage device so that
external-stick workloads can exercise the complete BCM2711 PCIe, VL805 and
VIA-hub path.  For example, attach a raw image to hub port one with safe
snapshot writes::

  -drive file=stick.raw,if=none,id=stick,format=raw,snapshot=on \
  -device usb-storage,drive=stick,bus=vl805.0,port=1.1

Ports ``1.1`` through ``1.3`` are free on both machines.  Port ``1.4`` is also
free on ``raspi4b`` but contains the integrated keyboard on ``raspi400``.
Remove ``snapshot=on`` only when writes to the host image should persist.

Ethernet
--------

GENET is both machines' on-board network device. Connect it to
any normal QEMU network backend; for example, unprivileged user-mode
networking is selected with::

  -nic user,model=genet

The model implements the BCM2711 GENET v5 register layout, the external MDIO
PHY, link state, descriptor DMA, interrupts, scatter-gather transmission and
checksum offload.  Upstream Linux can acquire a DHCP lease through it.  MIB
counters, wake-on-LAN and hardware receive filtering are not yet modeled.

Random number and thermal sensors
---------------------------------

The upstream Linux ``iproc-rng200`` driver can select the on-SoC RNG and read
``/dev/hwrng``.  RNG data is generated through QEMU's guest-randomness API;
the device's already-produced FIFO data and next refill deadline migrate with
the VM.

The AVS monitor reports 35050 millidegrees Celsius by default, corresponding
to raw code 770 and the BCM2711 device-tree calibration.  Its QOM
``temperature`` property is exposed at
``/machine/soc/peripherals/thermal`` in millidegrees Celsius.  Setting it
through QMP updates the raw ten-bit reading with the sensor's 487-millidegree
quantization.  The upstream ``bcm2711_thermal`` driver exposes the result as
the ``cpu-thermal`` thermal zone.
