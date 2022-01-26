# Zephyr EC PWM Configuration

[TOC]

## Overview

[PWM] provide support for PWM setup and control on the platform.

## Kconfig Options

Kconfig Option | Default | Documentation
:------------- | :------ | :------------
`CONFIG_PWM` | n | [PWM (Pulse Width Modulation) Drivers]
`CONFIG_PWM_<platform>` | n | Platform specific PWM driver
`CONFIG_PLATFORM_EC_PWM` | n | [PWM (Pulse Width Modulation) module]

Kconfig sub-option | Default | Documentation
:----------------- | :------ | :------------
`CONFIG_PLATFORM_EC_PWM_INIT_PRIORITY` | 51 | [Init priority of the GPIO module]

## Devicetree Nodes

Configure the PWM module by declaring all the available PWM channels in the the
devicetree node with `compatible = "named-pwms"`. Every PWM line should have a
*node label* identifier which is then used to refer to it on a higher level
driver.

Property | Description | Settings
:------- | :---------- | :-------
`#pwm-cells` | Specifier cell count, always `<0>`, required if the node label is used in a `-pwms` property. | `<0>`
`pwms` | PWM phandle, identifies the controller (X), channel (Y) and flags. | `<&pwmX Y flags>`
`frequency` | PWM frequency, in Hz | `integer (32 bit unsigned)`

In the PWM child node declarations use the lower case net name from the
schematic as the *node name*, the *node label* can be chosen depending on the
actual function.

```
named-pwms {
	compatible = "named-pwms";

	led1_blue: led_1_l {
		#pwm-cells = <0>;
		pwms = <&pwm2 0 PWM_POLARITY_INVERTED>;
		frequency = <4800>;
	};
	...
};
```

The `flags` cell of `pwms` defines the PWM signal properties, valid options are
listed in the [dt-bindings/pwm/pwm.h].

## Board Specific Code

None required.

## Threads

PWM support does not enable any thread.

## Testing and Debugging

### Shell Command

The EC application defines a `pwmduty` console command that can be used to
inspect and change the current PWM duty cycle.

Command | Description | Usage
:------ | :---------- | :----
`pwmduty` | Get/set PWM duty cycles | `[channel [<percent> \| -1=disable] p [raw <value>]]`

Parameter | Description
:-------- | :----------
`channel` | The PWM channel number (in order of definition).
`percent` | Duty cycle percentage to set the specified channel to.
`value` | Raw value to set the specified channel to.

## Features using PWM directly

PWM nodes can be used by few drivers directly to implement specific features
with no additional devicetree configuration.

### Display backlight

Kconfig Option | Default | Documentation
:------------- | :------ | :------------
`CONFIG_PLATFORM_EC_PWM_DISPLIGHT` | n | [PWM display backlight]

This requires defining a PWM node with `displight` as a node label, for example:

```
named-pwms {
	compatible = "named-pwms";
...
	displight: edp_bkltctl {
		pwms = <&pwm5 0 0>;
		frequency = <4800>;
	};
...
}
```

### Keyboard backlight

Kconfig Option | Default | Documentation
:------------- | :------ | :------------
`PLATFORM_EC_PWM_KBLIGHT` | n | [PWM keyboard backlight]

This requires defining a PWM node with `kblight` as a node label, for example:

```
named-pwms {
	compatible = "named-pwms";
...
	kblight: kb_bl_pwm {
		pwms = <&pwm3 0 0>;
		frequency = <10000>;
	};
...
}
```

## Example

The image below shows PWM assignments on the Volteer reference board.

![PWM Example]

In this example the LED lines should be configured as:

Net Name | PWM  | Channel | Flags
:------- | :--- | :----- | :----
LED_1_L  | PWM2 | 0 | Active low
LED_2_L  | PWM0 | 0 | Active low
LED_3_L  | PWM1 | 0 | Active low
LED_SIDESEL_4_L  | PWM7 | 0 | Active low

Which translate in the devicetree node:

```
named-pwms {
	compatible = "named-pwms";

	led1_blue: led_1_l {
		#pwm-cells = <0>;
		pwms = <&pwm2 0 PWM_POLARITY_INVERTED>;
		frequency = <4800>;
	};
	led2_green: led_2_l  {
		#pwm-cells = <0>;
		pwms = <&pwm0 0 PWM_POLARITY_INVERTED>;
		frequency = <4800>;
	};
	led3_red: led_3_l {
		#pwm-cells = <0>;
		pwms = <&pwm1 0 PWM_POLARITY_INVERTED>;
		frequency = <4800>;
	};
	led3_sidesel: led_sidesel_4_l {
		#pwm-cells = <0>;
		pwms = <&pwm7 0 PWM_POLARITY_INVERTED>;
		frequency = <2400>;
	};
};
```

The corresponding PWM devices nodes have to be configured as well, and the
corresponding properties are platform specific. For example on NPCX
([nuvoton,npcx-pwm]) based platforms:

```
/* Green LED */
&pwm0 {
	status = "okay";
	clock-bus = "NPCX_CLOCK_BUS_LFCLK";
};
```

And on ITE ([ite,it8xxx2-pwm]) based platforms:

```
/* LED1 */
&pwm0 {
        status = "okay";
        prescaler-cx = <PWM_PRESCALER_C4>;
};
```

[PWM (Pulse Width Modulation) Drivers]: https://docs.zephyrproject.org/latest/reference/kconfig/CONFIG_PWM.html
[PWM (Pulse Width Modulation) module]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_PWM%22
[Init priority of the GPIO module]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.init_priority?q=%22config%20PLATFORM_EC_PWM_INIT_PRIORITY%22
[PWM display backlight]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_PWM_DISPLIGHT%22
[PWM keyboard backlight]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig?q=%22config%20PLATFORM_EC_PWM_KBLIGHT%22
[dt-bindings/pwm/pwm.h]: https://github.com/zephyrproject-rtos/zephyr/blob/main/include/dt-bindings/pwm/pwm.h
[PWM Example]: pwm_schematic.png
[nuvoton,npcx-pwm]: https://github.com/zephyrproject-rtos/zephyr/blob/main/dts/bindings/pwm/nuvoton%2Cnpcx-pwm.yaml
[ite,it8xxx2-pwm]: https://github.com/zephyrproject-rtos/zephyr/blob/main/dts/bindings/pwm/ite%2Cit8xxx2-pwm.yaml
