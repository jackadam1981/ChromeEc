# Zephyr EC AP Power Requirements

[TOC]

## Overview

[AP] power configures the minimum amount of power needed to boot the Application
Processor.

## Kconfig Options

Kconfig Option                                   | Default | Documentation
:----------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_BOOT_AP_POWER_REQUIREMENTS`  | y       | [Boot AP Power]

Kconfig sub-option                                        | Default | Documentation
:-------------------------------------------------------- | :-----: | :------------
`PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON`            | 3       | [CHARGER min batt pwr ON]
`PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON_WITH_AC`    | 1       | [CHARGER min batt pwr ON with AC]
`PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON_WITH_BATT` | 15000   | [CHARGER min pwr for power on wth batt]
`PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON`           | 15000   | [CHARGER min pwr for power on]

## Devicetree Nodes

None required.

## Board Specific Code

None required.

## Threads

AP_POWER support does not enable any threads.

## Testing and Debugging

TBD

## Example

For Herobrine, the minimum battery level to boot the AP without AC is 2 percent
of the battery capacity and the minimum AC power to boot the AP with a battery
is set to 10000 milliwats.

CONFIG_PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON=2
CONFIG_PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON=10000


[AP]: ../ec_terms.md#ap
[Boot AP Power]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ap_power?q=%22menuconfig%20PLATFORM_EC_BOOT_AP_POWER_REQUIREMENTS%22&ss=chromiumos
[CHARGER min batt pwr ON]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ap_power?q=%22menuconfig%20PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON%22&ss=chromiumos
[CHARGER min batt pwr ON with AC]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ap_power?q=%22menuconfig%20PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON_WITH_AC%22&ss=chromiumos
[CHARGER min pwr for power on wth batt]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ap_power?q=%22menuconfig%20PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON_WITH_BATT%22&ss=chromiumos
[CHARGER min pwr for power on]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ap_power?q=%22menuconfig%20PLATFORM_EC_CHARGER__MIN_POWER_MW_FOR_POWER_ON%22&ss=chromiumos
