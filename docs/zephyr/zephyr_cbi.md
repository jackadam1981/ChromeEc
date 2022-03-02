# Zephyr CrOS Board Information (CBI) Configuration

[TOC]

## Overview

The CrOS Board Information [CBI] contains a variety of fields that the EC
retrieves from a [Config_EEPROM].

If your board includes an EEPROM to store CrOS Board Information, then this
feature must be enabled and configured.

CBI includes two different pieces of firmware relevant configuration
information.
1) The Firmware Configuration [FW_CONFIG] describes board options for a
given project which can be used by the Embedded Controller (EC) and the
Application Processor (AP).
2) The Second Source Factory Cache [SSFC] describes later decisions
for a board to indicate alternate second sourced hardware stuffing
which can be used by the EC to know which drivers to load.

The values in these CBI fields are programmed during manufacturing.

Note that the [I2C buses] must be configured and working before enabling CBI.

## Kconfig Options

Kconfig Option                  | Default state | Documentation
:------------------------------ | :-----------: | :------------
`CONFIG_PLATFORM_EC_CBI_EEPROM` | n             | [zephyr/Kconfig]


[CBI]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/design_docs/cros_board_info.md
[Config_EEPROM]: ./zephyr/zephry_eeprom.md
[FW_CONFIG]: ./zephyr/zephyr_fw_config.md
[I2C buses]: ./zephyr_i2c.md
[SSFC]: ./zephyr/zephyr_ssfc.md
[zephyr/Kconfig]: ../zephyr/Kconfig
