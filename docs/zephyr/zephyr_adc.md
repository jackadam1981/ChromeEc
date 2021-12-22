# Zephyr ADC Configuration

[TOC]

## Overview

ADC is used to measure VBUS, temperature and other values depending on a board.

## Kconfig Options

Kconfig Option                     | Default state | Documentation
:--------------------------------- | :------------ | :------------
`CONFIG_PLATFORM_EC_ADC`           | Enabled       | [EC ADC]

The following options are available only when `CONFIG_PLATFORM_EC_ADC=y`.

Kconfig sub-option                               | Default | Documentation
:----------------------------------------------- | :-----: | :------------
`CONFIG_ADC_SHELL`                               | n       | [CONFIG_ADC_SHELL]
`CONFIG_PLATFORM_EC_ADC_CMD`                     | y       | [ADC cmd]
`CONFIG_PLATFORM_EC_ADC_RESOLUTION`              | 10      | [ADC resolution]
`CONFIG_PLATFORM_EC_ADC_OVERSAMPLING`            | 0       | [ADC oversampling]
`CONFIG_PLATFORM_EC_ADC_CHANNELS_RUNTIME_CONFIG` | n       | [ADC runtime config]

## Devicetree Nodes

The EC chip disables all Analog-to-Digital Converters by default.  Enable ADC used on
your design by changing the chip-specific ADC `status` property to `"okay"`.

ADC properties:

Property | Description | Settings
:------- | :---------- | :-------
`status` | Enables or disables the ADC module | `"okay"` <br> `"disabled"`
`label` | Override the EC chip specific label. |`"ADC_<number>"`

Either Nuvoton NPCX and ITE IT8xxx2 ECs use single devicetree node `adc0` to describe ADC.
To enable the ADC set the `status` property to `"okay"`:
```
&adc0 {
	status = "okay";
};
```

### Mapping legacy ADC enums to ADC channels

The legacy ADC API for the Chromium EC application uses an enumeration (e.g.
`ADC_VBUS`, `ADC_AMON_BMON`) to specify the ADC channel to measure voltage.

The `named-adc-channels` node creates the mapping between the legacy ADC channels
enumeration and the Zephyr ADC channels.
```
named-adc-channels {
	compatible = "named-adc-channels";
	vbus {
		label = "VBUS";
		enum-name = "ADC_VBUS";
		channel = <1>;
		/* Measure VBUS through a 1/10 voltage divider */
		mul = <10>;
	};
};
```

Refer to the [named-adc.yaml] child-binding file for details about
each property.

## Board Specific Code

None required.

## Threads

ADC support does not enable any threads.

## Testing and Debugging

Unfortunetly, the are two `adc` console commands: a [Zephyr](Zephyr command) command and
the one implemented in [CrosEC](CrosEC command).
Only one of them can be enabled with `CONFIG_ADC_SHELL` or `CONFIG_PLATFORM_EC_ADC_CMD`
respectevly.

### Zephyr command
The Zephyr `adc` includes the following subcommands:

Subcommand | Description | Usage
:--------- | :---------- | :----
`acq_time` | Configure acquisition time | `adc <adc_label> acq_time <time> <unit>`
`channel` | Configure ADC channel | `adc <adc_label> channel id <channel_id>`
`gain` | Configure gain | `adc <adc_label> gain <gain>`
`print` | Print current configuration | `adc <adc_label> print`
`read` | Read adc value | `adc <adc_label> read <channel>`
`reference` | Configure reference | `adc <adc_label> reference <reference>`
`resolution` |Configure resolution for the `read` subcommand | `adc <adc_label> resolution <resolution>`

ADC parameter summary: Parameter | Description
:-------- | :----------
`<adc_label>` | The ADC label property. By default this is specified by the EC vendor in the respective devicetree include file unless you override the label in your devicetree.
`<time>` | For the `acq_time` subcommand, specifies the time of acquisition.
`<unit>` | For the `acq_time` subcommand, specifies the unit of the time of acquisition: us, ns, ticks.
`<channel_id>` | For the `channel` subcommand, specifies the channel ID.
`<gain>` | For the `gain` subcommand, specifies the gain of the adc value, e.g. GAIN_1_6, GAIN_2, GAIN_64.
`<channel>` | ADC channel.
`<reference>` | For the `reference` subcommand, specifies the reference for the adc measurement, e.g. VDD_1, INTERNAL.
`<resolution>` | For the `resolution` subcommand, specifies the resolution for the adc conversion for the `read` subcommand.

E.g.
```
21-12-22 13:51:11.380 uart:~$ adc ADC_0 read 5
adc ADC_0 read 5
21-12-22 13:51:26.031 read: 1023
```

### CrosEC command
The CrosEC shell command `adc` displays current ADC measurements, e.g.
```
21-12-21 09:58:03.746 uart:~$ adc
adc
21-12-21 09:58:05.322   ADC_VBUS = 14850 mV
21-12-21 09:58:05.325   ADC_AMON_BMON = 37111 mV
21-12-21 09:58:05.325   ADC_PSYS = 8432000 mV
```

## Example

The Volteer board uses Nuvoton NPCX EC that has one Analog-to-Digital Converter `adc0`, so enable it:
```
&adc0 {
	status = "okay";
};
```

The board has four temperature sensors handled with ADC. Map legacy cros-ec enums to ADC channels with
following values:

ADC Enumeration Name               | Volteer ADC channel
:--------------------------------- | :------------------
`ADC_TEMP_SENSOR_CHARGER`          | ADC0
`ADC_TEMP_SENSOR_PP3300_REGULATOR` | ADC1
`ADC_TEMP_SENSOR_DDR_SOC`          | ADC8
`ADC_TEMP_SENSOR_FAN`              | ADC3

The Volteer project establishes this map using the `named-adc-channels` as shown
below:

```
named-adc-channels {
	compatible = "named-adc-channels";

	adc_charger: charger {
		label = "TEMP_CHARGER";
		enum-name = "ADC_TEMP_SENSOR_CHARGER";
		channel = <0>;
	};
	adc_pp3300_regulator: pp3300_regulator {
		label = "TEMP_PP3300_REGULATOR";
		enum-name = "ADC_TEMP_SENSOR_PP3300_REGULATOR";
		channel = <1>;
	};
	adc_ddr_soc: ddr_soc {
		label = "TEMP_DDR_SOC";
		enum-name = "ADC_TEMP_SENSOR_DDR_SOC";
		channel = <8>;
	};
	adc_fan: fan {
		label = "TEMP_FAN";
		enum-name = "ADC_TEMP_SENSOR_FAN";
		channel = <3>;
	};
};
```

[EC ADC]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.adc?q=%22menuconfig%20PLATFORM_EC_ADC%22&ss=chromiumos
[CONFIG_ADC_SHELL]:
https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_ADC_SHELL.html
[ADC cmd]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.adc?q=%22config%20PLATFORM_EC_ADC_CMD%22&ss=chromiumos
[ADC resolution]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.adc?q=%22config%20PLATFORM_EC_ADC_RESOLUTION%22&ss=chromiumos
[ADC oversampling]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.adc?q=%22config%20PLATFORM_EC_ADC_OVERSAMPLING%22&ss=chromiumos
[ADC runtime config]:
https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.adc?q=%22config%20PLATFORM_EC_ADC_CHANNELS_RUNTIME_CONFIG%22&ss=chromiumos
[named-adc.yaml]:
../../zephyr/dts/bindings/adc/named-adc.yaml
