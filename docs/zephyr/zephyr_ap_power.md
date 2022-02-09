# Zephyr EC AP Power Requirements

[TOC]

## Overview

[AP] power configures the minimum amount of power needed to boot the Application
Processor.

## Kconfig Options

The Kconfig option CONFIG_PLATFORM_EC_BOOT_AP_POWER_REQUIREMENTS enables checking power
thresholds before booting the AP.  See the file [Kconfig.ap_power] for all Kconfig
options related to this feature.

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
[Kconfig.ap_power]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ap_power
