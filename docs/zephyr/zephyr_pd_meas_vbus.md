# Zephyr EC VBUS Measuring Configuration

[TOC]

## Overview

Select how [VBUS] voltage is measured.

## Kconfig Options

Kconfig Option                                         | Default | Documentation
:----------------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_NOT_PRESENT`   | n       | [VBUS MEAS NOT PRESENT]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_CHARGER`       | n       | [VBUS MEAS CHARGER]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_TCPC`          | n       | [VBUS MEAS TCPC]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_ADC_EACH_PORT` | n       | [VBUS MEAS ADC]
`CONFIG_PLATFORM_EC_USB_PD_VBUS_MEASURE_BY_BOARD`      | n       | [VBUS MEAS BY BOARD]

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
[VBUS MEAS NOT PRESENT]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus?q=%22menuconfig%20PLATFORM_EC_USB_PD_VBUS_MEASURE_NOT_PRESENT%22&ss=chromiumos
[VBUS MEAS CHARGER]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus?q=%22menuconfig%20PLATFORM_EC_USB_PD_VBUS_MEASURE_CHARGER%22&ss=chromiumos
[VBUS MEAS TCPC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus?q=%22menuconfig%20PLATFORM_EC_USB_PD_VBUS_MEASURE_TCPC%22&ss=chromiumos
[VBUS MEAS ADC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus?q=%22menuconfig%20PLATFORM_EC_USB_PD_VBUS_MEASURE_ADC_EACH_PORT%22&ss=chromiumos
[VBUS MEAS BY BOARD]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus?q=%22menuconfig%20PLATFORM_EC_USB_PD_VBUS_MEASURE_BY_BOARD%22&ss=chromiumos

