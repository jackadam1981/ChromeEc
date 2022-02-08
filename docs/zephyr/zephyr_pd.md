# Zephyr EC PD Configuration

[TOC]

## Overview

Configure [USB-C] [PD] features that enables a port to provide power
greater than the basic 5V @ 900mA. Power can be negotiated up to
20V @ 5A.

## Kconfig Options

Kconfig Option                          | Default | Documentation
:-------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_POWER_DELIVERY` | y       | [USB Power Delivery]

The following options are available only when `CONFIG_PLATFORM_EC_USB_POWER_DELIVERY=y`.

Kconfig sub-option                                | Default | Documentation
:------------------------------------------------ | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_HOST_CMD`              | y       | [PD HOST CMD]
`CONFIG_PLATFORM_EC_USB_PD_PORT_MAX_COUNT`        | 2       | [PD PORT MAX COUNT]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_MFALLOW`          | y       | [CONSOLE CMD MFALLOW]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_PD`               | y       | [CONSOLE CMD PD]
`CONFIG_PLATFORM_EC_USB_PD_DEBUG_FIXED_LEVEL`     | n       | [PD DEBUG FIXED LEVEL]
`CONFIG_PLATFORM_EC_USB_PD_DEBUG_LEVEL`           | 0       | [PD DEBUG LEVEL]
`CONFIG_PLATFORM_EC_USB_PD_5V_EN_CUSTOM`          | n       | [PD 5V EN CUSTOM]
`CONFIG_PLATFORM_EC_USB_PD_5V_CHARGER_CTRL`       | n       | [PD 5V CHARGER CTRL]
`CONFIG_PLATFORM_EC_USBC_VCONN`                   | y       | [USBC VCONN]
`CONFIG_PLATFORM_EC_USB_PD_DUAL_ROLE`             | y       | [PD DUAL ROLE]
`CONFIG_PLATFORM_EC_USB_PD_DPS`                   | n       | [PD DPS]
`CONFIG_PLATFORM_EC_USB_PD_DUAL_ROLE_AUTO_TOGGLE` | y       | [PD DUAL ROLE AUTO TOGGLE]
`CONFIG_PLATFORM_EC_USB_PD_REV30`                 | y       | [PD REV30]
`CONFIG_PLATFORM_EC_USB_PD_ALT_MODE`              | y       | [PD ALT MODE]
`CONFIG_PLATFORM_EC_USB_PD_REQUIRE_AP_MODE_ENTRY` | n       | [PD REQUIRE AP MODE ENTRY]
`CONFIG_PLATFORM_EC_USB_PD_ALT_MODE_DFP`          | y       | [PD ALT MODE DFP]
`CONFIG_PLATFORM_EC_USB_PD_ALT_MODE_UFP`          | n       | [PD ALT MODE UFP]
`CONFIG_PLATFORM_EC_USB_PD_USB32_DRD`             | y       | [PD USB32 DRD]
`CONFIG_PLATFORM_EC_USB_PD_DP_HPD_GPIO`           | n       | [PD DP HPD GPIO]
`CONFIG_PLATFORM_EC_USB_PD_DP_HPD_GPIO_CUSTOM`    | n       | [PD DP HPD GPIO CUSTOM]
`CONFIG_PLATFORM_EC_USB_PD_DATA_RESET_MSG`        | n       | [PD DATA RESET MSG]
`CONFIG_PLATFORM_EC_USB_TYPEC_SM`                 | y       | [TYPEC SM]
`CONFIG_PLATFORM_EC_USB_PRL_SM`                   | y       | [PRL SM]
`CONFIG_PLATFORM_EC_USB_PE_SM`                    | y       | [PE SM]
`CONFIG_PLATFORM_EC_USB_PD_DECODE_SOP`            | y       | [PD DECODE SOP]
`CONFIG_PLATFORM_EC_HOSTCMD_PD_CONTROL`           | y       | [HOSTCMD PD CONTROL]
`CONFIG_PLATFORM_EC_USB_PD_LOGGING`               | n       | [PD LOGGING]
`CONFIG_PLATFORM_EC_USB_PD_TRY_SRC`               | y       | [PD TRY SRC]
`CONFIG_PLATFORM_EC_USB_PD_USB4`                  | y       | [PD USB4]
`CONFIG_PLATFORM_EC_USB_PD_TBT_COMPAT_MODE`       | y       | [PD TBT COMPAT MODE]
zephyr_pd_int_shared.md
zephyr_pd_meas_vbus.md
zephyr_pd_frs.md
zephyr_pd_discharge.md
zephyr_pd_vbus_detection.md
zephyr_pd_console_cmd.md
zephyr_pd_usbc_device_type.md

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
[PD]:../ec_terms.md#pd
[PD HOST CMD]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_HOST_CMD%22&ss=chromiumos
[PD PORT MAX COUNT]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_PORT_MAX_COUNT%22&ss=chromiumos
[CONSOLE CMD MFALLOW]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_CONSOLE_CMD_MFALLOW%22&ss=chromiumos
[CONSOLE CMD PD]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_CONSOLE_CMD_PD%22&ss=chromiumos
[PD DEBUG FIXED LEVEL]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DEBUG_FIXED_LEVEL%22&ss=chromiumos
[PD DEBUG LEVEL]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DEBUG_LEVEL%22&ss=chromiumos
[PD 5V EN CUSTOM]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_5V_EN_CUSTOM%22&ss=chromiumos
[PD 5V CHARGER CTRL]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_5V_CHARGER_CTRL%22&ss=chromiumos
[USBC VCONN]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USBC_VCONN%22&ss=chromiumos
[PD DUAL ROLE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DUAL_ROLE%22&ss=chromiumos
[PD DPS]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DPS%22&ss=chromiumos
[PD DUAL ROLE AUTO TOGGLE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DUAL_ROLE_AUTO_TOGGLE%22&ss=chromiumos
[PD REV30]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_REV30%22&ss=chromiumos
[PD ALT MODE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_ALT_MODE%22&ss=chromiumos
[PD REQUIRE AP MODE ENTRY]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_REQUIRE_AP_MODE_ENTRY%22&ss=chromiumos
[PD ALT MODE DFP]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_ALT_MODE_DFP%22&ss=chromiumos
[PD ALT MODE UFP]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_ALT_MODE_UFP%22&ss=chromiumos
[PD USB32 DRD]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_USB32_DRD%22&ss=chromiumos
[PD DP HPD GPIO]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DP_HPD_GPIO%22&ss=chromiumos
[PD DP HPD GPIO CUSTOM]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DP_HPD_GPIO_CUSTOM%22&ss=chromiumos
[PD DATA RESET MSG]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DATA_RESET_MSG%22&ss=chromiumos
[TYPEC SM]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_TYPEC_SM%22&ss=chromiumos
[PRL SM]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PRL_SM%22&ss=chromiumos
[PE SM]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PE_SM%22&ss=chromiumos
[PD DECODE SOP]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_DECODE_SOP%22&ss=chromiumos
[HOSTCMD PD CONTROL]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_HOSTCMD_PD_CONTROL%22&ss=chromiumos
[PD LOGGING]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_LOGGING%22&ss=chromiumos
[PD TRY SRC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_TRY_SRC%22&ss=chromiumos
[PD USB4]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_USB4%22&ss=chromiumos
[PD TBT COMPAT MODE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd?q=%22menuconfig%20PLATFORM_EC_USB_PD_TBT_COMPAT_MODE%22&ss=chromiumos
