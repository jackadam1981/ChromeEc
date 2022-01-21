# Zephyr EC Feature Configuration Template

[TOC]

## Overview

<!-- *Description of the Zephyr EC feature and the capabilities provided* -->

The battery is a [rechargeable] internal power source for the device.

TODO - Add more details.

## Kconfig Options

| Kconfig Option                                    | Default | Documentation   |
|:--------------------------------------------------|:-------:|:----------------|
| `CONFIG_PLATFORM_EC_BATTERY_DEVICE`               | n       | [battery]       |

The following options are available only when `CONFIG_PLATFORM_EC_BATTERY=y`.

| Kconfig `CONFIG_PLATFORM_EC_BATTERY` sub-option        | Default                         | Documentation                       |
|:-------------------------------------------------------|:-------------------------------:|:------------------------------------|
| `CONFIG_PLATFORM_EC_BATTERY_CHECK_CHARGE_TEMP_LIMITS`  | n                               | [battery_check_charge_temp_limits]  |
| `CONFIG_PLATFORM_EC_BATTERY_CRITICAL_SHUTDOWN_CUT_OFF` | n                               | [battery_critical_shutdown_cut_off] |
| `CONFIG_PLATFORM_EC_BATTERY_CUT_OFF`                   | n                               | [battery_cut_off]                   |
| `CONFIG_PLATFORM_EC_BATTERY_DEVICE_CHEMISTRY`          | n                               | [battery_device_chemistry]          |
| `CONFIG_PLATFORM_EC_BATTERY_FUEL_GAUGE`                | n                               | [battery_fuel_gauge]                |
| `CONFIG_PLATFORM_EC_BATTERY_HW_PRESENT_CUSTOM`         | n                               | [battery_hw_present_custom]         |
| `CONFIG_PLATFORM_EC_BATTERY_MAX_IMBALANCE_MV`          | 200                             | [battery_max_imbalance_mv]          |
| `CONFIG_PLATFORM_EC_BATTERY_MEASURE_IMBALANCE`         | n                               | [battery_measure_imbalance]         |
| `CONFIG_PLATFORM_EC_BATTERY_PRESENT_CUSTOM`            | n                               | [battery_present_custom]            |
| `CONFIG_PLATFORM_EC_BATTERY_PRESENT_GPIO`              | n                               | [battery_present_gpio]              |
| `CONFIG_PLATFORM_EC_BATTERY_REVIVE_DISCONNECT`         | n                               | [battery_revive_disconnect]         |
| `CONFIG_PLATFORM_EC_BATTERY_SMART`                     | n                               | [battery_smart]                     |
| `CONFIG_PLATFORM_EC_BATTERY_TYPE_NO_AUTO_DETECT`       | n                               | [battery_type_no_auto_detect]       |
| `CONFIG_PLATFORM_EC_BATTERY_V1`                        | n                               | [battery_v1]                        |
| `CONFIG_PLATFORM_EC_BATTERY_V2`                        | n                               | [battery_v2]                        |
| `CONFIG_PLATFORM_EC_I2C_VIRTUAL_BATTERY`               | n                               | [i2c_virtual_battery]               |
| `CONFIG_PLATFORM_EC_I2C_VIRTUAL_BATTERY_ADDR`          | `0x0B if smart battery enabled` | [i2c_virtual_battery_addr]          |
| `CONFIG_PLATFORM_EC_USE_BATTERY_DEVICE_CHEMISTRY`      | n                               | [use_battery_device_chemistry]      |

The following options are available only when `CONFIG_PLATFORM_EC_BATTERY_V2=y`.

| Kconfig `CONFIG_PLATFORM_EC_BATTERY` sub-option | Default | Documentation   |
|:------------------------------------------------|:-------:|:----------------|
| `CONFIG_PLATFORM_EC_BATTERY_COUNT`              | 1       | [battery_count] |

## Devicetree Nodes

### Create a Battery

+ Add `vendor_part` to [named-batteries enum]
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

[battery-smart.yaml]
[Example CL adding a new battery]:
(https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3312506/)

### Enable Batteries on a Board

#### Enable Battery Feature CONFIGS

In `ec/zephyr/projects/{project}/{board}/prj.conf`, add
the battery configs.

For example:

```
# Battery
CONFIG_PLATFORM_EC_BATTERY=y
CONFIG_PLATFORM_EC_BATTERY_SMART=y
CONFIG_PLATFORM_EC_BATTERY_FUEL_GAUGE=y
CONFIG_PLATFORM_EC_BATTERY_CUT_OFF=y
CONFIG_PLATFORM_EC_BATTERY_HW_PRESENT_CUSTOM=y
CONFIG_PLATFORM_EC_BATTERY_REVIVE_DISCONNECT=y

```

#### Add batteries devicetree node to the board overlay's root

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

Here `vendor_part` will be the default battery. The `vendor` and `part` bits
will have to match a battery added to the aforementioned [battery bindings
directory].

#### Add I2C devicetree nodes

Add the battery present GPIO signal child node to the `named-gpios` node.

Example:

```
ec_batt_pres_odl {
	gpios = <{SOME GPIO} GPIO_INPUT>;
	label = "EC_BATT_PRES_ODL";
	enum-name = "GPIO_BATT_PRES_ODL";
};
```

Add a battery node the `named-i2c-ports` node.

Example:

```
battery {
	i2c-port = <&i2c{BUS-NUMBER}_{PORT-NUMBER};
	remote-port = <I2C_PASSTHRU-PORT-NUMBER>;
	enum-name = "I2C_PORT_BATTERY";
	label = "BATTERY";
};
```

[Example CL enabling batteries on a board]:
(https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3200068/)

## Board Specific Code

Enabling batteries does not require any board specific code.

## Threads

Battery support does not enable any threads.

## Testing and Debugging

One can check for battery information with the `battery` EC console command on a
flashed board.

[battery bindings directory]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/
[battery-smart enum]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/battery-smart.yaml?q=%22enum:%22&ss=chromiumos
[named-batteries enum]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/named-batteries.yaml?q=%22enum:%22&ss=chromiumos
[rechargeable]: ../ec_terms.md#bc12

<!-- Begin KCONFIG Links -->

[battery]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22menuconfig%20PLATFORM_EC_BATTERY%22&ss=chromiumos
[battery_check_charge_temp_limits]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CHECK_CHARGE_TEMP_LIMITS%22&ss=chromiumos
[battery_count]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_COUNT%22&ss=chromiumos
[battery_critical_shutdown_cut_off]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CRITICAL_SHUTDOWN_CUT_OFF%22&ss=chromiumos
[battery_cut_off]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CUT_OFF%22&ss=chromiumos
[battery_device_chemistry]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_DEVICE_CHEMISTRY%22&ss=chromiumos
[battery_fuel_gauge]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_FUEL_GAUGE%22&ss=chromiumos
[battery_hw_present_custom]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_HW_PRESENT_CUSTOM%22&ss=chromiumos
[battery_max_imbalance_mv]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_MAX_IMBALANCE_MV%22&ss=chromiumos
[battery_measure_imbalance]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_MEASURE_IMBALANCE%22&ss=chromiumos
[battery_present_custom]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_PRESENT_CUSTOM%22&ss=chromiumos
[battery_present_gpio]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_PRESENT_GPIO%22&ss=chromiumos
[battery_revive_disconnect]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_REVIVE_DISCONNECT%22&ss=chromiumos
[battery_smart]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_SMART%22&ss=chromiumos
[battery_type_no_auto_detect]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_TYPE_NO_AUTO_DETECT%22&ss=chromiumos
[battery_v1]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_V1%22&ss=chromiumos
[battery_v2]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_V2%22&ss=chromiumos
[i2c_virtual_battery]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_I2C_VIRTUAL_BATTERY%22&ss=chromiumos
[i2c_virtual_battery_addr]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_I2C_VIRTUAL_BATTERY_ADDR%22&ss=chromiumos
[use_battery_device_chemistry]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_USE_BATTERY_DEVICE_CHEMISTRY%22&ss=chromiumos
