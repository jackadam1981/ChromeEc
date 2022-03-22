/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <init.h>
#include <ap_power/ap_power.h>

#include "battery.h"
#include "charge_state.h"
#include "hooks.h"
#include "led_common.h"

#include "led.h"
#include "led_gpio.h"
#include "led_pwm.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_ERR);

#define COMPAT_POLICY	cros_ec_led_policy

/*
 * LED policy processing, configured using the cros_ec_led_policy
 * compat, mapping the inputs to the LED behaviour and actions.
 * These inputs comprise of the AP state, the charger state, and the
 * battery state.
 *
 * The intent is that the handlers define the behaviour and actions of
 * the LEDs (such as colour, brightness, flashing etc.), and the policy
 * defines the inputs that then map to the actions.
 * The mapping and actions are managed via DTS.
 */

/*
 * Enum representing the handler types.
 */
enum led_hand_type {
	LED_HANDLER_GPIO,
	LED_HANDLER_PWM,
};

/*
 * Define a common enum for all handlers, and
 * a handler table for all handlers (for all
 * compats).
 */
#define LED_HAND_ID(id)	DT_CAT(L_H_, id)
#define GEN_HAND_ID_ENUM(id)	LED_HAND_ID(id),

enum led_hand {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	DT_FOREACH_STATUS_OKAY(COMPAT_GPIO, GEN_HAND_ID_ENUM)
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_HAND_ID_ENUM)
#endif
};

/*
 * Map led handlers to EC LED ID enums.
 * If no led id enum, use 0xFF. This is used to call
 * led_auto_control_is_enabled() to check whether the LED
 * is auto-controlled.
 */
#define GEN_LED_ID_MAP(id)				\
	COND_CODE_1(DT_NODE_HAS_PROP(id, label),	\
		(DT_STRING_UPPER_TOKEN(id, label), ),	\
		(0xFF, ))

const static uint8_t led_id_map[] = {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_LED_ID_MAP)
#endif
};

/*
 * Define a handler table indexed by the handler enum
 * that contains the type enum and the index of the
 * specific handler within that type.
 */
struct led_handler_to_type {
	uint8_t h_type;		/* enum led_hand_type */
	uint8_t h_index;	/* Index within type */
};

#define GEN_HAND_TO_TYPE_ENTRY(id, t)				\
{							\
	.h_type = t,					\
	.h_index = LED_TYPE_INDEX(id),			\
},

static const struct led_handler_to_type handlers[] = {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_GPIO, GEN_HAND_TO_TYPE_ENTRY,
				     LED_HANDLER_GPIO)
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_PWM, GEN_HAND_TO_TYPE_ENTRY,
				     LED_HANDLER_PWM)
#endif
};

/*
 * LED policy configuration. This is the second part of the
 * LED handling, which defines the policy input processing.
 *
 * The overall idea is that when changes occur (such as the
 * state of the AP changing), the input states will be matched against
 * the different policies, and the first matching policy will
 * be used - each policy references a LED action, which defines for that
 * LED handler what it should do.
 */

/*
 * LED policy input structure. The policy is the combination of the
 * AP state, charger state, and battery capacity. Any or all of these
 * can be ignored.
 * The action is the action index for the LED handler associated with
 * this policy.
 */
struct led_policy_entry {
	uint8_t ap_state;	/* AP state to match */
	uint8_t charger_state;	/* Charger state to match */
	uint8_t battery[2];	/* Battery percentage range to match */
	uint8_t action;		/* Action to take when matched */
};

/*
 * The policy for a particular LED handler.
 * Each LED handler should have only a single list of policies associated
 * with it, which defines the actions that handler should take.
 */
struct led_policy {
	uint8_t led;	/* enum for this LED handler */
	uint8_t count;	/* The number of policy comparison entries */
	const struct led_policy_entry *entries;
};

/*
 * Inputs for LED policy control.
 * These enums define the policy inputs used to
 * identify the policy to use (and thus the LED action
 * to take).
 */
enum led_ap_state {
	LED_AP_ANY,		/* Any state */
	LED_AP_SUSPENDED,	/* AP is suspended */
	LED_AP_RUNNING,		/* AP is running */
	LED_AP_POWER_OFF,	/* AP is powered off */
};

enum led_charger_state {
	LED_CHARGER_ANY,		/* Any state */
	LED_CHARGER_FULL,		/* Charger present, battery full */
	LED_CHARGER_CHARGING,		/* Charging */
	LED_CHARGER_DISCHARGING,	/* No charger connected */
	LED_CHARGER_IDLE,		/* External power connected in IDLE */
	LED_CHARGER_ERROR,		/* Charger fault */
};

/*
 * The current state of the AP, charger and battery charge.
 */
static uint8_t cpu_state = LED_AP_POWER_OFF;
static uint8_t charger_state = LED_CHARGER_DISCHARGING;
static uint8_t battery_state;

/*
 * Define macros to map node names to identifiers for
 * the policy nodes and child nodes.
 */
#define POLICY_CHECK_TAB(id)	DT_CAT(P_C_T_, id)
#define POLICY_TAB(id)	DT_CAT(P_T_, id)

#define AP_NAME(nm)	DT_CAT(LED_AP_, nm)
#define CHG_NAME(nm)	DT_CAT(LED_CHARGER_, nm)

/*
 * Generate the policy comparison arrays.
 */
#define GEN_CHECK_TAB_ENTRY(id)					\
{								\
	.ap_state = AP_NAME(DT_STRING_TOKEN(id, cpu)),		\
	.charger_state = CHG_NAME(DT_STRING_TOKEN(id, charger)), \
	.battery = DT_PROP(id, battery),				\
	.action = LED_ACTION(DT_PHANDLE(id, action)),		\
},

#define GEN_CHECK_TABLE(id)					\
static const struct led_policy_entry POLICY_CHECK_TAB(id)[] = {	\
	DT_FOREACH_CHILD(id, GEN_CHECK_TAB_ENTRY)		\
};

DT_FOREACH_STATUS_OKAY(COMPAT_POLICY, GEN_CHECK_TABLE)

/*
 * Generate the policy array, representing the policy for
 * a single LED handler.
 */
#define GEN_POLICY_TABLE(id)					\
{								\
	.led = LED_HAND_ID(DT_PHANDLE(id, led)),		\
	.count = ARRAY_SIZE(POLICY_CHECK_TAB(id)),		\
	.entries = POLICY_CHECK_TAB(id),			\
},

static const struct led_policy policy_table[] = {
DT_FOREACH_STATUS_OKAY(COMPAT_POLICY, GEN_POLICY_TABLE)
};

/*
 * LED input and event processing.
 */

/*
 * These functions use the handler type to call the
 * appropriate function for each type.
 * Poor man's polymorphism.
 */

/*
 * Set the handler to this action.
 */
static void set_led_action(int handler, int action)
{
	switch (handlers[handler].h_type) {
	default:
		__ASSERT(false, "Unknown handler");
		break;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	case LED_HANDLER_GPIO:
		gpio_set_led_action(handlers[handler].h_index, action);
		break;
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	case LED_HANDLER_PWM:
		pwm_set_led_action(handlers[handler].h_index, action);
		break;
#endif
	}
}

/*
 * Get the brightness range. The brightness range is
 * an array of colors (enum ec_led_colors in ec_command.h),
 * and the values are returned depending on whether the
 * LED can display that color. The array is set to 1 for
 * GPIO LEDS, 255 for PWM LEDS, or 0 for unsupported LEDS.
 */
static void get_led_brightness(int handler, uint8_t *br)
{
	switch (handlers[handler].h_type) {
	default:
		__ASSERT(false, "Unknown handler");
		break;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	case LED_HANDLER_GPIO:
		gpio_get_led_brightness(handlers[handler].h_index, br);
		break;
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	case LED_HANDLER_PWM:
		pwm_get_led_brightness(handlers[handler].h_index, br);
		break;
#endif
	}
}

/*
 * Set the brightness range. Turn on the selected color.
 */
static void set_led_brightness(int handler, const uint8_t *br)
{
	switch (handlers[handler].h_type) {
	default:
		__ASSERT(false, "Unknown handler");
		break;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	case LED_HANDLER_GPIO:
		gpio_set_led_brightness(handlers[handler].h_index, br);
		break;
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	case LED_HANDLER_PWM:
		pwm_set_led_brightness(handlers[handler].h_index, br);
		break;
#endif
	}
}
/*
 * Disable auto control, so if stop timer and clear action.
 */
static void led_shutdown(int handler)
{
	switch (handlers[handler].h_type) {
	default:
		__ASSERT(false, "Unknown handler");
		break;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	case LED_HANDLER_GPIO:
		gpio_led_shutdown(handlers[handler].h_index);
		break;
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	case LED_HANDLER_PWM:
		pwm_led_shutdown(handlers[handler].h_index);
		break;
#endif
	}
}

/*
 * Some event has occurred which may require updating the
 * LEDs. Iterate through the LED policies, using the first
 * matched policy to identify the action to be taken.
 */
static void update_leds(void)
{
	/*
	 * Go through all the policy tables.
	 * There may be multiple LEDs, each with a different policy.
	 */
	for (int i = 0; i < ARRAY_SIZE(policy_table); i++) {
		int led = policy_table[i].led;
		uint8_t led_id = led_id_map[led];
		const struct led_policy_entry *e;

		/*
		 * If the LED associated with this policy does
		 * not have auto control on, skip it.
		 */
		if (led_id != 0xFF && !led_auto_control_is_enabled(led_id)) {
			led_shutdown(led);
			continue;
		}
		/*
		 * Iterate through the policy checks for this
		 * policy and see if any match.
		 */
		e = policy_table[i].entries;
		for (int j = 0; j < policy_table[i].count; j++, e++) {
			/* Check for AP state match */
			if (e->ap_state != LED_AP_ANY &&
			    e->ap_state != cpu_state) {
				continue;
			}
#if defined(CONFIG_CHARGER)
			/* Check for charger state match */
			if (e->charger_state != LED_CHARGER_ANY &&
			    e->charger_state != charger_state) {
				continue;
			}
#endif
#if defined(CONFIG_BATTERY)
			/* Check battery charge match */
			if (battery_state < e->battery[0] ||
			    battery_state > e->battery[1]) {
				continue;
			}
#endif
			/*
			 * Found a matching policy.
			 * Call the LED handler with this
			 * action.
			 */
			set_led_action(led, e->action);
			break;
		}
	}
}

/*
 * Callback for detecting changes to the AP state.
 * Update the cpu state and update the LEDs.
 */
static void cpu_update(struct ap_power_ev_callback *cb,
		       struct ap_power_ev_data data)
{
	switch (data.event) {
	default:
		break;

	case AP_POWER_RESUME:
		cpu_state = LED_AP_RUNNING;
		break;

	case AP_POWER_SUSPEND:
		cpu_state = LED_AP_SUSPENDED;
		break;

	case AP_POWER_SHUTDOWN:
		cpu_state = LED_AP_POWER_OFF;
		break;
	}
	update_leds();
}

#if defined(CONFIG_CHARGER) || defined(CONFIG_BATTERY)
/*
 * Poll the battery and charger every second and update
 * the LEDs.
 */
static void led_poll_inputs(void)
{
#if defined(CONFIG_CHARGER)
	switch (charge_get_state()) {
	default:
		break;
	case PWR_STATE_CHARGE:
		charger_state = LED_CHARGER_CHARGING;
		break;
	case PWR_STATE_DISCHARGE:
		charger_state = LED_CHARGER_DISCHARGING;
		break;
	case PWR_STATE_ERROR:
		charger_state = LED_CHARGER_ERROR;
		break;
	case PWR_STATE_IDLE:
		charger_state = LED_CHARGER_IDLE;
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		charger_state = LED_CHARGER_FULL;
		break;
	}
#endif
#if defined(CONFIG_BATTERY)
	battery_state = charge_get_percent();
#endif
	update_leds();
}

DECLARE_HOOK(HOOK_SECOND, led_poll_inputs, HOOK_PRIO_DEFAULT);
#endif /* CONFIG_CHARGER || CONFIG_BATTERY */

/*
 * Initialise the LED processing.
 */
static int init_led(const struct device *unused)
{
	static struct ap_power_ev_callback cb;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	pwm_led_init();
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	gpio_led_init();
#endif
	ap_power_ev_init_callback(&cb, cpu_update,
				  AP_POWER_RESUME |
				  AP_POWER_SUSPEND |
				  AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);
	return 0;
}

SYS_INIT(init_led, APPLICATION, 1);

/*
 * API for EC host commands.
 */

#define GEN_LABEL_ENUM(id)				\
	COND_CODE_1(DT_NODE_HAS_PROP(id, label),	\
		(DT_STRING_UPPER_TOKEN(id, label), ),	\
		())

const enum ec_led_id supported_led_ids[] = {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_LABEL_ENUM)
#endif
};

BUILD_ASSERT((sizeof(supported_led_ids) != 0),
	     "Must define at least one EC LED ID label");
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

/*
 * Finds the LED handler associated with this LED ID.
 * Returns -1 if not found.
 */
static int led_id_to_handler(enum ec_led_id led_id)
{
	for (int i = 0; i < ARRAY_SIZE(led_id_map); i++) {
		if (led_id_map[i] == led_id) {
			return i;
		}
	}
	return -1;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	int hand = led_id_to_handler(led_id);

	if (hand >= 0) {
		get_led_brightness(hand, brightness_range);
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	int hand = led_id_to_handler(led_id);

	if (hand >= 0) {
		set_led_brightness(hand, brightness);
	}
	return 0;
}
