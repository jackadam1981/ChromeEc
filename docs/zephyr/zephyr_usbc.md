# Zephyr EC USBC Configuration

[TOC]

## Overview

[USBC] enables the configuration of a Type-C port for a wide range of computing, display, and charging applications.

The image below shows a block diagram of a typical USBC setup.

[USBC Block Diagram]

## Kconfig Options

Kconfig Option                                                   | Default | Documentation
:--------------------------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_USBC`                                        | y       | [EC USBC]

The following options are available only when `CONFIG_PLATFORM_EC_USBC=y`.

Kconfig sub-option



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

[USBC]: ../ec_terms.md#usb-c
[USBC Block Diagram]: ../images/usbc_block_diagram.png
[EC USBC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usbc?q=%22menuconfig%20PLATFORM_EC_USBC%22&ss=chromiumos
