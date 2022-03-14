# Zephyr EC Charger

[TOC]

## Overview

The charger chip enables an external power supply to provide power to the
board's battery.

## Kconfig Options

`CONFIG_PLATFORM_EC_CHARGER` enables charging support in the EC
application. Refer to [Kconfig.charger] for all sub-options controlling charging
behavior.

Note: The charger chip configuration serves a different role than the USB
charging configuration found in [Kconfig.usb_charger].

### Example of enabled configs

In `ec/zephyr/projects/{project}/{board}/prj.conf`, add:

```
# Charger
CONFIG_PLATFORM_EC_CHARGER=y
CONFIG_PLATFORM_EC_CHARGE_RAMP_HW=y
CONFIG_PLATFORM_EC_CHARGER_DISCHARGE_ON_AC=y
CONFIG_PLATFORM_EC_CHARGER_DISCHARGE_ON_AC_CHARGER=y
CONFIG_PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON=2
CONFIG_PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON=10000
CONFIG_PLATFORM_EC_CHARGER_PROFILE_OVERRIDE=y
CONFIG_PLATFORM_EC_CHARGER_PSYS=y
CONFIG_PLATFORM_EC_CHARGER_PSYS_READ=y
CONFIG_PLATFORM_EC_CHARGER_SENSE_RESISTOR=10
CONFIG_PLATFORM_EC_CHARGER_SENSE_RESISTOR_AC=20
CONFIG_PLATFORM_EC_CONSOLE_CMD_CHARGER_ADC_AMON_BMON=y
```

## Devicetree Nodes
### How to add a charger chip devicetree node
#### Add charger devicetree node as a child of the board overlay's I2C node

Example:

```
&i2c_0 {
	tatus = "okay";
	lock-frequency = <I2C_BITRATE_STANDARD>;

	endor_part: vendor_part@{SOME_REG_VALUE} {
	compatible = "vendor,part";
	reg = {SOME_REG_VALUE};
	label = "VENDOR_PART";
	;
};

```

The vendor` and `part` references must match an existing charger chip defined
in [harger bindings directory].

See the I2C doc](./zephyr_i2c.md) for more information on configuring the I2C
node.

### How to create a new charger

Add `vendor,part.yaml` to the [charger bindings directory] beginning with:

Example Template:

```
description: Vendor Part Charger IC

compatible: "vendor,part"

include: i2c-device.yaml

```
See the I2C doc](./zephyr_i2c.md) for more information on configuring I2C nodes.

## TODO BELOW
## Board Specific Code

*Document any board specific routines that a user must create to successfully
compile and run. For many features, this can section can be empty.*

## Threads

*Document any threads enabled by this feature.*

## Testing and Debugging

*Provide any tips for testing and debugging the EC feature.*

## Example

*Provide code snippets from a working board to walk the user through
all code that must be created to enable this feature.*

<!--
The following demonstrates linking to a code search result for a Kconfig option.
Reference this link in your text by matching the text in brackets exactly.
-->
[Kconfig.charger]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.charger?q=%22menuconfig%20PLATFORM_EC_CHARGER%22&ss=chromiumos
[Kconfig.usb_charger]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.usb_charger?q=%22config%20PLATFORM_EC_USB_CHARGER%22&ss=chromiumos
[charger bindings directory]: https://crsrc.org/o/src/platform/ec/zephyr/dts/bindings/charger/
