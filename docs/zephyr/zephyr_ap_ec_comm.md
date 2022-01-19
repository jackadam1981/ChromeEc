# Application Processor to EC communication - host commands

[TOC]

## Overview

Host commands allow communication between AP and EC.

This communication is used to inform the ChromeOS running on AP about events
like opening or closing lid, connecting or disconnecting charger,
thermal status, keyboard events.

Testing and debugging the host commands on DUT are described in [CrOS EC documentation]

## Kconfig options

Kconfig Option                           | Default     | Documentation
:--------------------------------------- | :---------: | :------------
`CONFIG_PLATFORM_EC_HOSTCMD`             | y if AP     | [EC host commands]

### Transport layer

The `CONFIG_PLATFORM_EC_HOST_INTERFACE_TYPE` choice selects the interface type
used to transport the AP/EC messages.

Kconfig to select interface              | Default     | Documentation
:--------------------------------------- | :---------: | :------------
`CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI` | y if AP_X86 | [eSPI interface]
`CONFIG_PLATFORM_EC_HOST_INTERFACE_HECI` | n           | [HECI interface]
`CONFIG_PLATFORM_EC_HOST_INTERFACE_LPC`  | n           | [LPC interface]
`CONFIG_PLATFORM_EC_HOST_INTERFACE_SHI`  | y if AP_ARM | [SHI interface]

#### SHI

[SHI] is acronym for SPI Host Interface. It's default interface for computers
with ARM AP.
It doesn't have any special Kconfigs.

#### eSPI - Virtual wires

[eSPI] is acronym for Enhanced Serial Peripheral Interface.
It allows to define signals as virtual ones instead of using normal GPIOs.

The following options are available only if eSPI was selected as protocol
because they select virtual wires, feature of this specific layer.

Kconfig for eSPI                                      | Default | Documentation
:---------------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_ESPI_VW_SLP_S3`                   | n       | [eSPI virtual wire S3]
`CONFIG_PLATFORM_EC_ESPI_VW_SLP_S4`                   | n       | [eSPI virtual wire S4]
`CONFIG_PLATFORM_EC_ESPI_VW_SLP_S5`                   | n       | [eSPI virtual wire S5]
`CONFIG_PLATFORM_EC_ESPI_RESET_SLP_SX_VW_ON_ESPI_RST` | n       | [eSPI virtual wire rst]

#### HECI

HECI is acronym for Host Embedded Controller Interface.
It doesn't have any special Kconfigs.

#### LPC

[LPC] is acronym for Low Pin Count bus.
It doesn't have any special Kconfigs.

### Generic configuration

The following options are generic and defines features available through
host interface.

Kconfig sub-options                          | Default     | Documentation
:------------------------------------------- | :---------: | :------------
`CONFIG_PLATFORM_EC_HOSTCMD_CONSOLE`         | y           | [Host command console]
`CONFIG_PLATFORM_EC_HOSTCMD_CONSOLE_BUF_SIZE`| 4096        | [Host command console buffer size]
`CONFIG_PLATFORM_EC_HOSTCMD_GET_UPTIME_INFO` | y           | [Host command - get uptime]
`CONFIG_PLATFORM_EC_HOSTCMD_REGULATOR`       | n           | [Host command - regulator]
`CONFIG_PLATFORM_EC_HOSTCMD_I2C_CONTROL`     | n           | [Host command - i2c control]
`CONFIG_PLATFORM_EC_HOST_COMMAND_STATUS`     | y if PLATFORM_EC_HOST_INTERFACE_SHI | [Host command - status]
`CONFIG_PLATFORM_EC_HOSTCMD_AP_RESET`        | n           | [Host command - AP reset]
`CONFIG_PLATFORM_EC_HOSTCMD_RTC`             | n           | [Host command - RTC]
`CONFIG_PLATFORM_EC_HOSTCMD_PD_CONTROL`      | y           | [Host command - PD control]

### MKBP - Matrix Keyboard Protocol

MKBP was originally used to send keyboard events to AP OS.
Later, more functionalities were added and more types of events can be sent
using this protocol. It can transfer information about keystrokes, sensors,
switches, fingerprints and more.

Kconfig sub-options                          | Default     | Documentation
:------------------------------------------- | :---------: | :------------
`CONFIG_PLATFORM_EC_MKBP_INPUT_DEVICES`      | n           | [MKBP input devices]
`CONFIG_PLATFORM_EC_MKBP_EVENT`              | n           | [MKBP event]

The following options must be enabled to use respective masks specified in
device tree. See [Device Tree nodes](#device-tree-nodes) paragraph for details.

Kconfig sub-options                             | Default     | Documentation
:---------------------------------------------- | :---------: | :------------
`CONFIG_PLATFORM_EC_MKBP_EVENT_WAKEUP_MASK`     | n           | [MKBP event wake-up mask]
`CONFIG_PLATFORM_EC_MKBP_HOST_EVENT_WAKEUP_MASK`| n           | [MKBP host event wake-up mask]

The following options are exclusive to each other as they select the method used
to inform the AP about MKBP events.

Kconfig sub-options                              | Default     | Documentation
:----------------------------------------------- | :---------: | :------------
`CONFIG_PLATFORM_EC_MKBP_USE_GPIO`               | y           | [MKBP gpio]
`CONFIG_PLATFORM_EC_MKBP_USE_HOST_EVENT`         | n           | [MKBP host event]
`CONFIG_PLATFORM_EC_MKBP_USE_GPIO_AND_HOST_EVENT`| n           | [MKBP gpio and host event]
`CONFIG_PLATFORM_EC_MKBP_USE_CUSTOM`             | n           | [MKBP custom]

### Debug

The following options are exclusive to each other as they select the verbosity
level of messages on the EC console.

Kconfig debug verbosity | Default | Documentation
:---------------------- | :-----: | :------------
`CONFIG_HCDEBUG_OFF`    | n       | [Debug off]
`CONFIG_HCDEBUG_NORMAL` | y       | [Debug normal]
`CONFIG_HCDEBUG_EVERY`  | n       | [Debug every]
`CONFIG_HCDEBUG_PARAMS` | n       | [Debug params]

## Device Tree nodes

### eSPI

Zephyr has built-in support for eSPI and EC takes advantage of this.
Boards supported by Zephyr should have definitions of eSPI interfaces in their
device trees.
The only thing required to select it is to make a phandle in the `chosen` node
with the name `cros-ec,espi`:

```
/ {
	chosen {
		cros-ec,espi = &espi0;
	}
}
```

### SHI

SHI driver uses first instance of node with compatible string as `*,*-cros-shi`.
For example, for nuvoton npcx, it will be `nuvoton,npcx-cros-shi`

This node's required properties are defined in yaml files: [SHI bindings]

```
/ {
	shi: shi@4000f000 {
		compatible = "nuvoton,npcx-cros-shi";
		reg = <0x4000f000 0x120>;
		interrupts = <18 1>;
		clocks = <&pcc NPCX_CLOCK_BUS_APB3 NPCX_PWDWN_CTL5 1>;
		pinctrl-0 = <&altc_shi_sl>;
		shi-cs-wui =<&wui_io53>;
		label = "SHI";
	};
}
```

### MKBP masks

Host commands can wake up the computer from sleep modes.
To not wake up the computer too much, only specified ones can wake.
The events able to wake up are defined as device tree nodes. One node for
MKBP events and one for generic host events.
They have respective Kconfig options, which must be enabled to take the
masks into account.

Both masks have to be compatible with binding file: [MKBP event mask yaml]

Possible enums to use in these nodes are specified in file: [MKBP event mask enums]

```
/ {
	ec-mkbp-host-event-wakeup-mask {
		compatible = "ec-wake-mask-event";
		wakeup-mask = <(HOST_EVENT_LID_OPEN |
				HOST_EVENT_POWER_BUTTON |
				HOST_EVENT_AC_CONNECTED |
				HOST_EVENT_AC_DISCONNECTED |
				HOST_EVENT_HANG_DETECT |
				HOST_EVENT_RTC |
				HOST_EVENT_MODE_CHANGE |
				HOST_EVENT_DEVICE)>;
	};

	ec-mkbp-event-wakeup-mask {
		compatible = "ec-wake-mask-event";
		wakeup-mask = <(MKBP_EVENT_KEY_MATRIX |
				MKBP_EVENT_HOST_EVENT |
				MKBP_EVENT_SENSOR_FIFO)>;
	};
}
```

## Board Specific Code

No board specific code is required.

## Threads

Handler of host commands is running on separate thread.

Enabling the `CONFIG_PLATFORM_EC_HOSTCMD` automatically selects
`CONFIG_HAS_TASK_HOSTCMD` which is responsible for creating the thread.

## Testing and Debugging

### From EC console

If debug level is higher than `CONFIG_HCDEBUG_OFF`, then HC messages can
be seen on EC console.

If there's no output lines starting from HC, that may be due to hostcmd channel
being masked.
To check what channels are masked, execute `chan` command in EC console.
```
uart:~$ chan
 # Mask     E Channel
...
 7 00000080 * hostcmd
```
The asterisk next to channel name means that it is enabled. Otherwise, execute
command `chan 128` to enable hostcmd channel while disabling others.

If the hostcmd channel is disabled by default, it may be enabled by removing
its name from `ec-console` node.

```
ec-console {
	compatible = "ec-console";

	disabled = "hostcmd";
};
```

Removing the `hostcmd` and leaving the empty quotes will result in hostcmd
messages visible on EC console since boot-up.

## Examples

[Lazor MKBP wake-up](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/projects/trogdor/lazor/gpio.dts?q=ec-mkbp-host-event-wakeup-mask)

[Lazor MKBP Kconfig](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/projects/trogdor/lazor/prj.conf?q=CONFIG_PLATFORM_EC_MKBP_EVENT_WAKEUP_MASK)

[NPCX eSPI selection](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/include/cros/nuvoton/npcx.dtsi?q=cros-ec,espi)

[NPCX SHI definition](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/include/cros/nuvoton/npcx.dtsi?q=shi)

<!--
Links to the documentation
-->
[CrOS EC documentation]:../ap-ec-comm.md#ectool
[SHI]:../ec_terms.md#shi
[eSPI]:../ec_terms.md#espi
[LPC]:../ec_terms.md#lpc
[EC host commands]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22menuconfig%20PLATFORM_EC_HOSTCMD%22
[eSPI interface]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.host_interface?q=%22config%20PLATFORM_EC_HOST_INTERFACE_ESPI%22
[HECI interface]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.host_interface?q=%22config%20PLATFORM_EC_HOST_INTERFACE_HECI%22
[LPC interface]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.host_interface?q=%22config%20PLATFORM_EC_HOST_INTERFACE_LPC%22
[SHI interface]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.host_interface?q=%22config%20PLATFORM_EC_HOST_INTERFACE_SHI%22

[eSPI virtual wire S3]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.espi?q=%22config%20PLATFORM_EC_ESPI_VW_SLP_S3%22
[eSPI virtual wire S4]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.espi?q=%22config%20PLATFORM_EC_ESPI_VW_SLP_S4%22
[eSPI virtual wire S5]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.espi?q=%22config%20PLATFORM_EC_ESPI_VW_SLP_S5%22
[eSPI virtual wire rst]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.espi?q=%22config%20PLATFORM_EC_ESPI_RESET_SLP_SX_VW_ON_ESPI_RST%22

[Host command console]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.console?q=%22menuconfig%20PLATFORM_EC_HOSTCMD_CONSOLE%22
[Host command console buffer size]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.console?q=%22config%20PLATFORM_EC_HOSTCMD_CONSOLE_BUF_SIZE%22
[Host command - get uptime]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_HOSTCMD_GET_UPTIME_INFO%22
[Host command - regulator]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_HOSTCMD_REGULATOR%22
[Host command - i2c control]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_HOSTCMD_I2C_CONTROL%22
[Host command - status]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_HOST_COMMAND_STATUS%22
[Host command - AP reset]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.powerseq?q=%22config%20PLATFORM_EC_HOSTCMD_AP_RESET%22
[Host command - RTC]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.rtc?q=%22config%20PLATFORM_EC_HOSTCMD_RTC%22
[Host command - PD control]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.usbc?q=%22config%20PLATFORM_EC_HOSTCMD_PD_CONTROL%22

[MKBP input devices]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_MKBP_INPUT_DEVICES%22
[MKBP event]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_MKBP_EVENT%22

[MKBP event wake-up mask]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_MKBP_EVENT_WAKEUP_MASK%22
[MKBP host event wake-up mask]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_MKBP_HOST_EVENT_WAKEUP_MASK%22

[MKBP gpio]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.mkbp_event?q=%22config%20PLATFORM_EC_MKBP_USE_GPIO%22
[MKBP host event]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.mkbp_event?q=%22config%20PLATFORM_EC_MKBP_USE_HOST_EVENT%22
[MKBP gpio and host event]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.mkbp_event?q=%22config%20PLATFORM_EC_MKBP_USE_GPIO_AND_HOST_EVENT%22
[MKBP custom]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.mkbp_event?q=%22config%20PLATFORM_EC_MKBP_USE_CUSTOM%22

[Debug off]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20HCDEBUG_OFF%22
[Debug normal]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20HCDEBUG_NORMAL%22
[Debug every]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20HCDEBUG_EVERY%22
[Debug params]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20HCDEBUG_PARAMS%22

[SHI bindings]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/cros_shi/
[MKBP event mask yaml]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/cros_mkbp_event/ec-mkbp-event.yaml
[MKBP event mask enums]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/include/dt-bindings/wake_mask_event_defines.h
