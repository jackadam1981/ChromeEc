# Zephyr EC Feature Configuration Template

[TOC]

## Overview

*Description of the Zephyr EC feature and the capabilities provided*

The battery is a [rechargable] internal power source for the device.

## Kconfig Options

| Kconfig Option                                    | Default | Documentation   |
|:--------------------------------------------------|:-------:|:----------------|
| `CONFIG_PLATFORM_EC_BATTERY_DEVICE`               | n       | [battery]       |

The following options are available only when `CONFIG_PLATFORM_EC_BATTERY=y`.

| Kconfig `CONFIG_PLATFORM_EC_BATTERY` sub-option   | Default | Documentation           |
|:--------------------------------------------------|:-------:|:------------------------|
| `CONFIG_PLATFORM_EC_BATTERY_DEVICE_CHEMISTRY`     | n       | [battery device chemistry] |
| `CONFIG_PLATFORM_EC_BATTERY_PRESENT_CUSTOM`       | n       | [battery_smart] |
| `CONFIG_PLATFORM_EC_BATTERY_PRESENT_GPIO`         | n       | [battery_smart] |
| `CONFIG_PLATFORM_EC_I2C_VIRTUAL_BATTERY_ADDR`     | n       | [battery_smart] |
| `CONFIG_PLATFORM_EC_I2C_VIRTUAL_BATTERY`          | n       | [battery_smart] |
| `CONFIG_PLATFORM_EC_SMART`                        | n       | [battery_smart] |
| `CONFIG_PLATFORM_EC_USE_BATTERY_DEVICE_CHEMISTRY` | n       | [battery_smart] |


*Note - Avoid documenting `CONFIG_` options in the markdown as the relevant
`Kconfig*` contains the authoritative definition. Link directly to the Kconfig
option in source like this: [I2C Passthru Restricted].*

## Devicetree Nodes

*Detail the devicetree nodes that configure the feature.*

*Note - avoid documenting node properties here.  Point to the relevant `.yaml`
file instead, which contains the authoritative definition.*

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

https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery;l=23
-->

[battery]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22menuconfig%20PLATFORM EC BATTERY%22&ss=chromiumos
[battery check charge temp limits]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CHECK_CHARGE_TEMP_LIMITS%22&ss=chromiumos
[battery count]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC BATTERY_COUNT%22&ss=chromiumos
[battery critical shutdown cut off]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CRITICAL_SHUTDOWN_CUT_OFF%22&ss=chromiumos
[battery cut off]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_BATTERY_CUT_OFF%22&ss=chromiumos
[battery device chemistry]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_BATTERY_DEVICE_CHEMISTRY%22&ss=chromiumos
[battery fuel gauge]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_BATTERY_FUEL_GAUGE%22&ss=chromiumos
[battery hw present custom]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_HW_PRESENT_CUSTOM%22&ss=chromiumos
[battery max imbalance mv]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_MAX_IMBALANCE_MV%22&ss=chromiumos
[battery measure imbalance]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_BATTERY_MEASURE_IMBALANCE%22&ss=chromiumos
[battery present custom]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_BATTERY_PRESENT_CUSTOM%22&ss=chromiumos
[battery present gpio]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_BATTERY_PRESENT_GPIO%22&ss=chromiumos
[battery revive disconnect]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_BATTERY_REVIVE_DISCONNECT%22&ss=chromiumos
[battery smart]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC BATTERY_SMART%22&ss=chromiumos
[battery type no auto detect]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_TYPE_NO_AUTO_DETECT%22&ss=chromiumos
[battery v1]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC BATTERY_V1%22&ss=chromiumos
[battery v2]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC BATTERY_V2%22&ss=chromiumos
[i2c virtual battery]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM EC_I2C_VIRTUAL_BATTERY%22&ss=chromiumos
[i2c virtual battery addr]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_I2C_VIRTUAL_BATTERY_ADDR%22&ss=chromiumos
[rechargable]: ../ec_terms.md#bc12
[battery use battery device chemistry]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_USE_BATTERY_DEVICE_CHEMISTRY%22&ss=chromiumos
