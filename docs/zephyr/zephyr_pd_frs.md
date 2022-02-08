# Zephyr EC PD FRS Configuration

[TOC]

## Overview

Enables the protocol side of [FRS] that allows the device to switch from a Sink
to a Source (or vice versa) base on communication with the partner device.

## Kconfig Options

Kconfig Option                  | Default | Documentation
:------------------------------ | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_FRS` | n       | [PD FRS]

The following options are available only when `CONFIG_PLATFORM_EC_USB_PD_FRS=y`.

Kconfig sub-option                   | Default | Documentation
:----------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_FRS_PPC`  | n       | [PD FRS PPC]
`CONFIG_PLATFORM_EC_USB_PD_FRS_TCPC` | n       | [PD FRS TCPC]

## Devicetree Nodes

TBD

## Board Specific Code

TBD

## Threads

TBD

## Testing and Debugging

TBD

## Example

TBD

[FRS]:../ec_terms.md#frs
[PD FRS]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_frs?q=%22menuconfig%20PLATFORM_EC_USB_PD_FRS%22&ss=chromiumos
[PD FRS PPC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_frs?q=%22menuconfig%20PLATFORM_EC_USB_PD_FRS_PPC%22&ss=chromiumos
[PD FRS TCPC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_frs?q=%22menuconfig%20PLATFORM_EC_USB_PD_FRS_TCPC%22&ss=chromiumos

