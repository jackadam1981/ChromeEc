# Zephyr EC PPC Configuration

[TOC]

## Overview

Enable support for a [USB-C] [PPC].

## Kconfig Options

The Kconfig option `PLATFORM_EC_USBC_PPC` enables the selection of a [PPC].
See the file [Kconfig.ppc] for all Kconfig options related to this feature.

## Devicetree Nodes

The `PPC` device tree nodes are defined in the [`DTS Bindings`] file for
each type of `PPC` that extends [ppc-chip.yaml].

## Board Specific Code

None required.

## Threads

PPC support does not enable any threads.

## Testing and Debugging

The [`I2C bus scan`] can be used to verify the PPC device can be accessed.

## Example

The Hoglin system uses the Silergy SYV682X PPC on USBC port 0.

```
CONFIG_PLATFORM_EC_USBC_PPC=y
CONFIG_PLATFORM_EC_USBC_PPC_SYV682X=y
```

port0@0 {
	compatible = "named-usbc-port";
	reg = <0>;
	ppc_port0: ppc {
		compatible = "silergy,syv682x";
		status = "okay";
		port = <&i2c_tcpc0>;
		i2c-addr-flags = "SYV682X_ADDR1_FLAGS";
		frs_en_gpio = <&gpio_usb_c0_frs_en>;
	};
};

[USB-C]: ../ec_terms.md#usb-c
[PPC]: ../ec_terms.md#ppc
[`I2C bus scan`]: ./zephyr_i2c.md#Shell-Command_i2c
[Kconfig.ppc]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc
[`DTS Bindings`]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/usbc/
[ppc-chip.yaml]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/usbc/ppc-chip.yaml
