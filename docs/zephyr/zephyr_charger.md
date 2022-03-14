# Zephyr EC Charger

[TOC]

## Overview

The charger chip enables an external power supply to provide power to the
board's components.

## Kconfig Options

`CONFIG_PLATFORM_EC_CHARGER` enables charging support in the EC
application. Refer to [Kconfig.charger] for all sub-options controlling charging
behavior.

Note: At least one charger IC must be enabled.

Note: The charger chip configuration serves a different role than the USB
charging configuration found in [Kconfig.usb_charger].

### Example of enabled configs

In `ec/zephyr/projects/{project}/{board}/prj.conf`, one may add:

```
# Charger
CONFIG_PLATFORM_EC_CHARGER=y
# Charger sub-options
CONFIG_PLATFORM_EC_CHARGE_RAMP_HW=y
CONFIG_PLATFORM_EC_CHARGER_DISCHARGE_ON_AC=y
CONFIG_PLATFORM_EC_CHARGER_DISCHARGE_ON_AC_CHARGER=y
CONFIG_PLATFORM_EC_CHARGER_MIN_BAT_PCT_FOR_POWER_ON=2
CONFIG_PLATFORM_EC_CHARGER_MIN_POWER_MW_FOR_POWER_ON=10000
CONFIG_PLATFORM_EC_CHARGER_PROFILE_OVERRIDE=y
CONFIG_PLATFORM_EC_CHARGER_PSYS=y
CONFIG_PLATFORM_EC_CHARGER_PSYS_READ=y
CONFIG_PLATFORM_EC_CHARGER_SENSE_RESISTOR=10
CONFIG_PLATFORM_EC_CHARGER_SENSE_RESISTOR_AC=20
CONFIG_PLATFORM_EC_CONSOLE_CMD_CHARGER_ADC_AMON_BMON=y
```

## Devicetree Nodes
### How to add a charger devicetree node

#### Add charger node as child of `named-i2c-port`

Example:

```
named-i2c-ports {
   	compatible = "named-i2c-ports";
   	charger {
   		i2c-port = {ASSOCIATED PHANDLE};
		/* Could be any name, but must correlate to "Board Specific Code" */
   		enum-name = "I2C_PORT_CHARGER";
   	};
};
```

See the I2C doc on [mapping legacy I2C port numbers to Zephyr devicetree nodes]
for more information on configuring the `named-i2c-port` node.

### How to create a new charger

Add `vendor,part.yaml` to the [charger bindings directory]:

Example Template:

```
description: Vendor Part Charger IC

compatible: "vendor,part"

include: i2c-device.yaml

```
See the [I2C doc](./zephyr_i2c.md) for more information on configuring I2C
device bindings.

## Board Specific Code

### Define or append to the charger\_config\_t global array

Example configuring an ISL923x charger chip:

```c
const struct charger_config_t chg_chips[] = {
	{
		/* .i2c_port must match corresponding named-i2c-port child */
		.i2c_port = I2C_PORT_CHARGER,

		/* these may vary by vendor and part */
		.i2c_addr_flags = ISL923X_ADDR_FLAGS,
		.drv = &isl923x_drv,
	},
};
```

<!-- TODO(b/228237412) - charger chips should be defined in code via DT
macros. -->

## Threads

Enabling `CONFIG_PLATFORM_EC_CHARGER` also enables the [charger thread]
described by [Kconfig.tasks].

## Testing and Debugging a Flashed Board

### EC Console Commands

The `charger` [EC console command] may be invoked to inspect the the chip's
details and status.

```
> charger

charger
  Name:   ISL9241
  Option: 10100000110000000000000100 (0x2830004)
  Man id: 0x0049
  Dev id: 0x000e
  V_batt: 13152 (  64 - 18304,   8)
  I_batt:  2364 (   4 -  6140,   4)
  I_in:    3000 (   4 -  6140,   4)
  I_dptf: disabled
```

The `taskinfo` [EC console command] may be invoked to inspect if the charging
task was enabled.

```
> taskinfo

 Task Ready Name         Events      Time (s)  StkUsed
 0 R << idle >>       00000000 2055.869084   80/672
 1   HOOKS            00000000    5.215864  560/800
 2   CHG_RAMP         00000000    0.108705  424/672
 3   USB_CHG_P0       00000000    0.002139  368/672
 4   USB_CHG_P1       00000000    0.002132  368/672
 5   CHARGER          00000000   17.692626  488/928
 6 R MOTIONSENSE      80000002   50.203370  632/928
 7   KEYPROTO         00000000    0.008531  312/672
 8   CHIPSET          00000000    0.026394  528/800
 9   HOSTCMD          00000000    1.483327  600/800
10 R CONSOLE          00000000    0.101999  448/928
11   POWERBTN         00000000    0.001535  464/800
12   KEYSCAN          00000000    1.144058  328/672
13   PD_C0            00000000  176.510938  632/928
14   PD_C1            00000000   73.909944  624/928
15   PD_INT_C0        00000000    0.003969  472/672
16   PD_INT_C1        00000000    0.025180  512/672
```
#### if PLATFORM_EC_CHARGE_MANAGER is enabled

TODO: provided by charge_state_v2
pwr_avg
chgstate
chgdualdebug

TODO: provided by charge_manager
chgoverride
chglim
chgsup


### ECTool Commands (Googlers only) - TODO

Googlers can also use [ectool](http://go/platforms-ec-doc).

TODO:
chargecurrentlimit
chargecontrol
chargeoverride
chargestate


<!-- Reference Links -->
[EC console command]: https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/README.md#useful-ec-console-commands
[Kconfig.charger]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.charger
[Kconfig.tasks]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.tasks?q=%22config%20HAS_TASK_CHARGER%22&ss=chromiumos
[Kconfig.usb_charger]: https://crsrc.org/o/src/platform/ec/zephyr/Kconfig.usb_charger?q=%22config%20PLATFORM_EC_USB_CHARGER%22&ss=chromiumos
[charger bindings directory]: https://crsrc.org/o/src/platform/ec/zephyr/dts/bindings/charger/
[charger thread]: https://crsrc.org/o/src/platform/ec/common/charge_state_v2.c?q=%22void%20charger_task%22&ss=chromiumos
[mapping legacy I2C port numbers to Zephyr devicetree nodes]: ./zephyr_i2c.md#mapping-legacy-i2c-port-numbers-to-zephyr-devicetree-nodes
