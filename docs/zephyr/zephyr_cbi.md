# Zephyr CrOS Board Information (CBI) Configuration

[TOC]

## Overview

CrOS Board Info [`CBI`] is used to store static board information,
such as SKU and configuration information. With [`CBI`], the information is
stored in a writable [`EEPROM`] chip, so we can provision the correct SKU
at RMA time.

[`CBI`] includes two different pieces of firmware relevant configuration
information that are programmed during manufacturing.

1) The Firmware Configuration [`FW_CONFIG`] stores information
specifically for the firmware, such as whether the device has a backlit
keyboard.  One can view [`FW_CONFIG`] as the firmware characteristic of a
SKU, so a SKU only maps to a single [`FW_CONFIG`], but different SKUs can
map to the same [`FW_CONFIG`].
2) The Second Source Factory Cache [`SSFC`] also stores information about
the device for the firmware to read. The [`SSFC`] describes later decisions
for a board to indicate alternate second sourced hardware stuffing which
can be used by the EC to know which drivers to load.

The difference between [`SSFC`] and [`FW_CONFIG`] is that [`SSFC`] doesn’t
affect SKU. This prevents SKU explosion when a device has many second
source components.

If a Second Source Component is probeable, this should be stored in
[`SSFC`], which avoids creating a new SKU.  If it is not probeable,
it must be added to [`FW_CONFIG`].

If your board includes an [`EEPROM`] to store CrOS Board Info, then this
feature must be enabled and configured.

## Kconfig Options

Kconfig Option                  | Default state | Documentation
:------------------------------ | :-----------: | :------------
`CONFIG_PLATFORM_EC_CBI_EEPROM` | n             | [`zephyr/Kconfig`]

## Testing and Debugging

The [`ectool cbi`] command can be run from the kernel to get/set [`FW_CONFIG`]
and [`SSFC`] values.  The console has a "cbi" command that can be used to do
the same thing from the EC console.


[`CBI`]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/design_docs/cros_board_info.md
[`ectool cbi`]: ./zephyr_cbi.md#testing-and-debugging
[`EEPROM`]: ./zephyr_eeprom.md
[`FW_CONFIG`]: ./zephyr_fw_config.md
[`SSFC`]: ./zephyr_ssfc.md
[`zephyr/Kconfig`]: ../../zephyr/Kconfig
