# Zephyr EC LEDs

[TOC]

## Overview

[LEDs](../ec_terms.md#led) provide status about the following:

-   Dedicated battery state/charging state
-   Chromebook power
-   Adapter power
-   Left side USB-C port (battery state/charging state)
-   Right side USB-C port (battery state/charging state)
-   Recovery mode
-   Debug mode

LEDs can be configured as simple GPIOs, with on/off control only, or as [PWM](../ec_terms.md#pwm) with
adjustment brightness and color.

## Kconfig Options

The `CONFIG_PLATFORM_EC_LED_DT` option, found in the [Kconfig.led_dt](../../zephyr/Kconfig.led_dt) file, enables devicetree based configuration of LED
policies and colors.

Enabling the devicetree LED implementation requires that you disable the legacy EC implementation.

Example:
```
# LED
CONFIG_PLATFORM_EC_LED_COMMON=n
CONFIG_PLATFORM_EC_LED_DT=y
```

TODO: Enable other config options supported in legacy code.

## Devicetree Nodes

### GPIO based LEDs
GPIO based LEDs contain gpio_led_pins nodes described in [cros-ec,gpio_led_pins.yaml].

Example:
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
GPIO LED Pins dts file example: [led_pins_herobrine.dts]

### PWM based LEDs
PWM based LEDs contain pwm_led_pins nodes described in [cros-ec,pwm_led_pins.yaml] and physical config of the PWM pins described in [cros-ec,pwm_led_pin_config.yaml].

Example
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

PWM LED Pins dts file example: [led_pins_skyrim.dts]

### Policy nodes
led_policy nodes describe the LED policy and set the LED behavior by referencing gpio_led_pins or pwm_led_pins nodes. These are described in [cros-ec,led_policy.yaml]


Example 1: Discharge-S3 node

This node indicates that during charge-state `Discharge` and Chipset state `S3`, LED Amber is on for 1 sec and Off for 3 secs. This node doesn't depend on the `battery-level`, `extra-flag` and `charge-port` properties.

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


Example 2: Discharge-S0 node

This node indicates that during charge-state `Discharge` and Chipset state `S0`, if the battery level is between Empty and Low, LED white is On for 2 secs and Off for 1 sec. This node doesn't depend on `extra-flag` and `charge-port` properties.

```
led-policy {
	compatible = "cros-ec,led-policy";
	...
	...
	power-state-discharge-s0-batt-low {
		charge-state = "PWR_STATE_DISCHARGE";
		chipset-state = "POWER_S0";
		/* Battery percent range (>= Empty, <= Low) */
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
Note: It is recommended to split the policy specification and the pins specification into two devicetree files. e.g. [led_policy_skyrim.dts],  [led_pins_skyrim.dts]

LED policy dts file examples
[led_policy_skyrim.dts], [led_policy_herobrine.dts]

## Board Specific Code

None

## Threads

The LEDs are controlled by hook task in the file [led_driver/led.c](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/shim/src/led_driver/led.c).

## Testing and Debugging
TODO: Enable support for ledtest

## Examples

### How to setup LEDs and add nodes

![Alt text](https://screenshot.googleplex.com/4eqXmo2jLcSD6eL.png)

-   Look for the gpio/pwm pins in the schematic with which the LEDs are attached to.
-   In the above snippet, LEDs are configured to use PWM pins and attached to PWM2 and PWM3.
-   Add PWM config nodes as shown in [cros-ec,pwm_led_pin_config.yaml] and [led_pins_skyrim.dts].
-   Add pin nodes based on the color of the LEDs attached as shown in [cros-ec,pwm_led_pins.yaml] and [led_pins_skyrim.dts]. Name the nodes according to the LED color for readability. e.g. `color-amber`
-   Based on the device LED policy, create led_policy nodes as shown in [cros-ec,led_policy.yaml] and [led_policy_skyrim.dts].

### PWM

[Example CL enabling single port pwm based LEDs]

### GPIO

[Example CL enabling dual port gpio based LEDs]

<!-- Reference Links -->
[cros-ec,led_policy.yaml]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/leds/cros-ec,led-colors.yaml
[cros-ec,gpio_led_pins.yaml]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/leds/cros-ec,gpio-led-pins.yaml
[cros-ec,pwm_led_pins.yaml]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/leds/cros-ec,pwm-led-pins.yaml
[cros-ec,pwm_led_pin_config.yaml]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/dts/bindings/leds/cros-ec,pwm-led-pin-config.yaml
[led_policy_skyrim.dts]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/projects/skyrim/led_policy_skyrim.dts
[led_pins_skyrim.dts]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/projects/skyrim/led_pins_skyrim.dts
[led_policy_herobrine.dts]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/projects/herobrine/led_policy_herobrine.dts
[led_pins_herobrine.dts]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/ec/zephyr/projects/herobrine/led_pins_herobrine.dts
[Example CL enabling single port pwm based LEDs]: https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3651490
[Example CL enabling dual port gpio based LEDs]: https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3635067
