# Zephyr EC GPIO Configuration

[TOC]

## Overview

GPIO provide support for general purpose I/Os on the SoC, including
initialization, setting and interrupt management.

## Kconfig Options

Kconfig Option                         | Default | Documentation
:------------------------------------- | :-----: | :------------
`PLATFORM_EC_GPIO_INIT_PRIORITY`       | 51      | [GPIO Init Priority]

*No sub-options available.*

## Devicetree Nodes

The GPIO module is configured by enumerating all the GPIOs in a devicetree node
declared as `compatible = "named-gpios"`. All GPIOs listed there are
automatically initialized and can be referred to using the specified `enum-name`.


Named GPIO properties:

Property | Description | Settings
:------- | :---------- | :-------
`#gpio-cells` | Specifier cell count, always `<0>`, required if the node label is used in a `-gpios` property. | `<0>`
`gpios` | GPIO phandle, identifies the port, pin number and flags. | `<&gpioX Y GPIO_FLAGS>`
`enum-name` | The enum used to refer to the GPIO in the code. | `GPIO_<NAME>`

The list of valid `enum-name` values is defined in [gpio-enum-name.yaml].

In the GPIO declaration it is conventional use the net name used in the
schematic as the *node name*, and the same prefixed with `gpio_` as *node label*. For example:

```
named-gpios {
        compatible = "named-gpios";
...
        gpio_en_pp5000_fan: en_pp5000_fan {
                gpios = <&gpio6 1 GPIO_OUT_LOW>;
                enum-name = "GPIO_EN_PP5000_FAN";
        };
...
}

```

The `flags` cell of the `gpios` property defines how the GPIO is initialized,
valid options are listed in [dt-bindings/gpio_defines.h], which is normally included from the main project DTS file.

## Board Specific Code

None required.

## Threads

GPIO support does not enable any threads.

## Testing and Debugging

### Shell Commands

The EC application defines two different shell commands to read and change the state of a GPIO:

Command | Description | Usage
:------ | :---------- | :----
`gpioget` | Read the current state of a GPIO | `gpioget [name]`
`gpioset` | Change the state of a GPIO | `gpioset name <value>`

GPIO parameter summary:

Parameter | Description
:-------- | :----------
`name` | The GPIO node name as defined in the devicetree.
`value` | The requested state, `0` or `1`.

## Example

The image below shows a GPIO assignment on the Volteer reference board.

![GPIO Example]

Net Name | Port | Pin | Flags
:------- | :--- | :-- | :----
EC_ENTERING_RW | GPIOE | 3 | Output, init low

```
named-gpios {
        compatible = "named-gpios";
...
        gpio_ec_entering_rw: ec_entering_rw {
                #gpio-cells = <0>;
                gpios = <&gpioe 3 GPIO_OUT_LOW>;
                enum-name = "GPIO_ENTERING_RW";
        };
...
}
```

The GPIO can then be referred from in the code with the `enum-name` directly, or through other DTS nodes by using the `&gpio_ec_entering_rw` node label, for example:

```
cbi_eeprom: eeprom@50 {
        compatible = "atmel,at24";
        reg = <0x50>;
        wp-gpios = <&gpio_ec_wp_l>;
};
```

[GPIO Example]: ../images/gpio_example.png
[GPIO Init Priority]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.init_priority?q=%22config%20PLATFORM_EC_GPIO_INIT_PRIORITY%22&ss=chromiumos
[gpio-enum-name.yaml]: ../../zephyr/dts/bindings/gpio/gpio-enum-name.yaml
[dt-bindings/gpio_defines.h]: ../../zephyr/include/dt-bindings/gpio_defines.h
