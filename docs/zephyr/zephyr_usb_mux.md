# Zephyr EC USB MUX Configuration

[TOC]

## Overview

Enable support for [USB_MUX].

## Kconfig Options

The Kconfig option CONFIG_PLATFORM_EC_USB_MUX enables the selction of a [USB_MUX].
See the file [Kconfig.usb_mux] for all Kconfig options related to this feature.

## Devicetree Nodes

TBD

## Board Specific Code

TBD

## Threads

When Kconfig option `CONFIG_PLATFORM_EC_USB_MUX_TASK=y`, a dedicated thread is run that
processes USB mux sets and HPD.

## Testing and Debugging

TBD

## Example

TBD

[USB_MUX]:../ec_terms.md#usb_mux
[Kconfig.usb_mux]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usb_mux
