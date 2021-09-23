/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/cros_led.h>
#include "cros_board_info.h"

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "system.h"
#include "util.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(cros_led, LOG_LEVEL_ERR);

#define LED_ONE_SEC (1000 / HOOK_TICK_INTERVAL_MS)

#define LED_ON_LVL 1
#define LED_OFF_LVL 0

/* Driver convenience defines */
#define DRV_DATA(dev) ((struct cros_led_data *)(dev)->data)

#define LED_Y_C0_NODE  DT_NODE_EXISTS(DT_PATH(named_gpios, ec_chg_led_y_c0))
#define LED_Y_C1_NODE  DT_NODE_EXISTS(DT_PATH(named_gpios, ec_chg_led_y_c1))
#define LED_W_C0_NODE  DT_NODE_EXISTS(DT_PATH(named_gpios, ec_chg_led_w_c0))
#define LED_W_C1_NODE  DT_NODE_EXISTS(DT_PATH(named_gpios, ec_chg_led_w_c1))
#define LED_B_C1_NODE  DT_NODE_EXISTS(DT_PATH(named_gpios, ec_chg_led_b_c1))
#define LED_R_C0_NODE  DT_NODE_EXISTS(DT_PATH(named_gpios, ec_chg_led_r_c0))
#define LED_G_C0_NODE  DT_NODE_EXISTS(DT_PATH(named_gpios, ec_chg_led_g_c0))
#define POWER_LED_NODE DT_NODE_EXISTS(DT_NODELABEL(led_id_power))

#define CURRENT_BOARD    DT_NODELABEL(current_board)
#define BRIGHTNESS_RANGE DT_NODELABEL(brightness_range)
#define CHARGING         DT_NODELABEL(charging)
#define DISCHARGE_S0     DT_NODELABEL(discharge_s0)
#define DISCHARGE_S3     DT_NODELABEL(discharge_s3)
#define DISCHARGE_S5     DT_NODELABEL(discharge_s5)
#define ERROR            DT_NODELABEL(error)
#define NEAR_FULL        DT_NODELABEL(near_full)
#define IDLE             DT_NODELABEL(idle)
#define LED_CONTROL      DT_NODELABEL(led_control)
#define POWER_STATE_S0   DT_NODELABEL(power_state_s0)
#define POWER_STATE_S3   DT_NODELABEL(power_state_s3)
#define POWER_STATE_S5   DT_NODELABEL(power_state_s5)

/* Driver data */
struct cros_led_data {
    enum board_names board_name;
    struct battery_led_color battery_led;
#if POWER_LED_NODE
    struct power_led_color power_led;
#endif
    enum led_color led_control;
};

static void cros_led_ec_set_color_battery(const struct device *dev, enum led_color color)
{
    struct cros_led_data *data = DRV_DATA(dev);

    switch (data->board_name) {

    case COACHZ:
#if LED_Y_C0_NODE
        gpio_set_level(GPIO_EC_CHG_LED_Y_C0,
		    (color == LED_AMBER) ? LED_ON_LVL : LED_OFF_LVL);
#endif

#if LED_W_C0_NODE
	    gpio_set_level(GPIO_EC_CHG_LED_W_C0,
		    (color == LED_BLUE) ? LED_ON_LVL : LED_OFF_LVL);
#endif
        break;

    case PAZQUEL:
#if LED_Y_C1_NODE
        gpio_set_level(GPIO_EC_CHG_LED_Y_C1,
            (color == LED_RED) ? LED_ON_LVL : LED_OFF_LVL);
#endif

#if LED_W_C1_NODE
        gpio_set_level(GPIO_EC_CHG_LED_W_C1,
            (color == LED_BLUE) ? LED_ON_LVL : LED_OFF_LVL);
#endif
        break;

    case POMPOM:
#if LED_Y_C0_NODE
	    gpio_set_level(GPIO_EC_CHG_LED_Y_C0,
		    (color == LED_AMBER) ? LED_ON_LVL : LED_OFF_LVL);
#endif

#if LED_W_C0_NODE
    	gpio_set_level(GPIO_EC_CHG_LED_W_C0,
            (color == LED_WHITE) ? LED_ON_LVL : LED_OFF_LVL);
#endif
        break;

    case LAZOR:
    case MARZIPAN:
#if LED_Y_C1_NODE
        gpio_set_level(GPIO_EC_CHG_LED_Y_C1,
		    (color == LED_AMBER) ? LED_ON_LVL : LED_OFF_LVL);
#endif

#if LED_B_C1_NODE
	    gpio_set_level(GPIO_EC_CHG_LED_B_C1,
		    (color == LED_BLUE) ? LED_ON_LVL : LED_OFF_LVL);
#endif
        break;

    case HOMESTAR:
    case MRBLAND:
    case WORMDINGLER:
#if LED_R_C0_NODE
#if LED_G_C0_NODE
        gpio_set_level(GPIO_EC_CHG_LED_R_C0,
            (color == LED_RED) ? LED_ON_LVL : LED_OFF_LVL);
        gpio_set_level(GPIO_EC_CHG_LED_G_C0,
            (color == LED_GREEN) ? LED_ON_LVL : LED_OFF_LVL);
        if (color == LED_AMBER) {
            gpio_set_level(GPIO_EC_CHG_LED_R_C0, LED_ON_LVL);
            gpio_set_level(GPIO_EC_CHG_LED_G_C0, LED_ON_LVL);
        }
#endif
#endif
        break;

    default:
        break;
    };
}

#if POWER_LED_NODE
static void cros_led_ec_set_color_power(enum led_color color)
{
	gpio_set_level(GPIO_EC_PWR_LED_W,
		(color == LED_WHITE) ? LED_ON_LVL : LED_OFF_LVL);
}
#endif

//TODO: This doesn't work for Pazquel
static int cros_led_ec_set_brightness(const struct device *dev, enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_RED] != 0)
			cros_led_ec_set_color_battery(dev, LED_RED);
		else if (brightness[EC_LED_COLOR_GREEN] != 0)
			cros_led_ec_set_color_battery(dev, LED_GREEN);
		else if (brightness[EC_LED_COLOR_BLUE] != 0)
			cros_led_ec_set_color_battery(dev, LED_BLUE);
		else if (brightness[EC_LED_COLOR_WHITE] != 0)
			cros_led_ec_set_color_battery(dev, LED_WHITE);
		else if (brightness[EC_LED_COLOR_AMBER] != 0)
			cros_led_ec_set_color_battery(dev, LED_AMBER);
		else
			cros_led_ec_set_color_battery(dev, LED_OFF);
	}
#if POWER_LED_NODE
    else if (led_id == EC_LED_ID_POWER_LED) {
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			cros_led_ec_set_color_power(LED_WHITE);
		else
			cros_led_ec_set_color_power(LED_OFF);
	}
#endif

	return EC_SUCCESS;
}

static int cros_led_ec_get_brightness_range(const struct device *dev, enum ec_led_id led_id, uint8_t *brightness_range)
{
    if (led_id == EC_LED_ID_BATTERY_LED) {
        uint8_t temp[EC_LED_COLOR_COUNT] = DT_PROP(DT_PATH(gpio_led, brightness_range), brightness_range_battery);

        for (int i = 0; i < EC_LED_COLOR_COUNT; i++)
        {
            brightness_range[i] = temp[i];
        }
    }
#if POWER_LED_NODE
    else if (led_id == EC_LED_ID_POWER_LED) {
        uint8_t temp[EC_LED_COLOR_COUNT] = DT_PROP(DT_PATH(gpio_led, brightness_range), brightness_range_power);

        for (int i = 0; i < EC_LED_COLOR_COUNT; i++)
        {
            brightness_range[i] = temp[i];
        }
    }
#endif

    return EC_SUCCESS;
}

static int cros_board_led_ec_set_battery(const struct device *dev)
{
    struct cros_led_data *data = DRV_DATA(dev);
	static int battery_ticks;
	int color = LED_OFF;
	int period = 0;
	uint32_t chflags = charge_get_flags();

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		color = data->battery_led.charging_led;
		break;
	case PWR_STATE_DISCHARGE:
        // TODO: Need additional work to make it work Coachz
        if (data->battery_led.chipset_state) {
            if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
                /* Discharging in S3 */
                period = (data->battery_led.discharge_s3_led_period) * LED_ONE_SEC;
                battery_ticks = battery_ticks % period;
                if (battery_ticks < 1 * LED_ONE_SEC)
                    color = data->battery_led.discharge_s3_led_1;
                else
                    color = data->battery_led.discharge_s3_led_2;
            } else if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
                /* Discharging in S5 */
                color = data->battery_led.discharge_s5_led;
            } else if (chipset_in_state(CHIPSET_STATE_ON)) {
                /* Discharging in S0 */
                color = data->battery_led.discharge_s0_led;
            }
        } else {
            color = LED_OFF;
        }
		break;
	case PWR_STATE_ERROR:
		/* Battery error */
		period = (data->battery_led.error_led_period) * LED_ONE_SEC;
		battery_ticks = battery_ticks % period;
		if (battery_ticks < 1 * LED_ONE_SEC)
			color = data->battery_led.error_led_1;
		else
			color = data->battery_led.error_led_2;
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		/* Full Charged */
        // TODO: Need additional logic for chipset states
		color = data->battery_led.near_full_led;
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		if (chflags & CHARGE_FLAG_FORCE_IDLE) {
			/* Factory mode */
			period = (data->battery_led.idle_led_period) * LED_ONE_SEC;
			battery_ticks = battery_ticks % period;
			if (battery_ticks < 2 * LED_ONE_SEC)
				color = data->battery_led.idle_led_1;
			else
				color = data->battery_led.idle_led_2;
		} else
			color = data->battery_led.idle_led_3;
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}

	cros_led_ec_set_color_battery(dev, color);

    return EC_SUCCESS;
}

#if POWER_LED_NODE
static void cros_board_led_ec_set_power(const struct device *dev)
{
    struct cros_led_data *data = DRV_DATA(dev);
	static int power_ticks;
	int color = LED_OFF;
	int period = 0;

	power_ticks++;

	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
		/* S3: On 1 sec, Off 3 sec */
		period = data->power_led.s3_period;
		power_ticks = power_ticks % period;
		if (power_ticks < 1)
			color = data->power_led.s3_led_1;
		else
			color = data->power_led.s3_led_2;
	} else if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		/* S5: LED off */
		color = data->power_led.s5_led;
	} else if (chipset_in_state(CHIPSET_STATE_ON)) {
		/* S0: LED on */
		color = data->power_led.s0_led;
	}

	cros_led_ec_set_color_power(color);
}
#endif

static int cros_led_ec_control(const struct device *dev, enum ec_led_id led_id, enum ec_led_state state)
{
    struct cros_led_data *data = DRV_DATA(dev);
	enum led_color color;

	if ((led_id != EC_LED_ID_RECOVERY_HW_REINIT_LED) &&
	    (led_id != EC_LED_ID_SYSRQ_DEBUG_LED))
		return EC_SUCCESS;

	if (state == LED_STATE_RESET) {
		led_auto_control(EC_LED_ID_BATTERY_LED, 1);
		cros_board_led_set_battery(dev);
		return EC_SUCCESS;
	}

	color = state ? data->led_control : LED_OFF;

	led_auto_control(EC_LED_ID_BATTERY_LED, 0);

	cros_led_ec_set_color_battery(dev, color);

    return EC_SUCCESS;
}

static int cros_led_ec_init(const struct device *dev)
{
    ARG_UNUSED(dev);

    return 0;
}

static int led_init(const struct device *dev)
{
    struct cros_led_data *data = DRV_DATA(dev);

    /* Board Name */
    data->board_name = DT_ENUM_TOKEN(CURRENT_BOARD, current_board_name);

    /* Charge State */
    data->battery_led.charging_led = DT_ENUM_TOKEN(CHARGING, led_color);

    /* Discharge state */
#if DT_NODE_HAS_PROP(DISCHARGE_S3, period)
    data->battery_led.discharge_s0_led = DT_ENUM_TOKEN(DISCHARGE_S0, led_color);
    data->battery_led.discharge_s3_led_1 = DT_ENUM_TOKEN(DISCHARGE_S3, led_color_1);
    data->battery_led.discharge_s3_led_2 = DT_ENUM_TOKEN(DISCHARGE_S3, led_color_2);
    data->battery_led.discharge_s3_led_period = DT_PROP(DISCHARGE_S3, period);
    data->battery_led.discharge_s5_led = DT_ENUM_TOKEN(DISCHARGE_S5, led_color);
#endif

    /* Error state */
    data->battery_led.error_led_1 = DT_ENUM_TOKEN(ERROR, led_color_1);
    data->battery_led.error_led_2 = DT_ENUM_TOKEN(ERROR, led_color_2);
    data->battery_led.error_led_period = DT_PROP(ERROR, period);

    /* Near full state */
    data->battery_led.near_full_led = DT_ENUM_TOKEN(NEAR_FULL, led_color);

    /* Idle state */
    data->battery_led.idle_led_1 = DT_ENUM_TOKEN(IDLE, led_color_1);
    data->battery_led.idle_led_2 = DT_ENUM_TOKEN(IDLE, led_color_2);
    data->battery_led.idle_led_3 = DT_ENUM_TOKEN(IDLE, led_color_3);
    data->battery_led.idle_led_period = DT_PROP(IDLE, period);


#if POWER_LED_NODE
    data->battery_led.s0_led = DT_ENUM_TOKEN(POWER_STATE_S0, led_color);
    data->battery_led.s3_led_1 = DT_ENUM_TOKEN(POWER_STATE_S3, led_color_1);
    data->battery_led.s3_led_2 = DT_ENUM_TOKEN(POWER_STATE_S3, led_color_2);
    data->battery_led.s3_period = DT_PROP(POWER_STATE_S3, period);
    data->battery_led.s5_led = DT_ENUM_TOKEN(POWER_STATE_S5, led_color);
#endif

    data->led_control = DT_ENUM_TOKEN(LED_CONTROL, led_color);

    return 0;
}

struct cros_led_data led_data;

/* cros ec led driver registration */
static const struct cros_led_driver_api cros_led_driver_api = {
    .init = cros_led_ec_init,
    .led_set_brightness = cros_led_ec_set_brightness,
    .led_get_brightness_range = cros_led_ec_get_brightness_range,
    .board_led_set_battery = cros_board_led_ec_set_battery,
    .led_control = cros_led_ec_control,
};

DEVICE_DEFINE(cros_led, CROS_LED_LABEL, led_init,
		      NULL, &led_data, NULL, PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		      &cros_led_driver_api);
