# Zephyr EC Feature Configuration Template

[TOC]

## Overview

*Description of the Zephyr EC feature and the capabilities provided*

The battery is a [rechargable] internal power source for the device.

## Kconfig Options

| Kconfig Option                                    | Default | Documentation   |
|:--------------------------------------------------|:-------:|:----------------|
| `CONFIG_PLATFORM_EC_BATTERY_DEVICE`               | n       | [BATTERY]       |

The following options are available only when `CONFIG_PLATFORM_EC_BATTERY=y`.

| Kconfig `CONFIG_PLATFORM_EC_BATTERY` sub-option   | Default | Documentation           |
|:--------------------------------------------------|:-------:|:------------------------|
| `CONFIG_PLATFORM_EC_BATTERY_DEVICE_CHEMISTRY`     | n       | [BATTERY_SMART] |
| `CONFIG_PLATFORM_EC_BATTERY_PRESENT_CUSTOM`       | n       | [BATTERY_SMART] |
| `CONFIG_PLATFORM_EC_BATTERY_PRESENT_GPIO`         | n       | [BATTERY_SMART] |
| `CONFIG_PLATFORM_EC_I2C_VIRTUAL_BATTERY_ADDR`     | n       | [BATTERY_SMART] |
| `CONFIG_PLATFORM_EC_I2C_VIRTUAL_BATTERY`          | n       | [BATTERY_SMART] |
| `CONFIG_PLATFORM_EC_SMART`                        | n       | [BATTERY_SMART] |
| `CONFIG_PLATFORM_EC_USE_BATTERY_DEVICE_CHEMISTRY` | n       | [BATTERY_SMART] |


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
[I2C Passthru Restricted]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_I2C_PASSTHRU_RESTRICTED%22&ss=chromiumos

[rechargable]:../ec_terms.md#bc12

[BATTERY]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22menuconfig%20PLATFORM_EC_BATTERY%22&ss=chromiumos

[BATTERY_SMART]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_SMART%22&ss=chromiumos
[BATTERY_PRESENT_CUSTOM]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_PRESENT_CUSTOM%22&ss=chromiumos
[BATTERY_PRESENT_GPIO]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_PRESENT_GPIO%22&ss=chromiumos
[USE_BATTERY_DEVICE_CHEMISTRY]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_USE_BATTERY_DEVICE_CHEMISTRY%22&ss=chromiumos
[BATTERY_DEVICE_CHEMISTRY]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_DEVICE_CHEMISTRY%22&ss=chromiumos
[I2C_VIRTUAL_BATTERY]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_I2C_VIRTUAL_BATTERY%22&ss=chromiumos
[I2C_VIRTUAL_BATTERY_ADDR]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_I2C_VIRTUAL_BATTERY_ADDR%22&ss=chromiumos
[BATTERY_CRITICAL_SHUTDOWN_CUT_OFF]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CRITICAL_SHUTDOWN_CUT_OFF%22&ss=chromiumos
[BATTERY_CHECK_CHARGE_TEMP_LIMITS]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CHECK_CHARGE_TEMP_LIMITS%22&ss=chromiumos
[BATTERY_CUT_OFF]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_CUT_OFF%22&ss=chromiumos
[BATTERY_FUEL_GAUGE]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_FUEL_GAUGE%22&ss=chromiumos
[BATTERY_HW_PRESENT_CUSTOM]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_HW_PRESENT_CUSTOM%22&ss=chromiumos
[BATTERY_REVIVE_DISCONNECT]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_REVIVE_DISCONNECT%22&ss=chromiumos
[BATTERY_MEASURE_IMBALANCE]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_MEASURE_IMBALANCE%22&ss=chromiumos
[BATTERY_MAX_IMBALANCE_MV]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_MAX_IMBALANCE_MV%22&ss=chromiumos
[BATTERY_V1]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_V1%22&ss=chromiumos
[BATTERY_V2]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_V2%22&ss=chromiumos
[BATTERY_TYPE_NO_AUTO_DETECT]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_TYPE_NO_AUTO_DETECT%22&ss=chromiumos
[BATTERY_COUNT]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.battery?q=%22CONFIG%20PLATFORM_EC_BATTERY_COUNT%22&ss=chromiumos
