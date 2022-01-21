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

+ Add `{vendor},{part}.yaml` to [battery bindings directory] with:
  + prop1
  + prop1
  + prop1
+ Add `{vendor}_{part}` to [named-batteries enum].
+ Add `{vendor}_{part}` to [battery-smart enum].

### Enabling Board Batteries


<!-- *Detail the devicetree nodes that configure the feature.* -->

<!-- *Note - avoid documenting node properties here.  Point to the relevant `.yaml` -->
<!-- file instead, which contains the authoritative definition.* -->


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

[battery bindings directory]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/
[named-batteries enum]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/named-batteries.yaml?q=%22enum:%22&ss=chromiumos
[battery-smart enum]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/battery/battery-smart.yaml?q=%22enum:%22&ss=chromiumos
[rechargeable]: ../ec_terms.md#bc12

<!-- KCONFIG Links -->

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
