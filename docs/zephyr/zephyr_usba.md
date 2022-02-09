# Zephyr USBA Configuration

[TOC]

## Overview

[USBA] is used to configure the number of USB Type-A ports in the system and
optional control of the power supplied by said ports.

## Kconfig Options

CONFIG_PLATFORM_EC_USBA enables support for USB-A ports in the EC application.
Refer to [Kconfig.usba] for all sub-options controlling USB-A ports.

## Devicetree Nodes

By default, for each USB Type-A port, a GPIO pin is required to control when power
is supplied to the port. The GPIO pins are described in Device Tree nodes.

Refer to the [named-gpios.yaml] and [cros-ec,usba-port-enable-pins.yaml] child-binding
files for details about gpio properties.

## Board Specific Code

none

## Threads

When CONFIG_PLATFORM_EC_USB_PORT_POWER_DUMB=y, then the EC application automatically
powers up USB-A ports when the AP chipset starts upand powers off the USB-A ports
when the AP chipset shuts down.

## Testing and Debugging

The `gpioset` console command could be used to enable and disable the USB Type-A
port power.

`gpioset` usage: gpioset <pin_name> <0 | 1>

The `usbchargemode` console command could be used to enable and disable charging
from the USB Type-A port.

`usbchargemode` <port> <0 | 1 | 2 | 3> <0 | 1>

## Example

The Herobrine board has one USB Type-A port:

The following configures the project for one port.

```
CONFIG_PLATFORM_EC_USB_PORT_POWER_DUMB=y
CONFIG_PLATFORM_EC_USB_A_PORT_COUNT=1
```

The following device tree node configures the gpio pin.

```
en_usb_a_5v {
	gpios = <&gpiof 0 GPIO_OUT_LOW>;
	enum-name = "GPIO_EN_USB_A_5V";
};
```

The system defines USB_PORT_COUNT as CONFIG_PLATFORM_EC_USB_A_PORT_COUNT

[USBA]: ../ec_terms.md#usba
[Kconfig.usba]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usba
[named-gpios.yaml]: ../../zephyr/dts/bindings/gpio/named-gpios.yaml
[cros-ec,usba-port-enable-pins.yaml]: ../../zephyr/dts/bindings/gpio/cros-ec,usba-port-enable-pins.yaml
