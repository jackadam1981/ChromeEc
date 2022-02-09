# Zephyr EC Retimer Configuration

[TOC]

## Overview

<TODO> ADD USB MUX

[RETIMER] enables support for a retimer.

## Kconfig Options

The Kconfig option `CONFIG_PLATFORM_EC_USBC` enable the selection of a [RETIMER].
See the file [Kconfig.retimer] for all Kconfig options related to this feature.

## Devicetree Nodes

The `RETIMER` device tree nodes are defined in the [`DTS Bindings`] file for
each type of `RETIMER`.

## Board Specific Code

None required.

## Threads

RETIMER support does not enable any threads.

## Testing and Debugging

The [`I2C bus scan`] can be used to verify the RETIMER device can be accessed.

## Example

The Brya system uses the Intel BB retimer on USBC ports 0 and 1. The retimers are
I2C devices with address 0x56 and 0x57.

```
CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB=y
```

&i2c3_0 {
        status = "okay";
        clock-frequency = <I2C_BITRATE_STANDARD>;

        usb_c0_bb_retimer: jhl8040r@56 {
                compatible = "intel,jhl8040r";
                reg = <0x56>;
                label = "USB_C0_BB_RETIMER";
                int-pin = <&usb_c0_rt_int_odl>;
                reset-pin = <&usb_c0_rt_rst_odl>;
        };

        usb_c2_bb_retimer: jhl8040r@57 {
                compatible = "intel,jhl8040r";
                reg = <0x57>;
                label = "USB_C2_BB_RETIMER";
                int-pin = <&usb_c2_rt_int_odl>;
                reset-pin = <&usb_c2_rt_rst_odl>;
        };
};

[USB_MUX]:../ec_terms.md#usb_mux
[Kconfig.usb_mux]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usb_mux
[RETIMER]: ../ec_terms.md#retimer
[Kconfig.retimer]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.retimer
[`I2C bus scan`]: ./zephyr_i2c.md#Shell-Command_i2c
[`DTS Bindings`]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/retimer/
