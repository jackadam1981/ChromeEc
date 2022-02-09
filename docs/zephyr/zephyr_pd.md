# Zephyr EC PD Configuration

[TOC]

## Overview

Configure [USB-C] [PD] features that enables a port to provide power
greater than the basic 5V @ 900mA. Power can be negotiated up to
20V @ 5A.

## Kconfig Options

The `CONFIG_PLATFORM_EC_USB_POWER_DELIVERY` option enables USB-C power delivery
support on the Chromebook. See [Kconfig.pd] for sub-options related to this feature.

### Additional USB-C PD Configuration

The behavior of the USB-C PD implementation is further controlled through the
following options:

1. Shared PD Interrupts - See [Kconfig.pd_int_shared]
2. Measuring VBUS Voltage - See [Kconfig.pd_meas_vbus]
3. Detecting VBUS Voltage - See [Kconfig.pd_vbus_detection]
4. VBUS Discharge - See [Kconfig.pd_discharge]
5. Fast Role Swap - See [Kconfig.pd_frs]
6. Console Commands - See [Kconfig.pd_console_cmd]
7. USBC Device Type - See [Kconfig.pd_usbc_device_type]

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

The primary use of the port node is to describe what devices are connected to each port.
For example, the `named-usbc-port` can include the following devices:
	* [PPC]s
	* [TCPC]s
	* USB muxes and USB retimers
	* [BC12]

The two ports shown above both have the same three devices, but this is not required. Board
designs may use different USB-C devices on each USB-C port.

[USB-C]:../ec_terms.md#usb-c
[PD]:../usb-c.md#pd
[VBUS]:../ec_terms.md#vbus
[FRS]:../ec_terms.md#frs
[BC12]:../ec_terms.md#bc12
[PPC]:../usb-c.md#ppc
[TCPC]:../usb-c.md#tcpc
[Kconfig.pd]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd
[Kconfig.pd_int_shared]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared
[Kconfig.pd_meas_vbus]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus
[Kconfig.pd_frs]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_frs
[Kconfig.pd_discharge]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_discharge
[Kconfig.pd_vbus_detection]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_vbus_detection
[Kconfig.pd_console_cmd]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_console_cmd
[Kconfig.pd_usbc_device_type]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_usbc_device_type
