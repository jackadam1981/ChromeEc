# Zephyr EC Charger

[TOC]

## Overview

The charger chip enables an external power supply to provide power to the
board.

## Kconfig Options

`CONFIG_PLATFORM_EC_CHARGER` enables charging support in the EC
application. Refer to [Kconfig.charger] for all sub-options controlling charging
behavior.

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
-->
[I2C Passthru Restricted]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_I2C_PASSTHRU_RESTRICTED%22&ss=chromiumos
[Kconfig.charger]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.charger?q=%22menuconfig%20PLATFORM_EC_CHARGER%22&ss=chromiumos
