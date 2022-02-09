# Zephyr EC PD_INT_SHARED Configuration

[TOC]

## Overview

PD_INT_SHARED enables multiple USBC ports to share a single IRQ on the EC.

## Kconfig Options

[Kconfig.pd_int_shared]

## Devicetree Nodes

None required.

## Board Specific Code

None required.

## Threads

PD_INT_SHARED enabled a thread that services PD message interrupts.

## Testing and Debugging

TBD

## Example

The Brya device is configured to share a common IRQ between ports 0 and 2.

CONFIG_PLATFORM_EC_USB_PD_INT_SHARED=y
CONFIG_PLATFORM_EC_USB_PD_PORT_0_SHARED=y
CONFIG_PLATFORM_EC_USB_PD_PORT_2_SHARED=y

[Kconfig.pd_int_shared]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared

