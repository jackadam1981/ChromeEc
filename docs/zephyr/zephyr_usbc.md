# Zephyr EC USBC Configuration

[TOC]

## Overview

[USB-C] is a flexible connector supporting multiple data rates, protocols, and
power in either direction.

From the system, USB PD requires a complex state machine as USB PD can
operate in many different modes. This includes but isn't limited to:

*   Negotiated power contracts. Either side of the cable can source or sink
    power up to 100W (if supported by device).
*   Reversed cable mode. This requires a mux to switch the signals before
    getting to the SoC (or AP).
*   Debug accessory mode, e.g. [Case Closed Debugging (CCD)]
*   Multiple uses for the 4 differential pair signals including
    *   USB SuperSpeed mode (up to 4 lanes for USB data)
    *   DisplayPort Alternate Mode (up to 4 lanes for DisplayPort data)
    *   Dock Mode (2 lanes for USB data, and 2 lanes for DisplayPort)
    *   Audio Accessory mode. (1 lane is used for L and R analog audio signal)
    *   USB4/Thunderbolt mode. (4 lanes for USB data)

For a more complete list of USB-C Power Delivery features, see the
[USB-C PD spec][USB PD Spec Id].

The image below shows a block diagram of a typical [USB-C] setup.

![USBC Block Diagram]

[AP]
[EC]
[PPC]
[TCPC]
[USB-C Mux]
[RETIMER]

## Kconfig Options

The `CONFIG_PLATFORM_EC_USBC` option enables USB-C support on the Chromebook.
See [Kconfig.usbc] for sub-options related to this feature.

See the following for USB-C sub-component configuruation and devicetree setup:
*   [zephyr_pd.md]
*   [zephyr_ppc.md]
*   [zephyr_usbc_ss_mux.md]
*   [zephyr_usb_charger.md]
*   [zephyr_retimer.md]

## Example

```
usbc {
	#address-cells = <1>;
	#size-cells = <0>;

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
		ppc_port0: ppc {
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
	chg {
		compatible = "intersil,isl923x";
		status = "okay";
		port = <&i2c_charger>;
	};
	usb-muxes = <&usb_mux_0>;
	};
	usb_mux_0: usb-mux-0 {
		compatible = "parade,usbc-mux-ps8xxx";
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
	usb-muxes = <&usb_mux_1>;
	};
	usb_mux_1: usb-mux-1 {
		compatible = "parade,usbc-mux-ps8xxx";
	};
};
```

[USB-C]:../ec_terms.md#usb-c
[AP]:../ec_terms.md#ap
[EC]:../ec_terms.md#ec
[PPC]:../ec_terms.md#ppc
[TCPC]:../ec_terms.md#tcpc
[USB-C Mux]:../ec_terms.md#ssmux
[RETIMER]:../ec_terms.md#retimer
[USBC Block Diagram]:../images/usbc_block_diagram.png
[USB PD Spec Id]: https://www.usb.org/document-library/usb-power-delivery
[Kconfig.usbc]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usbc
[zephyr_pd.md]:zephyr_pd.md
[zephyr_ppc.md]:zephyr_ppc.md
[zephyr_usbc_ss_mux.md]:zephyr_usbc_ss_mux.md
[zephyr_usb_charger.md]:zephyr_usb_charger.md
[zephyr_retimer.md]:zephyr_retimer.md
