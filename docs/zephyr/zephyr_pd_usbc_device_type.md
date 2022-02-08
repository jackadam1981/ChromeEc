# Zephyr EC USB-C Device Type Configuration

[TOC]

## Overview

Configure the [USB-C] device type.

## Kconfig Options

Kconfig Option                          | Default | Documentation
:-------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_VPD`            | n       | [USB VPD]
`CONFIG_PLATFORM_EC_USB_CTVPD`          | n       | [USB CTVPD]
`CONFIG_PLATFORM_EC_USB_DRP_ACC_TRYSRC` | n       | [USB DRP ACC TRYSRC]

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

[USB-C]:../ec_terms.md#usb-c
[USB VPD]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_usbc_device_type?q=%22menuconfig%20PLATFORM_EC_USB_VPD%22&ss=chromiumos
[USB CTVPD]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_usbc_device_type?q=%22menuconfig%20PLATFORM_EC_USB_CTVPD%22&ss=chromiumos
[USB DRP ACC TRYSRC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_usbc_device_type?q=%22menuconfig%20PLATFORM_EC_USB_DRP_ACC_TRYSRC%22&ss=chromiumos
