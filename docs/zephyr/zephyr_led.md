# Zephyr LED Configuration

[TOC]

## Overview

The LED subsystem allows to display the battery and charging states.
It supports multiple LEDs and colors controlled by GPIO or by PWM.

The behavior of LEDs can be defined using two methods.
First one, legacy, is to define the array describing the behavior in
board-specific code.
The second one is in progress and is only implemented for Lazor at the moment.
It uses the device-tree to specify the number of LEDs, colors and states which
triggers specific behavior.
Both methods are described in this document and are linked in the
[examples section](#examples).

## Kconfig Options

Kconfig Option                                  | Default | Documentation
:---------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_LED_COMMON`                 | n       | [LED subsystem]

Kconfig for LEDs controlled by PWM              | Default | Documentation
:---------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_LED_PWM`                    | n       | [LED - PWM]
`CONFIG_PLATFORM_EC_CONSOLE_CMD_LEDTEST`        | y       | [LED - console test]
`CONFIG_PLATFORM_EC_LED_PWM_CHARGE_COLOR`       | 5       | [LED PWM - charge color]
`CONFIG_PLATFORM_EC_LED_PWM_CHARGE_ERROR_COLOR` | 0       | [LED PWM - charge error color]
`CONFIG_PLATFORM_EC_LED_PWM_NEAR_FULL_COLOR`    | 1       | [LED PWM - near full color]
`CONFIG_PLATFORM_EC_LED_PWM_SOC_ON_COLOR`       | 1       | [LED PWM - SoC on color]
`CONFIG_PLATFORM_EC_LED_PWM_SOC_SUSPEND_COLOR`  | 1       | [LED PWM - SoC suspend color]
`CONFIG_PLATFORM_EC_LED_PWM_LOW_BATT_COLOR`     | 5       | [LED PWM - low battery color]

Kconfig for LEDs controlled by GPIO             | Default | Documentation
:---------------------------------------------- | :-----: | :------------
`CONFIG_PLATFORM_EC_LED_ONOFF_STATES`           | n       | [LED - GPIO]
`CONFIG_PLATFORM_EC_LED_ONOFF_STATES_BAT_LOW`   | 10      | [LED GPIO - low battery threshold]

## Devicetree Nodes

### PWM

Documenatation about enabling PWM is in another [file](zephyr_pwm.md).

Specification of required nodes are described in the
[binding file](/zephyr/dts/bindings/led/cros-ec,pwm-leds.yaml)

The example definition of LEDs with PWMs that belong to them:
```
pwmleds {
	compatible = "pwm-leds";
	pwm_led0: pwm_led_0 {
		pwms = <&pwm0 0 PWM_POLARITY_INVERTED
			&pwm1 0 PWM_POLARITY_INVERTED
			&pwm2 0 PWM_POLARITY_INVERTED>;
	};
};
```

And the definition of LEDs:
```
cros-pwmleds {
	compatible = "cros-ec,pwm-leds";

	leds = <&pwm_led0>;
	frequency = <100>;

	color-map-red    = <100   0   0>;
	color-map-green  = <  0 100   0>;
	color-map-amber  = <100  20   0>;

	brightness-range = <255 255 0 0 0 255>;

	#address-cells = <1>;
	#size-cells = <0>;

	pwm_led_0@0 {
		reg = <0>;
		ec-led-name = "EC_LED_ID_BATTERY_LED";
	};
};
```

### GPIO

The legacy GPIO interface doesn't require any special device tree nodes, except
that the GPIOs must be configured properly as outputs.

### LEDs behavior - work in progress

*This feature is currently implemented only for Lazor board.*

Specification of required nodes is described in the
[binding file](/zephyr/dts/bindings/gpio_led/cros-ec,gpio-led-colors.yaml)

Each color node defines state that trigger specific sequence, for
example the chipset state (suspend) or battery level (low power).
They can have up to `MAX_COLOR` phases.
The sub-nodes specifies the phase of colors sequence that LED will display.
They can either be static color, or dynamic sequence with period specified in
seconds.

## Board Specific Code

### Both GPIO and PWM

This section contains code that must be implemented by both interfaces, either
using the GPIO or PWM.

There are variables that define the low and full battery thresholds:
```
__override const int led_charge_lvl_1 = 5;
__override const int led_charge_lvl_2 = 97;
```

The behavior for battery LED must be defined in a two-dimensional array.
First index is the state which triggers the behavior, and the second one is a
phase of the color sequence.

The `LED_NUM_STATES` and `PWR_LED_NUM_STATES` values are defined in
[led_onoff_states.h](/include/led_onoff_states.h) header file and specify the
number of states (of battery and power, accordingly) that can trigger the
LEDs sequences.

The `LED_NUM_PHASES` value specifies how many phases can be specified within
one color sequence.
It also is defined in
[led_onoff_states.h](/include/led_onoff_states.h) header file.

The possible colors (if implemented by board) are defined in
[ec_commands.h](/include/ec_commands.h) header file as `enum ec_led_colors`.

```
__override struct led_descriptor
			led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
	[STATE_CHARGING_LVL_1]	     = {{EC_LED_COLOR_AMBER, LED_INDEFINITE} },
	[STATE_CHARGING_LVL_2]	     = {{EC_LED_COLOR_AMBER, LED_INDEFINITE} },
	...
	[STATE_CHARGING_FULL_CHARGE] = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
};
```

The power LED is configured the same as battery LED, only the name of an array
and the states are different:
```
__override const struct led_descriptor
		led_pwr_state_table[PWR_LED_NUM_STATES][LED_NUM_PHASES] = {
			...
		};
```

There's need to implement function that sets color for the battery LED:
```
__override void led_set_color_battery(enum ec_led_colors color)
{
	switch (color) {
	case EC_LED_COLOR_AMBER:
		pwm_enable(PWM_CH_LED_CHRG, LED_ON_LVL);
		pwm_enable(PWM_CH_LED_FULL, LED_OFF_LVL);
		break;
	...
	default: /* Unsupported colors */
		CPRINTS("Unsupported LED color: %d", color);
		break;
	}
}
```

The function that sets the color for power LED uses similar prototype:
```
__override void led_set_color_power(enum ec_led_colors color)
{
	...
}
```

### GPIO

This section contains code that is required by the GPIO interface in addition
to the code specified in paragraph [Both GPIO and PWM](#both-gpio-and-pwm).

There must be an array of LEDs types that will be controlled
(e.g. battery, power) and const value that contains number of these LEDs:
```
const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);
```

There are functions that set the brightness and return the brightness range
and are used by host commands. The set brightness function can be a wrapper to
the `led_set_color_battery` function which is responsible for powering on and
off the LEDs:
```
void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		brightness_range[EC_LED_COLOR_WHITE] = 1;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_battery(EC_LED_COLOR_WHITE);
		...
		else
			led_set_color_battery(LED_OFF);
	} else {
		CPRINTS("Unsupported LED set: %d", led_id);
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}
```

## Threads

The LEDs are controlled by hook task.

## Testing and Debugging

### PWM

The console command to test the LEDs can be enabled as described in
[Kconfig options](#kconfig-options) paragraph.

Name of the command is `ledtest` and the parameters it takes are
`<pwm led idx> <enable|disable> [color|off]`

Parameter       | Description
:-------------- | ------:
pwm_led_idx     | index of LED
enable\|disable | enable/disable LED testing
color\|off      | color can be one of the following: `red` \| `green` \| `amber` \| `blue` \| `white` \| `yellow`

Color availability depends on the board and configuration.

## Examples

### PWM

The example of board with LEDs using PWM interface is `kingler`.
Files that describe this feature are:

[Project configuration](/zephyr/projects/corsola/prj_kingler.conf)

[PWM configuration](/zephyr/projects/corsola/pwm_kingler.dts)

[LEDs definitions](/zephyr/projects/corsola/led_kingler.dts)

[LEDs board implementation](/zephyr/projects/corsola/src/kingler/led.c)

### GPIO

The example of board with LEDs using GPIO interface is `skyrim`.
It uses the GPIO interface, however internally the LEDs are controlled by PWM.
Files that describe this feature are:

[Project configuration](/zephyr/projects/skyrim/prj_skyrim.conf)

[LEDs board implementation](/zephyr/projects/skyrim/led.c)

### Lazor

These files describe the new implementation of LEDs behavior that is currently
implemented only for `Lazor` board:

[Project configuration](/zephyr/projects/trogdor/lazor/prj.conf)

[LEDs behavior in device-tree](/zephyr/projects/trogdor/lazor/led.dts)

[LEDs board implementation](/zephyr/projects/trogdor/lazor/src/led.c)

<!-- Links to Kconfigs -->

[LED subsystem]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=%22menuconfig%20PLATFORM_EC_LED_COMMON%22

[LED - PWM]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_PWM
[LED - console test]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_CONSOLE_CMD_LEDTEST
[LED PWM - charge color]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_PWM_CHARGE_COLOR
[LED PWM - charge error color]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_PWM_CHARGE_ERROR_COLOR
[LED PWM - near full color]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_PWM_NEAR_FULL_COLOR
[LED PWM - SoC on color]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_PWM_SOC_ON_COLOR
[LED PWM - SoC suspend color]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_PWM_SOC_SUSPEND_COLOR
[LED PWM - low battery color]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_PWM_LOW_BATT_COLOR

[LED - GPIO]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_ONOFF_STATES
[LED GPIO - low battery threshold]:https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/Kconfig.led?q=PLATFORM_EC_LED_ONOFF_STATES_BAT_LOW
