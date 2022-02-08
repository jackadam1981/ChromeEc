# Zephyr EC PD VBUS Detection Configuration

[TOC]

## Overview

Configure how [VBUS] is detected.

## Kconfig Options

Kconfig Option                                  | Default | Documentation
:---------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_NONE`    | n       | [VBUS DETECT NONE]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_TCPC`    | n       | [VBUS DETECT TCPC]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_CHARGER` | n       | [VBUS DETECT CHARGER]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_DETECT_PPC`     | n       | [VBUS DETECT PPC]

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
[VBUS DETECT NONE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_vbus_detection?q=%22menuconfig%20PLATFORM_EC_USB_PD_VBUS_DETECT_NONE%22&ss=chromiumos
[VBUS DETECT TCPC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_vbus_detection?q=%22menuconfig%20PLATFORM_EC_USB_PD__VBUS_DETECT_TCPC%22&ss=chromiumos
[VBUS DETECT CHARGER]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_vbus_detection?q=%22menuconfig%20PLATFORM_EC_USB_PD__VBUS_DETECT_CHARGER%22&ss=chromiumos
[VBUS DETECT PPC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_vbus_detection?q=%22menuconfig%20PLATFORM_EC_USB_PD__VBUS_DETECT_PPC%22&ss=chromiumos
