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
 * SD/MMC host controller
 * USB2 host controller (DWC2 and MPHI)
 * MailBox controller (MBOX)
 * VideoCore firmware (property)
 * Peripheral SPI controller (SPI)
 * Broadcom Serial Controller (I2C)

Missing devices
---------------

 * Pulse Width Modulation (PWM)
 * PCIe root port
 * GENET Ethernet controller
 * RNG200 random number generator
 * BCM2711 thermal sensor
