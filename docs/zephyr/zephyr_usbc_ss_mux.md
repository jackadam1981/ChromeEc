# Zephyr EC USBC SS MUX Configuration

[TOC]

## Overview

Enable support for [USB-C SuperSpeed Mux]

## Kconfig Options

The Kconfig option `PLATFORM_EC_USBC_SS_MUX` enables the selection of
a [USB-C SuperSpeed Mux]. See the file [Kconfig.usbc_ss_mux] for all
Kconfig options related to this feature.

## Devicetree Nodes

The `USB-C SuperSpeed Mux` device tree nodes are defined in the [`DTS Bindings`]
file for each type of `USB-C SuperSpeed Mux` that extends [cros-ec,usbc-mux-tcpci].

## Board Specific Code

None required.

## Threads

USB-C SuperSpeed Mux does not enable any threads.

## Testing and Debugging

<HOW TO TEST THIS>

## Example

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

[USB-C SupperSpeed Mux]: ../ec_terms.md#usbc_ss_mux
[Kconfig.usbc_ss_mux]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usbc_ss_mux
[cros-ec,usbc-mux-tcpci]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/usbc/mux/cros-ec,usbc-mux-tcpci
