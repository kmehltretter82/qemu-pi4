Raspberry Pi 4 Model B (``raspi4b``)
====================================

``qemu-pi4`` provides a focused model of a Raspberry Pi 4 Model B revision
1.5 with four Cortex-A72 cores and 2 GiB of RAM. The model is available only
in the AArch64 system emulator.

The Raspberry Pi 400 uses the same BCM2711 generation but is not yet exposed
as a distinct machine. Raspberry Pi 5 is not supported: its BCM2712 SoC and
RP1 I/O controller require separate models.

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
 * MailBox controller (MBOX)
 * VideoCore firmware property interface, including firmware-controlled GPIOs
 * Peripheral SPI controller (SPI)
 * Broadcom Serial Controller (I2C)

Missing devices
---------------

 * V3D 4.2 graphics accelerator (its MMIO range is an unimplemented
   placeholder)
 * BCM2711 HDMI/display pipeline and HDMI DDC I2C controllers
 * BCM2711 always-on L2 interrupt controller used by HDMI
 * Pulse Width Modulation (PWM)
 * PCIe root port
 * GENET Ethernet controller
 * RNG200 random number generator
 * BCM2711 thermal sensor

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
      -append 'earlycon=pl011,mmio32,0xfe201000 console=ttyAMA0,115200 root=/dev/mmcblk0p2 rootwait rw' \
      -nographic

With an unpartitioned filesystem image, use ``root=/dev/mmcblk0`` instead.
The SD model requires an image whose size is a valid SD card capacity; a
power-of-two size such as 4 GiB is a convenient choice.  Remove
``snapshot=on`` only when guest writes should persist.
