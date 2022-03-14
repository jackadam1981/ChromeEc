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

## Devicetree Nodes

### How to enable charging on a board

#### Enable charger feature configs

Add charger configs to `ec/zephyr/projects/{project}/{board}/prj.conf`.

Example:

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
[I2C Passthru Restricted]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_I2C_PASSTHRU_RESTRICTED%22&ss=chromiumos
[Kconfig.charger]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.charger?q=%22menuconfig%20PLATFORM_EC_CHARGER%22&ss=chromiumos
[Kconfig.usb_charger]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.usb_charger?q=%22config%20PLATFORM_EC_USB_CHARGER%22&ss=chromiumos
