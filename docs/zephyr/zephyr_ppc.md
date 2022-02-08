# Zephyr EC PPC Configuration

[TOC]

## Overview

Enable support for a [USB-C] [PPC].

## Kconfig Options

Kconfig Option                | Default | Documentation
:-----------------------------| :-----: | :------------
`CONFIG_PLATFORM_EC_USBC_PPC` | y       | [USBC PPC]

The following options are available only when `CONFIG_PLATFORM_EC_USBC_PPC=y`.

Kconfig sub-option                                    | Default | Documentation
:---------------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USBC_PPC_POLARITY`                | n       | [PPC POLARITY]
`CONFIG_PLATFORM_EC_USBC_PPC_SBU`                     | n       | [PPC SBU]
`CONFIG_PLATFORM_EC_USBC_PPC_VCONN`                   | n       | [PPC VCONN]
`CONFIG_PLATFORM_EC_USBC_PPC_AOZ1380`                 | n       | [PPC AOZ1380]
`CONFIG_PLATFORM_EC_USBC_PPC_RT1718S`                 | n       | [PPC RT1718S]
`CONFIG_PLATFORM_EC_USBC_PPC_KTU1125`                 | n       | [PPC KTU1125]
`CONFIG_PLATFORM_EC_USBC_PPC_NX20P3483`               | n       | [PPC NX20P3483]
`CONFIG_PLATFORM_EC_USBC_PPC_SN5S330`                 | n       | [PPC SN5S330]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X`                 | n       | [PPC SYV682X]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682C`                 | n       | [PPC SYV682C]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X_HV_ILIM`         | n       | [PPC SYV682X HV ILIM]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X_NO_CC`           | n       | [PPC SYV682X NO CC]
`CONFIG_PLATFORM_EC_USBC_PPC_SYV682X_SMART_DISCHARGE` | n       | [PPC SYV682X SMART DISCHARGE]
`CONFIG_PLATFORM_EC_USBC_PPC_DEDICATED_INT`           | n       | [PPC DEDICATED INT]

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

[USB-C]: ../ec_terms.md#usb-c
[PPC]: ../ec_terms.md#ppc
[PPC POLARITY]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_POLARITY%22&ss=chromiumos
[PPC SBU]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_SBU%22&ss=chromiumos
[PPC VCONN]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_VCONN%22&ss=chromiumos
[PPC AOZ1380]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_AOZ1380%22&ss=chromiumos
[PPC RT1718S]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_RT1718S%22&ss=chromiumos
[PPC KTU1125]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_KTU1125%22&ss=chromiumos
[PPC NX20P3483]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_NX20P3483%22&ss=chromiumos
[PPC SN5S330]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_SN5S330%22&ss=chromiumos
[PPC SYV682X]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_SYV682X%22&ss=chromiumos
[PPC SYV682C]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_SYV682C%22&ss=chromiumos
[PPC SYV682X HV ILIM]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_SYV682X_HV_ILIM%22&ss=chromiumos
[PPC SYV682X NO CC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_SYV682X_NO_CC%22&ss=chromiumos
[PPC SYV682X SMART DISCHARGE]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_SYV682X_SMART_DISCHARGE%22&ss=chromiumos
[PPC DEDICATED INT]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.ppc?q=%22menuconfig%20PLATFORM_EC_USBC_PPC_DEDICATED_INT%22&ss=chromiumos
