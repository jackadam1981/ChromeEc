# Zephyr CBI EEPROM Configuration.

[TOC]

## Overview

Zephyr EEPROM [CBI Configuration]

Note: Since the `EEPROM` uses an I2C interface, the [I2C buses] must be
configured and working before enabling CBI.

## Kconfig Options

The CrOS Board Information [CBI] feature is enabled with the
CONFIG_PLATFORM_EC_CBI_EEPROM Kconfig, as defined in the [CBI Configuration].

The specific `EEPROM` also needs to be enabled. For example, the Atmel AT24
would need the following enabled.

Kconfig Option                  | Enabled state | Documentation
:------------------------------ | :-----------: | :------------
`CONFIG_EEPROM`                 | y             | Enabled EEPROM
`CONFIG_EEPROM_AT24`            | y             | Enable Atmel AT24

Device tree is used to define and specify the `EEPROM` device.

## Devicetree Nodes

The `EEPROM` device tree nodes are defined for each type of device
YAML bindings that are specific to that particular `EEPROM`.  The standard
fashion of defining that `EEPROM` is used with one exception, the `EEPROM`
node must have the label `cbi_eeprom`.

An example definition of the Atmel AT24 is:
```
    &i2c0_0 {
        label = "I2C_EEPROM";
        clock-frequency = <I2C_BITRATE_FAST>;

        cbi_eeprom: eeprom@50 {
            compatible = "atmel,at24";
            reg = <0x50>;
            label = "EEPROM_CBI";
            size = <2048>;
            pagesize = <16>;
            address-width = <8>;
            timeout = <5>;
        };
    };
```

## Threads

No threads used in this feature.

## Testing and Debugging

There are unit tests.


[CBI]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/design_docs/cros_board_info.md
[CBI Configuration]: ./zephyr_cbi.md
[I2C buses]: ./zephyr_i2c.md
