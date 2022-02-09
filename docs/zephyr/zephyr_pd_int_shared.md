# Zephyr EC PD_INT_SHARED Configuration

[TOC]

## Overview

[PD_INT_SHARED] enables multiple USBC ports to share a single IRQ on the EC.

## Kconfig Options

Kconfig Option                         | Default | Documentation
:------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_INT_SHARED` | n       | [USB PD INT SHARED]

The following options are available only when `CONFIG_PLATFORM_EC_USB_PD_INT_SHARED=y`.

Kconfig sub-option                       | Default | Documentation
:--------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_PORT_0_SHARED | n       | [USB PD PORT 0 SHARED]
`CONFIG_PLATFORM_EC_USB_PD_PORT_1_SHARED | n       | [USB PD PORT 1 SHARED]
`CONFIG_PLATFORM_EC_USB_PD_PORT_2_SHARED | n       | [USB PD PORT 2 SHARED]
`CONFIG_PLATFORM_EC_USB_PD_PORT_3_SHARED | n       | [USB PD PORT 3 SHARED]

## Devicetree Nodes

TBD

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

[USB PD INT SHARED]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared?q=%22menuconfig%20PLATFORM_EC_USB_PD_INT_SHARED%22&ss=chromiumos
[USB PD PORT 0 SHARED]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared?q=%22menuconfig%20PLATFORM_EC_USB_PD_PORT_0_SHARED%22&ss=chromiumos
[USB PD PORT 1 SHARED]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared?q=%22menuconfig%20PLATFORM_EC_USB_PD_PORT_1_SHARED%22&ss=chromiumos
[USB PD PORT 2 SHARED]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared?q=%22menuconfig%20PLATFORM_EC_USB_PD_PORT_2_SHARED%22&ss=chromiumos
[USB PD PORT 3 SHARED]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared?q=%22menuconfig%20PLATFORM_EC_USB_PD_PORT_3_SHARED%22&ss=chromiumos

