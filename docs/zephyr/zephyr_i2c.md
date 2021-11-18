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

Kconfig sub-option                            | Default | Documentation
:-------------------------------------------- | :-----: | :------------
`CONFIG_I2C_SHELL`                            | y       | [CONFIG_I2C_SHELL]
`CONFIG_PLATFORM_EC_I2C_DEBUG`                | n       | [I2C Debug]
`CONFIG_PLATFORM_EC_I2C_DEBUG_PASSTHRU`       | n       | [I2C Debug Passthru]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_I2C_PORTMAP`  | n       | [I2C Portmap]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_I2C_SPEED`    | n       | [I2C Speed]
`CONFIG_PLATFORM_EC_HOSTCMD_I2C_CONTROL`      | n       | [I2C Control]
`CONFIG_PLATFORM_EC_I2C_PASSTHRU_RESTRICTED`  | n       | [I2C Passthru Restricted]

## Devicetree Nodes

The EC chip disables all I2C busses by default.  Enable the I2C busses used on
your design by chanigng the chip-specific I2C bus `status` property to `"okay"`.

I2C bus properties: Property | Description | Settings
:------- | :---------- | :-------
`status` | Enables or disables the I2C controller | `"okay"` <br> `"disabled"`
`label` | Override the EC chip specific label. We recommend changing the label to match the net name of the I2C bus. The label must begin with `"I2C_"`. |`"I2C_<net_name>"`
`clock-frequency` | Sets the initial I2C bus frequency in Hz. | `I2C_BITRATE_STANDARD` - 100 KHz <br> `I2C_BITRATE_FAST` - 400 KHz <br> `I2C_BITRATE_FAST_PLUS` - 1 MHz

Example enabling I2C0 and I2C3 at 100 KHz and 1 MHz, respectively.
```
&i2c0 {
        status = "okay";
        label = "I2C_BATTERY";
        clock-frequency = <I2C_BITRATE_STANDARD>;
};
&i2c3 {
        status = "okay";
        label = "I2C_USB_C0_PD";
        clock-frequency = <I2C_BITRATE_FAST_PLUS>;
};
```

### Nuvoton NPCX ECs

Nuvoton ECs use two devicetree nodes to describe the I2C busses used, an I2C
controller and an I2C port.

Nuvoton I2C node labels use the following pattern:
- I2C controller: `&i2c_ctrl<controller>`
- I2C port: `&i2c<controller>_<port>`

Where `<controller>` specifies the I2C controller number (0-7), and `<port>`
specifies the port number (0-1). Not all I2C controllers support both ports, and
each I2C controller can only be specified once.

The Nuvoton I2C port contains the standard Zephyr I2C bus properties. The
Nuvoton I2C controller contains only the `status` property.

To enable a Nuvoton I2C bus, set both the I2C controller and I2C port `status`
property to `"okay"`.Set the `clock-frequency` and `label` properties in the I2C
port as shown below:

```
&i2c_ctrl4 {
        status = "okay";
};
&i2c4_1 {
        status = "okay";
        label = "I2C_EEPROM";
        clock-frequency = <I2C_BITRATE_FAST>;
};
```

### ITE IT8xxx2 ECs

ITE ECs use a single devicetree node, `&i2c<channel>` to enable an I2C bus.
`<channel>` specifies the I2C/SMBus channel number (0-5).

```
&i2c3 {
        status = "okay";
        label = "I2C_USB_C0_PD";
        clock-frequency = <I2C_BITRATE_STANDARD>;
};
```

### Mapping legacy I2C port numbers to Zephyr devicetree nodes

The legacy I2C API for the Chromium EC application uses an enumeration (e.g.
`I2C_PORT_ACCEL`, `I2C_PORT_EEPROM`) to specify the I2C bus during transfer
operations.

The `named-i2c-ports` node creates the mapping between the legacy I2C bus
enumeration and the Zephyr I2C bus device instance.

```
named-i2c-ports {
        compatible = "named-i2c-ports";
        battery {
                i2c-port = <&i2c0_0>;
                remote-port = <0>;
                enum-name = "I2C_PORT_BATTERY";
        }
};
```

You can map multiple enumeration values to the same Zephyr I2C bus device
instance.

```
named-i2c-ports {
        compatible = "named-i2c-ports";
        battery {
                i2c-port = <&i2c0_0>;
                remote-port = <0>;
                enum-name = "I2C_PORT_BATTERY";
        }
        charger {
                i2c-port = <&i2c0_0>;
                remote-port = <0>;
                enum-name = "I2C_PORT_CHARGER";
        };
};
```

Refer to the [cros-ec-i2c-port-base.yaml] child-binding file for details about
each property.

## Board Specific Code

None required.

## Threads

I2C support does not enable any threads.

## Testing and Debugging

### `i2c` shell command
The EC application enables the the Zephyr shell command, `i2c`, which includes
the following subcommands:

Subcommand | Description | Usage
:--------- | :---------- | :----
`scan` | Scan I2C devices | `i2c scan <i2c_bus_label>`
`recover` | Recover I2C bus | `i2c recover <i2c_bus_label>`
`read` | Read bytes from an I2C device | `i2c read <i2c_bus_label> <dev_addr> <reg_addr> [<num_bytes>]`
`read_byte` | Read a byte from an I2C device | `i2c read_byte <i2c_bus_label> <dev_addr> <reg_addr>`
`write` | Write bytes to an I2C device | `i2c write <i2c_bus_label> <dev_addr> <reg_addr> <out_byte0> .. <out_byteN>`
`write_byte` | Write a byte to an I2C device | `i2c write_byte <i2c_bus_label> <dev_addr> <reg_addr> <out_byte>`

I2C parameter summary: Parameter | Description
:-------- | :----------
`<i2c_bus_label>` | The I2C bus label property. By default this is specified by the EC vendor in the respective devicetree include file unless you override the label in your devicetree.
`<dev_addr>` | The I2C device address, specified using 7-bit notation. Valid device addresses are 0 - 0x7F.
`<reg_addr>` | The register address with the I2C device to read or write.
`<num_bytes>` | For the `read` subcommand, specifies the number of bytes to read from the I2C device. Default is 16 bytes if not specified.
`<out_byte>` | For the `write_byte` subcommand, specifies the single data byte to write to the I2C device.
`<out_byte0>..<out_byteN>` | For the `write` subcommand, specifies the data bytes to write to the I2C device.

### `i2c_portmap` shell command
The shell command `i2c_portmap` displays the mapping of I2C bus enumeration to
the physical bus and to the remote port index.

**TODO** - Show example output.

## Example

*Optional - provide code snippets from a working board to walk the user through
all code that must be created to enable this feature.*

[CONFIG_I2C_SHELL]:
https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_I2C_SHELL.html
[I2C Debug]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig;l=464?q=config%20PLATFORM_EC_I2C_DEBUG&sq=&ss=chromiumos
[I2C Debug Passthru]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig;l=477?q=config%20PLATFORM_EC_I2C_DEBUG_PASSTHRU&ss=chromiumos
[I2C Portmap]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig;l=485?q=config%20PLATFORM_EC_CONSOLE_CMD_I2C_PORTMAP&ss=chromiumos
[I2C Speed]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_CONSOLE_CMD_I2C_SPEED%22&ss=chromiumos
[I2C Control]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_HOSTCMD_I2C_CONTROL%22&ss=chromiumos
[I2C Passthru Restricted]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_I2C_PASSTHRU_RESTRICTED%22&ss=chromiumos
[cros-ec-i2c-port-base.yaml]:
../../zephyr/dts/bindings/i2c/cros-ec-i2c-port-base.yaml