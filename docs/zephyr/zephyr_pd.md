# Zephyr EC PD Configuration

[TOC]

## Overview

Configure [USB-C] [PD] features that enables a port to provide power
greater than the basic 5V @ 900mA. Power can be negotiated up to
20V @ 5A.

## Kconfig Options

The `CONFIG_PLATFORM_EC_USB_POWER_DELIVERY` option enables USB-C power delivery
support on the Chromebook. See [Kconfig.pd] for sub-options related to this feature.

### Shared PD Interrupts

Enable the `CONFIG_PLATFORM_EC_USB_PD_INT_SHARED` if multiple USB-C ports share a single
interrupt signal on the EC. See [Kconfig.pd_int_shared] for details about all sub-options.

A platform that shares an interrupt signal between USB-C ports 0 and 2 includes these
configuration settings:

```
CONFIG_PLATFORM_EC_USB_PD_INT_SHARED=y
CONFIG_PLATFORM_EC_USB_PD_PORT_0_SHARED=y
CONFIG_PLATFORM_EC_USB_PD_PORT_2_SHARED=y
```
### Measuring VBUS Voltage

See [Kconfig.pd_meas_vbus] for options that enable several ways to measure [VBUS]

### Detecting VBUS Voltage

See [Kconfig.pd_vbus_detection] for options that enable several ways to detect [VBUS]

### VBUS Discharge

See [Kconfig.pd_discharge] for options that enable several ways to discharge [VBUS]

### Fast Role Swap

The `CONFIG_PLATFORM_EC_USB_PD_FRS` option enables USB-C power delivery [FRS] support.
See [Kconfig.pd_frs] for details about all sub-options.

### Console Commands

The `CONFIG_PLATFORM_EC_USB_PD_CONSOLE_CMD` option enables various USB-C PD related
console commands. See [Kconfig.pd_console_cmd] for details about all sub-options.

### USBC Device Type

See [Kconfig.pd_usbc_device_type] for options on the available USB-C device types.

[USB-C]:../ec_terms.md#usb-c
[PD]:../ec_terms.md#pd
[VBUS]:../ec_terms.md#vbus
[FRS]:../ec_terms.md#frs
[Kconfig.pd]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd
[Kconfig.pd_int_shared]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_int_shared
[Kconfig.pd_meas_vbus]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_meas_vbus
[Kconfig.pd_frs]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_frs
[Kconfig.pd_discharge]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_discharge
[Kconfig.pd_vbus_detection]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_vbus_detection
[Kconfig.pd_console_cmd]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_console_cmd
[Kconfig.pd_usbc_device_type]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.pd_usbc_device_type
