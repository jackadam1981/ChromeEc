# Zephyr EC Retimer Configuration

[TOC]

## Overview

[RETIMER] enables support for a retimer.

## Kconfig Options

Kconfig Option                                            | Default | Documentation
:-------------------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB`                | n       | [RETIMER Intel BB]
`CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG` | y       | [RETIMER Intel BB runtime config]
`CONFIG_PLATFORM_EC_USBC_RETIMER_ANX7451`                 | n       | [RETIMER ANX7451]
`CONFIG_PLATFORM_EC_USBC_RETIMER_PS8811`                  | n       | [RETIMER PS8811]
`CONFIG_PLATFORM_EC_USBC_RETIMER_PS8818`                  | n       | [RETIMER PS8818]
`CONFIG_PLATFORM_EC_USBC_RETIMER_KB800X`                  | n       | [RETIMER KB800X]
`CONFIG_PLATFORM_EC_KB800X_CUSTOM_XBAR`                   | n       | [RETIMER KB800X CUSTOM XBAR]

## Devicetree Nodes

TBD

## Board Specific Code

None required.

## Threads

RETIMER support does not enable any threads.

## Testing and Debugging

TBD

## Example

The Volteer system uses the Intel BB retimer on USBC port 1. The retimer is an
I2C device with address 0x40.

CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB=y

&i2c3_0 {
        status = "okay";
        clock-frequency = <I2C_BITRATE_STANDARD>;

        usb_c1_bb_retimer: jhl8040r@40 {
                compatible = "intel,jhl8040r";
                reg = <0x40>;
                label = "USB_C1_BB_RETIMER";
                int-pin = <&gpio_usb_c1_mix_int_odl>;
                reset-pin = <&gpio_usb_c1_rt_rst_odl>;
                ls-en-pin = <&gpio_unused_gpio41>;
        };
};

[RETIMER]../ec_terms.md#retimer
[RETIMER Intel BB]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer?q=%22menuconfig%20PLATFORM_EC_USBC_RETIMER_INTEL_BB%22&ss=chromiumos
[RETIMER Intel BB runtime config]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer?q=%22menuconfig%20PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG%22&ss=chromiumos
[RETIMER ANX7451]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer?q=%22menuconfig%20PLATFORM_EC_USBC_RETIMER_ANX7451%22&ss=chromiumos
[RETIMER PS8811]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer?q=%22menuconfig%20PLATFORM_EC_USBC_RETIMER_PS8811%22&ss=chromiumos
[RETIMER PS8818]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer?q=%22menuconfig%20PLATFORM_EC_USBC_RETIMER_PS8818%22&ss=chromiumos
[RETIMER KB800X]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer?q=%22menuconfig%20PLATFORM_EC_USBC_RETIMER_KB800X%22&ss=chromiumos
[RETIMER KB800X CUSTOM XBAR]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer?q=%22menuconfig%20PLATFORM_EC_KB800X_CUSTOM_XBAR%22&ss=chromiumos
