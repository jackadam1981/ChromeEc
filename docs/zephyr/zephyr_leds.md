# Zephyr EC LEDs

[TOC]

## Overview

LEDs provide status about the following:

-   Dedicated battery state/charging state
-   Chromebook power
-   Adapter power
-   Left side USB-C port (battery state/charging state)
-   Right side USB-C port (battery state/charging state)
-   Recovery mode
-   Debug mode

LEDs can be configured as simple GPIOs, with on/off control only, or as PWM with
adjustment brightness and color.

## Kconfig Options

`CONFIG_PLATFORM_EC_LED_DT` enables devicetree based configuration of LED
 policies and colors.

Enable devicetree implementation and disable legacy implementation.

Example:
```
# LED
CONFIG_PLATFORM_EC_LED_COMMON=n
CONFIG_PLATFORM_EC_LED_DT=y
```

TODO: Enable other config options supported in legacy code.

## Devicetree Nodes

### Add devicetree files
1. led_policy_$BOARD.dts
2. led_pins_$BOARD.dts

#### Add LED policy devicetree file

`led_policy_$BOARD.dts` contains LED policies and LED behavior for a particular policy.
Each policy node in the file describes a system state and LED behavior during that state.

LED behavior depends on different properties. These properties are optional and only
added to the nodes if they alter the LED behavior during that system state.

Properties of policy nodes:
1. `charge-state`
   -   PWR_STATE_CHARGE
   -   PWR_STATE_DISCHARGE
   -   PWR_STATE_ERROR
   -   PWR_STATE_IDLE
   -   PWR_STATE_CHARGE_NEAR_FULL

2. `chipset-state`
   -   POWER_S0
   -   POWER_S3
   -   POWER_S5

3. `battery-level`

   Battery level range using battery-level macros. e.g. `<BATTERY_LEVEL_LOW BATTERY_LEVEL_FULL>`
   
   Battery-level macros:
   -    BATTERY_LEVEL_EMPTY
   -    BATTERY_LEVEL_DISCHARGE
   -    BATTERY_LEVEL_CRITICAL
   -    BATTERY_LEVEL_LOW
   -    BATTERY_LEVEL_NEAR_FULL
   -    BATTERY_LEVEL_FULL

4. `extra-flag`
   -  LED_CHFLAG_FORCE_IDLE
   -  LED_CHFLAG_DEFAULT

5. `charge-port`
   -   0 or 1 depending on left or right charging port.

Policy nodes also contain color children nodes.

Properties of color children nodes:
1. `led-color`
   -    phandle to the LED pins node (described below).

2. `period-ms`
   -    period value in millisec. On-Off period for blinking LEDs. A missing period value indicates
        a solid LED.

Example 1:
```
led-policy {
	compatible = "cros-ec,led-policy";
	...
	...
	power-state-discharge-s3 {
		charge-state = "PWR_STATE_DISCHARGE";
		chipset-state = "POWER_S3";

		/* Amber 1 sec, off 3 sec */
		color-0 {
			led-color = <&color_amber>;
			period-ms = <1000>;
		};
		color-1 {
			led-color = <&color_off>;
			period-ms = <3000>;
		};
	};
	...
	...
}
```

The above node indicates that during charge-state `Discharge` and Chipset state `S3`, LED Amber is on for 1 sec and Off for 3 secs. This node doesn't depend on the `battery-level`, `extra-flag` and `charge-port` properties.

Example 2:
```
led-policy {
	compatible = "cros-ec,led-policy";
	...
	...
	power-state-discharge-s0-batt-low {
		charge-state = "PWR_STATE_DISCHARGE";
		chipset-state = "POWER_S0";
		/* Battery percent range (>= Empty, < Low) */
		batt-lvl = <BATT_LEVEL_EMPTY BATT_LEVEL_LOW>;

		/* White 2 sec, off 1 sec */
		color-0 {
			led-color = <&color_white>;
			period-ms = <2000>;
		};
		color-1 {
			led-color = <&color_off>;
			period-ms = <1000>;
		};
	};
	...
	...
};
```

The above node indicates that during charge-state `Discharge` and Chipset state `S0`, if the battery level is between Empty and Low, LED white is On for 2 secs and Off for 1 sec. This node doesn't depend on `extra-flag` and `charge-port` properties.

LED policy devicetree file examples
[led_policy_skyrim.dts], [led_policy_herobrine.dts]

#### Add LED pins devicetree file
`led_pins_$BOARD.dts` contains nodes that enable a particular color physically. These are referenced by policy nodes using phandles.

Properties of led_pins nodes:
1. `led-pins`
   -   Group of gpio or pwm pins and corresponding values.

2. `led-color`
   -   led_color enum. Used to identify a pins node that sets a particular color. Only used for ectool support and testing.

3. `led-id`
   -   ec_led_id enum. Only used for ectool support to identify led-ids supported by the board.

4. `br-color`
   -   brightness range color. Only used for ectool support of brightness range APIs.

`led_pins.dts` also contains hardware pin configuration for gpio or pwm pins.

Example 1:

pwm based LEDs
```
pwm_pins {
	compatible = "cros-ec,pwm-pin-config";

	pwm_y: pwm_y {
		#led-pin-cells = <1>;
		pwms = <&pwm2 0 PWM_HZ(100) PWM_POLARITY_INVERTED>;
	};

	pwm_w: pwm_w {
		#led-pin-cells = <1>;
		pwms = <&pwm3 0 PWM_HZ(100) PWM_POLARITY_INVERTED>;
	};
};

pwm-led-pins {
	compatible = "cros-ec,pwm-led-pins";
	pwm-frequency = <100>;
	...
	...
	color_amber: color-amber {
		led-color = "LED_AMBER";
		led-id = "EC_LED_ID_BATTERY_LED";
		br-color = "EC_LED_COLOR_AMBER";
		led-pins = <&pwm_y 100>,
			   <&pwm_w 0>;
	};
	...
	...
};                                                                                                                    
```

The above node indicates that in order to enable Amber LED, pwm_y needs to set to 100, and pwm_w needs to set to 0.

Example 2:

GPIO based LEDs

```
gpio-led-pins {
	compatible = "cros-ec,gpio-led-pins"; 
	...
	...
	color_amber: color-amber {
		led-color = "LED_AMBER";
		led-id = "EC_LED_ID_BATTERY_LED";
		br-color = "EC_LED_COLOR_AMBER";
		led-pins = <&gpio_ec_chg_led_y_c1 1>,
			   <&gpio_ec_chg_led_b_c1 0>;
	};
	...
	...
};
```

## Board Specific Code

None

## Threads

The LEDs are controlled by hook task.

## Testing and Debugging
TODO: Enable support for ledtest

## Examples

### PWM

[Example CL enabling single port pwm based LEDs]

### GPIO

[Example CL enabling dual port gpio based LEDs]

<!-- Reference Links -->
[Example CL enabling single port pwm based LEDs]: https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3651490
[Example CL enabling dual port gpio based LEDs]: https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3635067
