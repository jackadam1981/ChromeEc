# Zephyr EC PD Discharge Configuration

[TOC]

## Overview

When enabled, [VBUS] is discharged rapidly on disconnect.

## Kconfig Options

Kconfig Option                        | Default | Documentation
:------------------------------------ | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE` | y       | [PD DISCHARGE]

The following options are available only when `CONFIG_PLATFORM_EC_USB_PD_DISCHARGE=y`.

Kconfig sub-option                         | Default | Documentation
:----------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE_GPIO` | n       | [PD DISCHARGE GPIO]
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE_TCPC` | n       | [PD DISCHARGE TCPC]
`CONFIG_PLATFORM_EC_USB_PD_DISCHARGE_PPC`  | n       | [PD DISCHARGE PPC]

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

[VBUS]:../ec_terms.md#vbus
[PD DISCHARGE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_discharge?q=%22menuconfig%20PLATFORM_EC_USB_PD_DISCHARGE%22&ss=chromiumos
[PD DISCHARGE GPIO]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_discharge?q=%22menuconfig%20PLATFORM_EC_USB_PD_DISCHARGE_GPIO%22&ss=chromiumos
[PD DISCHARGE TCPC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_discharge?q=%22menuconfig%20PLATFORM_EC_USB_PD_DISCHARGE_TCPC%22&ss=chromiumos
[PD DISCHARGE PPC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_discharge?q=%22menuconfig%20PLATFORM_EC_USB_PD_DISCHARGE_PPC%22&ss=chromiumos
