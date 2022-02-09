# Zephyr EC USB-C SS MUX and Retimer Configuration

[TOC]

## Overview

Enable support for a [USB-C] [SuperSpeed Mux] and [Retimer].

## Kconfig Options

The Kconfig option `PLATFORM_EC_USBC_SS_MUX` enables the selection of
a [USB-C] [SuperSpeed Mux]. See the file [Kconfig.usbc_ss_mux] for all
Kconfig options related to this feature.

The Kconfig option `CONFIG_PLATFORM_EC_USBC` enable the selection of a [Retimer].
See the file [Kconfig.retimer] for all Kconfig options related to this feature.

## Devicetree Nodes

The `USB-C SuperSpeed Mux` device tree nodes are defined in the [`DTS Bindings`]
file for each type of `USB-C SuperSpeed Mux` that extends [cros-ec,usbc-mux-tcpci].

The `Retimer` device tree nodes are defined in the [`DTS Bindings`] file for
each type of `Retimer`.

## Board Specific Code

None required.

## Threads

USB-C SuperSpeed Mux and Retimer does not enable any threads.

## Testing and Debugging

The [`I2C bus scan`] can be used to verify the Retimer device can be accessed.

## Example
```
port0@0 {
	compatible = "named-usbc-port";
	reg = <0>;
	tcpc {
		compatible = "parade,ps8xxx";
		status = "okay";
		port = <&i2c_tcpc0>;
		i2c-addr-flags = "PS8XXX_I2C_ADDR1_FLAGS";
	};
	usb-muxes = <&usb_mux_0>;
};
usb_mux_0: usb-mux-0 {
	compatible = "parade,usbc-mux-ps8xxx";
};

&i2c3_0 {
	status = "okay";
	clock-frequency = <I2C_BITRATE_STANDARD>;

	usb_c0_bb_retimer: jhl8040r@56 {
		compatible = "intel,jhl8040r";
		reg = <0x56>;
		label = "USB_C0_BB_RETIMER";
		int-pin = <&usb_c0_rt_int_odl>;
		reset-pin = <&usb_c0_rt_rst_odl>;
	};

	usb_c2_bb_retimer: jhl8040r@57 {
		compatible = "intel,jhl8040r";
		reg = <0x57>;
		label = "USB_C2_BB_RETIMER";
		int-pin = <&usb_c2_rt_int_odl>;
		reset-pin = <&usb_c2_rt_rst_odl>;
	};
};
```
[USB-C]: ../usb-c.md
[SuperSpeed Mux]:../usb-c.md#ssmux
[Retimer]: ../ec_terms.md#retimer
[Kconfig.usbc_ss_mux]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usbc_ss_mux
[Kconfig.retimer]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer
[`I2C bus scan`]: ./zephyr_i2c.md#Shell-Command_i2c
[cros-ec,usbc-mux-tcpci]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/usbc/mux/cros-ec,usbc-mux-tcpci.yaml
[`DTS Bindings`]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/usbc/
