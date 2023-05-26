# Zephyr EC TCPC Configuration

[TOC]

## Overview

Enable support for a [USB-C] [TCPC].

## Kconfig Options

See the file [Kconfig.tcpm] for all Kconfig options related to [TCPC].

Note that the Kconfig option to enable the TCPC driver is automatically enabled
based on the devicetree contents.

For example, a devicetree can enable the Analogix ANX7447 device with this
devicetree snippet:

```c
	tcpc_port0: anx7447-tcpc@2c {
		compatible = "analogix,anx7447-tcpc";
		status = "okay";
		reg = <0x2c>;
		/* ... other required properties */
	};
```

The build system automatically enables the corresponding Kconfig option
`CONFIG_PLATFORM_EC_USB_PD_TCPM_ANX7447` because the devicetree contains a node
with the compatible string `"analogix,anx7447-tcpc"`.

You should check the [Kconfig.tcpm] file for any Kconfig option related to the
specific TCPC drivers used by your design. For the Analogix TCPC, this includes
`CONFIG_PLATFORM_EC_USB_PD_TCPM_ANX7447_AUX_PU_PD` and potentially other
options.

## Devicetree Nodes

The `TCPC` device tree nodes are defined in the [`DTS Bindings TCPC`] directory.
The `TCPC` device is added to the corresponding I2C bus node and the
"named-usbc-port" contains a phandle to the `TCPC` device

## Board Specific Code

None required.

## Threads

TCPC support does not enable any threads.

## Testing and Debugging

The [`I2C bus scan`] can be used to verify the TCPC device can be accessed and
the `tcpc_dump` console command can be used to dump the TCPC register.

```
Usage: tcpc_dump <USB-C port>
```

## Example

The Herobrine system uses the Parade PS8805 TCPC on USBC port 0.

```
CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8805=y
```

```
port0@0 {
	compatible = "named-usbc-port";
	reg = <0>;
	tcpc = <&tcpc_port0>;
};

&i2c1_0 {
	tcpc_port0: ps8xxx@b {
		compatible = "parade,ps8xxx";
		reg = <0xb>;
	};
};
```

[USB-C]: ../usb-c.md
[TCPC]: ../ec_terms.md#tcpc
[`I2C bus scan`]: ./zephyr_i2c.md#Shell-Command_i2c
[Kconfig.tcpm]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.tcpm
[`DTS Bindings TCPC`]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/usbc/tcpc
