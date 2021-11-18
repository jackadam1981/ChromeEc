# Zephyr I2C Bus Configuration

[TOC]

## Overview

The I2C busses provide access and control to on-board peripherals, including
USB-C chips, battery, charging IC, and sensors.

## Kconfig Options

Kconfig Option                     | Default state | Documentation
:--------------------------------- | :------------ | :------------
`CONFIG_PLATFORM_EC_I2C`           | Enabled       | [zephyr/Kconfig](../zephyr/Kconfig)

The following options are available only when `CONFIG_PLATFORM_EC_I2C=y`.

Kconfig sub-option                     | Default state | Documentation
:------------------------------------- | :------------ | :------------
`CONFIG_I2C_SHELL                      | Enabled | [CONFIG_I2C_SHELL]
`CONFIG_PLATFORM_EC_I2C_DEBUG`         | Disabled      | [zephyr/Kconfig](../zephyr/Kconfig)
`CONFIG_PLATFORM_EC_I2C_DEBUG_PASSTHRU`       | Disabled      | [zephyr/Kconfig](../zephyr/Kconfig)
`CONFIG_PLATFORM_EC_CONSOLE_CMD_I2C_PORTMAP`  | Disabled      | [zephyr/Kconfig](../zephyr/Kconfig)
`CONFIG_PLATFORM_EC_CONSOLE_CMD_I2C_SPEED`    | Disabled      | [zephyr/Kconfig](../zephyr/Kconfig)
`CONFIG_PLATFORM_EC_HOSTCMD_I2C_CONTROL`      | Disabled      | [zephyr/Kconfig](../zephyr/Kconfig)
`CONFIG_PLATFORM_EC_I2C_PASSTHRU_RESTRICTED`  | Disabled      | [zephyr/Kconfig](../zephyr/Kconfig)

*Note - Avoid documenting `CONFIG_` options in the markdown as the relevant
`Kconfig*` contains the authoritative definition.*

## Devicetree Nodes

The EC chip disables all I2C busses by default.  Enable the I2C busses used on your design by chanigng the chip-specific I2C port node status to `"okay"`.

### Nuvoton NPCX ECs

Nuvoton ECs use two devicetree nodes to describe the I2C busses used, an I2C controller and an I2C port. Mark the I2C port as enabled and specify the operating frequency of the bus.  You also enable the matching I2C controller as shown below.

```
&i2c_ctrl4 {
	status = "okay";
};
&i2c4_1 {
	status = "okay";
	clock-frequency = <I2C_BITRATE_FAST>;
};
```

The general form for the Nuvoton I2C node labels is:
- `&i2c_ctrl<controller>`
- `&i2c<controller>_<port>`

Where `<controller>` is the I2C controller number (0-7), and `<port>` is the port number (0-1). Not all I2C controllers support both ports, and each I2C controller can only be specified once.

### ITE IT8xxx2 ECs

ITE ECs use a single devicetree node, `&i2c<channel>` to enable an I2C bus. `<channel>` specifies the I2C/SMBus channel number (0-5).

```
&i2c3 {
	status = "okay";
	clock-frequency = <I2C_BITRATE_STANDARD>;
};
```

### Mapping legacy I2C port numbers to Zephyr devicetree nodes

The legacy I2C API for the Chromium EC application uses an enumeration (e.g. `I2C_PORT_ACCEL`, `I2C_PORT_EEPROM`) to specify the I2C bus during transfer operations.

The `named-i2c-ports` node creates the mapping between the legacy I2C bus enumeration and the Zephyr I2C bus device instance.

```
named-i2c-ports {
        compatible = "named-i2c-ports";
        battery {
                i2c-port = <&i2c0_0>;
                remote-port = <0>;
                enum-name = "I2C_PORT_BATTERY";
                label = "BATTERY";
        }
};
```

You can map multiple enumeration values to the same Zephyr I2C bus device instance.

```
named-i2c-ports {
        compatible = "named-i2c-ports";
        battery {
                i2c-port = <&i2c0_0>;
                remote-port = <0>;
                enum-name = "I2C_PORT_BATTERY";
                label = "BATTERY";
        }
	charger {
		i2c-port = <&i2c0_0>;
                remote-port = <0>;
		enum-name = "I2C_PORT_CHARGER";
	};
};
```

Refer to the [cros-ec-i2c-port-base.yaml] child-binding file for details about each property.

## Board Specific Code

None required.

## Threads

I2C support does not enable any threads.

## Testing and Debugging

The EC application enables the the Zephyr shell command, `i2c`, which includes the following subcommands:

Subcommand | Description | Usage
:--------- | :---------- | :----
`scan` | Scan I2C devices | `i2c scan <i2c_bus_label>`
`recover` | Recover I2C bus | `i2c recover <i2c_bus_label>`
`read` | Read bytes from an I2C device | `i2c read <i2c_bus_label> <dev_addr> <reg_addr> [<num_bytes>]`
`read_byte` | Read a byte from an I2C device | `i2c read_byte <i2c_bus_label> <dev_addr> <reg_addr>`
`write` | Write bytes to an I2C device | `i2c write <i2c_bus_label> <dev_addr> <reg_addr> <out_byte0> .. <out_byteN>`
`write_byte` | Write a byte to an I2C device | `i2c write_byte <i2c_bus_label> <dev_addr> <reg_addr> <out_byte>`

I2C parameter summary:
Parameter | Description
:-------- | :----------
`<i2c_bus_label>` | The I2C bus label property. By default this is specified by the EC vendor in the respective devicetree include file. Use the board devicetree overlay to override this name, if desired.
`<dev_addr>` | The I2C device address,

`I2C_4_PORT_1`

All subcommands take

## Example

*Optional - provide code snippets from a working board to walk the user through
all code that must be created to enable this feature.*

[CONFIG_I2C_SHELL]: https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_I2C_SHELL.html
[cros-ec-i2c-port-base.yaml]: ../zephyr/dts/bindings/i2c/cros-ec-i2c-port-base.yaml