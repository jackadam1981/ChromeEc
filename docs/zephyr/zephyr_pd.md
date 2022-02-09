# Zephyr EC PD Configuration

[TOC]

## Overview

Configure [USB-C] [PD] features that enables a port to provide power
greater than the basic 5V @ 900mA. Power can be negotiated up to
20V @ 5A.

## Kconfig Options

The `CONFIG_PLATFORM_EC_USB_POWER_DELIVERY` option enables USB-C power delivery
support on the Chromebook. See [Kconfig.pd] for sub-options related to this feature.

### Shared PD Interrupts

Enable the `CONFIG_PLATFORM_EC_USB_PD_INT_SHARED` if multiple USB-C ports share a single
interrupt signal on the EC. See [Kconfig.pd_int_shared] for details about all sub-options.

A platform that shares an interrupt signal between USB-C ports 0 and 2 includes these
configuration settings:

```
CONFIG_PLATFORM_EC_USB_PD_INT_SHARED=y
CONFIG_PLATFORM_EC_USB_PD_PORT_0_SHARED=y
CONFIG_PLATFORM_EC_USB_PD_PORT_2_SHARED=y
```
### Measuring VBUS Voltage

See [Kconfig.pd_meas_vbus] for options that enable several ways to measure [VBUS]

### Detecting VBUS Voltage

See [Kconfig.pd_vbus_detection] for options that enable several ways to detect [VBUS]

### VBUS Discharge

See [Kconfig.pd_discharge] for options that enable several ways to discharge [VBUS]

### Fast Role Swap

The `CONFIG_PLATFORM_EC_USB_PD_FRS` option enables USB-C power delivery [FRS] support.
See [Kconfig.pd_frs] for details about all sub-options.

### Console Commands

The `CONFIG_PLATFORM_EC_USB_PD_CONSOLE_CMD` option enables various USB-C PD related
console commands. See [Kconfig.pd_console_cmd] for details about all sub-options.

### USBC Device Type

See [Kconfig.pd_usbc_device_type] for options on the available USB-C device types.

## Devicetree

Devicetree nodes that have their compatible property set to `named-usbc-port` are used to
represent [USB-C] [PD] ports. For example, the follow two nodes represent the two [PD]
ports on Herobrine, aptly named port0 and port1.

```
port0@0 {
	compatible = "named-usbc-port";
	reg = <0>;
	bc12 {
		compatible = "pericom,pi3usb9201";
		status = "okay";
		irq = <&int_usb_c0_bc12>;
		port = <&i2c_power>;
		i2c-addr-flags = "PI3USB9201_I2C_ADDR_3_FLAGS";
	};
	ppc {
		compatible = "ti,sn5s330";
		status = "okay";
		port = <&i2c_tcpc0>;
		i2c-addr-flags = "SN5S330_ADDR0_FLAGS";
	};
	tcpc {
		compatible = "parade,ps8xxx";
		status = "okay";
		port = <&i2c_tcpc0>;
		i2c-addr-flags = "PS8XXX_I2C_ADDR1_FLAGS";
	};
};

port1@1 {
	compatible = "named-usbc-port";
	reg = <1>;
	bc12 {
		compatible = "pericom,pi3usb9201";
		status = "okay";
		irq = <&int_usb_c1_bc12>;
		port = <&i2c_eeprom>;
		i2c-addr-flags = "PI3USB9201_I2C_ADDR_3_FLAGS";
	};
	ppc {
		compatible = "ti,sn5s330";
		status = "okay";
		port = <&i2c_tcpc1>;
		i2c-addr-flags = "SN5S330_ADDR0_FLAGS";
	};
	tcpc {
		compatible = "parade,ps8xxx";
		status = "okay";
		port = <&i2c_tcpc1>;
		i2c-addr-flags = "PS8XXX_I2C_ADDR1_FLAGS";
	};
};
```

Each port gets a `reg` property that is used to represent the port in the system. For example,
`reg = <0>` for port0 and `reg = <1>` for port1. Also note that the `reg` value matches the
value after the @ in the node name.

The primary use of the port node is to describe what devices are connected to each port. The two
ports shown above both have the same three I2C device but this does not necessarily need to be
the case. Please see [BC12], [PPC], and [TCPC] for documentation on these devices.


[USB-C]:../ec_terms.md#usb-c
[PD]:../ec_terms.md#pd
[VBUS]:../ec_terms.md#vbus
[FRS]:../ec_terms.md#frs
[BC12]./zephyr_usb_charger.md
[PPC]./zephyr_ppc.md
[TCPC]./zephyr_tcpc.md
[Kconfig.pd]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd
[Kconfig.pd_int_shared]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared
[Kconfig.pd_meas_vbus]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus
[Kconfig.pd_frs]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_frs
[Kconfig.pd_discharge]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_discharge
[Kconfig.pd_vbus_detection]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_vbus_detection
[Kconfig.pd_console_cmd]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_console_cmd
[Kconfig.pd_usbc_device_type]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_usbc_device_type
