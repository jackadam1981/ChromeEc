# Zephyr EC PD Console Command Configuration

[TOC]

## Overview

Enable USB-C PD console commands.

## Kconfig Options

Kconfig Option                          | Default | Documentation
:-------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_CONSOLE_CMD` | y       | [PD CONSOLE CMD]

The following options are available only when `CONFIG_PLATFORM_EC_USB_PD_CONSOLE_CMD=y`.

Kconfig sub-option                            | Default | Documentation
:-------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_CONSOLE_CMD_USB_PD_PE`    | y       | [CONSOLE CMD PD PE]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_USB_PD_CABLE` | y       | [CONSOLE CMD PD CABLE]

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

[PD CONSOLE CMD]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_console_cmd?q=%22menuconfig%20PLATFORM_EC_USB_PD_CONSOLE_CMD%22&ss=chromiumos
[CONSOLE CMD PD PE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_console_cmd?q=%22menuconfig%20PLATFORM_EC_CONSOLE_CMD_USB_PD_PE%22&ss=chromiumos
[CONSOLE CMD PD CABLE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_console_cmd?q=%22menuconfig%20PLATFORM_EC_CONSOLE_CMD_USB_PD_CABLE%22&ss=chromiumos
