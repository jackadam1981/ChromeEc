# Zephyr EC Feature Configuration Template

[TOC]

## Overview

The battery is the rechargeable internal power source for the device.

## Kconfig Options

`CONFIG_PLATFORM_EC_BATTERY` enables battery support in the EC application.
Refer to [Kconfig.battery] for all sub-options controlling battery behavior.

## Devicetree Nodes

### How to enable batteries on a board

#### Enable battery feature configs

Add battery configs to `ec/zephyr/projects/{project}/{board}/prj.conf`.

Example:

```
# Battery
CONFIG_PLATFORM_EC_BATTERY=y
CONFIG_PLATFORM_EC_BATTERY_SMART=y
CONFIG_PLATFORM_EC_BATTERY_FUEL_GAUGE=y
CONFIG_PLATFORM_EC_BATTERY_CUT_OFF=y
CONFIG_PLATFORM_EC_BATTERY_HW_PRESENT_CUSTOM=y
CONFIG_PLATFORM_EC_BATTERY_REVIVE_DISCONNECT=y

```

#### Add devicetree nodes

##### Add batteries devicetree node to the board overlay's root node

Example:

```
  batteries {
		default_battery: vendor_part {
			compatible = "vendor,part";
		};
		vendor2_part2 {
			compatible = "vendor2,part2";
		};
   };

```

Here `vendor_part` will be the default battery type. If this nodelabel is
present in the overlay, the [DEFAULT_BATTERY_TYPE] is set in the battery
shim. The `vendor` and `part` bits should match a battery in the aforementioned
[battery bindings directory].

##### Add the battery present GPIO node as a child of `named-gpios`

Example:

```
ec_batt_pres_odl {
	gpios = <{SOME GPIO} GPIO_INPUT>;
	label = "EC_BATT_PRES_ODL";
	enum-name = "GPIO_BATT_PRES_ODL";
};
```

##### Add a battery node as a child of `named-i2c-ports`

Example:

```
battery {
	i2c-port = <&i2c{BUS-NUMBER}_{PORT-NUMBER};
	remote-port = <{I2C_PASSTHRU-PORT-NUMBER}>;
	enum-name = "I2C_PORT_BATTERY";
	label = "BATTERY";
};
```

Refer to the [cros-ec-i2c-port-base.yaml] child-binding file for details about
each property.

[Example CL enabling batteries on a board]

### How to create a new battery

+ Add `vendor,part` to [battery-smart enum]
+ Add `vendor,part.yaml` to the [battery bindings directory] beginning with:

```
description: "VENDOR PART"
compatible: "vendor,part"

include: battery-smart.yaml

properties:
   enum-name:
      type: string
      default: "vendor,part"

```

[Example CL adding a new battery]



## Board Specific Code

Enabling batteries does not require any board specific code.

## Threads

Battery support does not enable any threads.

## Testing and Debugging

The `battery` [EC console command] may be invoked to check battery information
on a flashed board.

<!-- Reference Links -->

[DEFAULT_BATTERY_TYPE]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/shim/src/battery.c?q=%22DEFAULT_BATTERY_TYPE%22&ss=chromiumos
[EC console command]: https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/README.md#useful-ec-console-commands
[Example CL adding a new battery]: https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3312506/
[Example CL enabling batteries on a board]: https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3200068/
[Kconfig.battery]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery
[battery bindings directory]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/
[battery-smart enum]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/battery-smart.yaml?q=%22enum:%22&ss=chromiumos
[cros-ec-i2c-port-base.yaml]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/i2c/cros-ec-i2c-port-base.yaml
