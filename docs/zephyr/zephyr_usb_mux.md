# Zephyr EC USB MUX Configuration

[TOC]

## Overview

Enable support for [USB_MUX].

## Kconfig Options

Kconfig Option               | Default | Documentation
:--------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_MUX` | y       | [USB MUX]

The following options are available only when `CONFIG_PLATFORM_EC_USB_MUX=y`.

Kconfig sub-option                    | Default | Documentation
:------------------------------------ | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_MUX_AMD_FP6`  | n       | [USB MUX AMD FP6]
`CONFIG_PLATFORM_EC_USB_MUX_IT5205`   | n       | [USB MUX IT5205]
`CONFIG_PLATFORM_EC_USB_MUX_PS8743`   | n       | [USB MUX PS8743]
`CONFIG_PLATFORM_EC_USB_MUX_TUSB1044` | n       | [USB MUX TUSB1044]

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

[USB_MUX]:../ec_terms.md#usb_mux
[USB MUX AMD FP6]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usb_mux?q=%22menuconfig%20PLATFORM_EC_USB_MUX_AMD_FP6%22&ss=chromiumos
[USB MUX IT5205]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usb_mux?q=%22menuconfig%20PLATFORM_EC_USB_MUX_IT5205%22&ss=chromiumos
[USB MUX PS8743]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usb_mux?q=%22menuconfig%20PLATFORM_EC_USB_MUX_PS8743%22&ss=chromiumos
[USB MUX TUSB1044]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usb_mux?q=%22menuconfig%20PLATFORM_EC_USB_MUX_TUSB1044%22&ss=chromiumos
