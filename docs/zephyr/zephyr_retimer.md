# Zephyr EC Retimer Configuration

[TOC]

## Overview

[RETIMER] enables support for a retimer.

## Kconfig Options

[Kconfig.retimer]

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
[Kconfig.retimer]
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer
