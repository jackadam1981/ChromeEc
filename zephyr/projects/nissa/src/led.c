/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <init.h>
#include <drivers/pwm.h>
#include <ap_power/ap_power.h>

#include "battery.h"
#include "charge_state.h"
#include "hooks.h"
#include "led_common.h"

#include <logging/log.h>

#define COMPAT_GPIO	cros_ec_gpio_leds
#define COMPAT_PWM	cros_ec_multi_pwm_leds
#define COMPAT_POLICY	cros_ec_led_policy

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/*
 * This file is basically in 2 parts.
 *
 * The first deals with the LED handlers that drive
 * the LED hardware via PWM or GPIOs, and
 * defined via the cros_ec_multi_pwm_leds and
 * cros_ec_gpio_leds compats.
 * TODO: Complete GPIO implementation.
 *
 * The second is the policy portion, configured using the
 * cros_ec_led_policy compat, that maps the inputs to the
 * LED behaviour and actions.
 *
 * The intent is that the first part defines the behaviour and
 * actions of the LEDs (such as colour, brightness, flashing etc.),
 * and the second defines the inputs that then map to the actions.
 * The mapping and actions are managed via DTS.
 */

/*
 * Common macros and defines used by both PWM and GPIO handlers.
 */
/*
 * Duration is stored in tenths of a second.
 */
#define D_TICKS(d)	K_MSEC((d) * 100)

/*
 * Generate names or enums used to reference the LED handler structures.
 * These names are used entirely internally to this file
 * to allow the policy tables to reference the LED actions and
 * handler tables. The names are generated from the DTS node name
 * prepended with a string.
 */

#define LED_HANDLER_INDEX(id)	DT_CAT(L_H_I_, id)
#define LED_ACTION(id)	DT_CAT(L_A_, id)
#define LED_HAND_TAB(id)	DT_CAT(L_H_T_, id)
#define LED_ACTION_TAB(id)	DT_CAT(L_A_T_, id)

#define GEN_HAND_INDEX_ENUM(id)	LED_HANDLER_INDEX(id),

/*
 * Generate enums for the action indices for
 * each of the LED handlers. These are not typed
 * as they are only used internally as an index.
 */
#define GEN_ACTION_ENUM(id)	LED_ACTION(id),

#define GEN_ACTION_ENUM_LIST(id)				\
enum {								\
	DT_FOREACH_CHILD(id, GEN_ACTION_ENUM)			\
};

/*
 * PWM LED handler
 */
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)

/*
 * Defines a single PWM output driving a LED.
 * These are used as an array representing
 * multiple PWM outputs to a grouped multi-colour LED.
 */
struct pwm_led {
	const struct device *pwm;
	uint8_t chan;
	pwm_flags_t flags;
};

/*
 * Led action, which holds a list of colours/durations.
 * The colors are stored as a byte array, and each RGB/duration
 * set takes up 4 bytes.
 * The colours are sets of 4 values, the first three are
 * the RGB colour values (as a percentage 0-100), and the
 * fourth is a duration (in tenths of a second)
 * If there is more than a single set of colours/duration,
 * the colours are cycled through, with the duration
 * indicating how long to pause before moving to the next
 * colour. If there is only one set of RGB/duration values,
 * the duration is ignored.
 */
struct led_pwm_action {
	const uint8_t *colors;
	uint8_t color_count;	/* Number of RGB/duration entries */
};

/*
 * Multi-PWM LED handler read-only configuration.
 * Stores the DTS originated configuration for each
 * handler for the multi-PWM LED driver.
 */
struct led_multi_pwm {
	uint32_t frequency;	/* Not sure if this is needed */
	uint8_t pwm_count;	/* Number of PWMs */
	uint8_t action_count;	/* Number of LED actions */
	const struct pwm_led *pwms;
	const struct led_pwm_action *actions;
	const uint8_t *color_map;
};

/*
 * Multi-PWM state.
 * Stores the current state of the multi-PWM LED handler.
 * Used to hold the timer for blinking.
 */
struct led_multi_pwm_state {
	struct k_timer timer;
	uint8_t current_action;
	uint8_t current_color;
	uint8_t auto_control;
};

#define LED_PWM_COLOR_LIST(id)	DT_CAT(L_C_L_, id)
#define LED_PWM_COLOR_MAP(id)	DT_CAT(L_C_M_, id)

/*
 * Enum representing the multi-PWM LED handlers.
 * One is created for each instance of the multi-PWM LED
 * handlers.
 */
enum led_pwm_hand {
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_HAND_INDEX_ENUM)
	LED_PWM_HAND_COUNT	/* Needed to avoid an empty enum list */
};

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_ACTION_ENUM_LIST)

/*
 * Generate byte arrays containing the LED colour
 * configuration. Separate arrays are generated for
 * each child node belonging to separate multi-PWM LEDs,
 * and named using the node name so they can be referenced later.
 */
#define GEN_PWM_COLOR_LIST(id)				\
static const uint8_t LED_PWM_COLOR_LIST(id)[] =		\
	DT_PROP(id, colors)				\
;							\
BUILD_ASSERT((sizeof(LED_PWM_COLOR_LIST(id)) % 4) == 0,	\
	     "Wrong number of color values");

#define GEN_PWM_COLOR_LISTS(id)				\
	DT_FOREACH_CHILD(id, GEN_PWM_COLOR_LIST)	\

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_PWM_COLOR_LISTS)

/*
 * Generate a led_pwm_action array, with one entry for
 * each colors list.
 */
#define GEN_PWM_ACTION_LIST(id)					\
{								\
	.colors = LED_PWM_COLOR_LIST(id),				\
	.color_count = (sizeof(LED_PWM_COLOR_LIST(id))/4),		\
},

#define GEN_PWM_ACTION_TAB(id)					\
static const struct led_pwm_action LED_ACTION_TAB(id)[] = {		\
	DT_FOREACH_CHILD(id, GEN_PWM_ACTION_LIST)			\
};

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_PWM_ACTION_TAB)

/*
 * Generate the PWM list for each handler.
 * This list captures the PWM device, channel and flags for
 * each PWM used to drive the LEDs.
 * The list is named using a unique name based on the node name
 * so that it can be referenced later.
 */
#define GEN_PWM_ENTRY(id, p, idx)				\
{								\
	.pwm = DEVICE_DT_GET(DT_PWMS_CTLR_BY_IDX(id, idx)),	\
	.chan = DT_PWMS_CHANNEL_BY_IDX(id, idx),		\
	.flags = DT_PWMS_FLAGS_BY_IDX(id, idx),			\
},

#define GEN_PWM_TABLE(id)					\
static const struct pwm_led LED_HAND_TAB(id)[] = {		\
	DT_FOREACH_PROP_ELEM(id, pwms, GEN_PWM_ENTRY)		\
};

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_PWM_TABLE)

/*
 * Generate byte array holding color map for
 * EC host command brightness mapping.
 */
#define GEN_PWM_COLOR_MAP(id)					\
static const uint8_t LED_PWM_COLOR_MAP(id)[] = DT_PROP(id, color_list);

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_PWM_COLOR_MAP)
/*
 * Generate handler structures. These represent the
 * top level multi-PWM configuration, and contains
 * references to the LED actions and the list
 * of PWMs for each handler.
 */

#define GEN_HAND_TABLE(id)					\
{								\
	.frequency = DT_PROP(id, frequency),			\
	.pwm_count = DT_PROP_LEN(id, pwms),			\
	.action_count = ARRAY_SIZE(LED_ACTION_TAB(id)),		\
	.pwms = LED_HAND_TAB(id),				\
	.actions = LED_ACTION_TAB(id),				\
	.color_map = LED_PWM_COLOR_MAP(id),			\
},

static const struct led_multi_pwm pwm_hand[] = {
DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_HAND_TABLE)
};

/*
 * Contains the run-time status of the handlers,
 * such as the current action, and the current color index
 * in the event of colors being cycled (blinking etc.)
 */
static struct led_multi_pwm_state pwm_state[ARRAY_SIZE(pwm_hand)];

/*
 * Update the LED PWM settings using the RGB color values provided.
 * The color values are defined as a percentage (0-100), and
 * this is used to calculate the duty cycle of the PWM.
 */
static void set_pwm_led_values(const struct led_multi_pwm *pwm,
			       const uint8_t *colors)
{
	uint32_t period = 10*1000; /* 10 ms */
	const struct pwm_led *pl = pwm->pwms;

	for (int i = 0; i < pwm->pwm_count; i++, pl++, colors++) {
		/*
		 * Calculate PWM percentage duty cycle.
		 */
		uint32_t pulse = *colors * (10*1000/100);

		pwm_pin_set_usec(pl->pwm, pl->chan, period,
				 pulse, pl->flags);
	}
}

/*
 * Colour cycle timer callback. Move to the next colour,
 * wrapping to the beginning when finished, and restart the timer
 * with the new duration.
 */
static void pwm_led_timer(struct k_timer *timer)
{
	struct led_multi_pwm_state *state = k_timer_user_data_get(timer);
	const struct led_multi_pwm *pwm = &pwm_hand[state - pwm_state];
	const struct led_pwm_action *ap = &pwm->actions[state->current_action];
	const uint8_t *cp;

	/*
	 * Cycle to next colour in list.
	 */
	state->current_color++;
	if (state->current_color >= ap->color_count) {
		state->current_color = 0;
	}
	cp = &ap->colors[state->current_color * 4];
	k_timer_start(&state->timer, D_TICKS(cp[3]), K_FOREVER);
	set_pwm_led_values(pwm, cp);
}

/*
 * Initialise the runtime state of the PWM LED handler.
 */
static void pwm_led_init(void)
{
	/*
	 * Initialise timer.
	 */
	for (int i = 0; i < ARRAY_SIZE(pwm_state); i++) {
		pwm_state[i].auto_control = 1;
		k_timer_init(&pwm_state[i].timer,
			     pwm_led_timer,
			     NULL);
		k_timer_user_data_set(&pwm_state[i].timer,
				      &pwm_state[i]);
	}
}

static inline int pwm_get_led_auto(enum led_pwm_hand h)
{
	return pwm_state[h].auto_control;
}

static inline void pwm_set_led_auto(enum led_pwm_hand h, int enable)
{
	pwm_state[h].auto_control = enable;
	if (!enable) {
		/*
		 * If disabling automatic control,
		 * make sure the timer is stopped.
		 */
		k_timer_stop(&pwm_state[h].timer);
	}
}

static void pwm_get_led_brightness(enum led_pwm_hand h, uint8_t *br)
{
	const struct led_multi_pwm *pwm = &pwm_hand[h];
	const uint8_t *cmp = pwm->color_map;
	/*
	 * Walk through the color_map for the PWM LED and
	 * set the brightness range to 255 for each
	 * of the supported colors.
	 */
	for (int i = 0; i < pwm->pwm_count; i++) {
		br[cmp[i]] = 255;
	}
}

static void pwm_set_led_brightness(enum led_pwm_hand h, const uint8_t *br)
{
	uint8_t colors[4];
	const struct led_multi_pwm *pwm = &pwm_hand[h];
	const uint8_t *cmp = pwm->color_map;
	/*
	 * Walk through the color_map for the PWM LED and
	 * set the channel according to the
	 * brightness range selected. The color_map
	 * entries are the indices of the supported colors
	 * in the brightness array.
	 */
	for (int i = 0; i < pwm->pwm_count; i++) {
		colors[i] = br[cmp[i]];
	}
	set_pwm_led_values(pwm, colors);
}

/*
 * Set the LED behaviour of this multi-PWM LED handler
 * to the action passed. The action identifies the list
 * of colours and duration (if any).
 */
static void pwm_set_led(enum led_pwm_hand h, int action)
{
	struct led_multi_pwm_state *state = &pwm_state[h];
	const struct led_multi_pwm *pwm;
	const struct led_pwm_action *ap;

	/*
	 * Check whether it is the same action, and
	 * skip setting the PWMs if the action hasn't changed.
	 */
	if (action == state->current_action) {
		return;
	}
	state->current_action = action;
	pwm = &pwm_hand[h];
	ap = &pwm->actions[action];
	/*
	 * Stop timer. No-op if timer is already stopped.
	 */
	k_timer_stop(&state->timer);
	/*
	 * Set colour values to PWM channels.
	 */
	set_pwm_led_values(&pwm_hand[h], ap->colors);
	/*
	 * Check whether timer needs to be enabled for flashing,
	 * when there is more than one set of colours.
	 */
	if (ap->color_count > 1) {
		state->current_color = 0;
		/* One shot timer */
		k_timer_start(&state->timer, D_TICKS(ap->colors[3]),
			      K_FOREVER);
	}
}
#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM) */

/*
 * Common data structures for both GPIO and PWM.
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
#define LED_HAND_NAME(id)	DT_CAT(L_H_, id)
#define GEN_HAND_ENUM(id)	LED_HAND_NAME(id),

enum led_hand {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	DT_FOREACH_STATUS_OKAY(COMPAT_GPIO, GEN_HAND_ENUM)
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_HAND_ENUM)
#endif
	LED_HAND_COUNT	/* Needed to avoid an empty enum list */
};

/*
 * Define a handler table indexed by the handler enum
 * that contains the type.
 */
struct led_handler {
	uint8_t h_type;		/* enum led_hand_type */
	uint8_t h_index;	/* Index within type */
};

#define GEN_HAND_ENTRY(id, t)				\
{							\
	.h_type = t,					\
	.h_index = LED_HANDLER_INDEX(id),		\
},

static const struct led_handler handlers[] = {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_GPIO, GEN_HAND_ENTRY,
				     LED_HANDLER_GPIO)
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_PWM, GEN_HAND_ENTRY,
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

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_POLICY)
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
	.led = LED_HANDLER_INDEX(DT_PHANDLE(id, led)),		\
	.count = ARRAY_SIZE(POLICY_CHECK_TAB(id)),		\
	.entries = POLICY_CHECK_TAB(id),			\
},

static const struct led_policy policy_table[] = {
DT_FOREACH_STATUS_OKAY(COMPAT_POLICY, GEN_POLICY_TABLE)
};

#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_POLICY) */

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
static void set_led(int handler, int action)
{
	switch (handlers[handler].h_type) {
	default:
		__ASSERT(false, "Unknown handler");
		break;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	case LED_HANDLER_GPIO:
		gpio_set_led(handlers[handler].h_index, action);
		break;
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	case LED_HANDLER_PWM:
		pwm_set_led(handlers[handler].h_index, action);
		break;
#endif
	}
}

/*
 * Get the auto control flag. The auto control flag can be
 * set or retrieved via the common LED API.
 * Returns true or false.
 */
static int get_led_auto(int handler)
{
	switch (handlers[handler].h_type) {
	default:
		__ASSERT(false, "Unknown handler");
		return 0;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	case LED_HANDLER_GPIO:
		return gpio_get_led_auto(handlers[handler].h_index);
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	case LED_HANDLER_PWM:
		return pwm_get_led_auto(handlers[handler].h_index);
#endif
	}
}

/*
 * Set the auto control flag.
 */
static void set_led_auto(int handler, int enable)
{
	switch (handlers[handler].h_type) {
	default:
		__ASSERT(false, "Unknown handler");
		break;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)
	case LED_HANDLER_GPIO:
		gpio_set_led_auto(handlers[handler].h_index, enable);
		break;
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	case LED_HANDLER_PWM:
		pwm_set_led_auto(handlers[handler].h_index, enable);
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
 * Some event has occurred which may require updating the
 * LEDs. Iterate through the LED policies, using the first
 * matched policy to identify the action to be taken.
 */
static void update_leds(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_POLICY)
	/*
	 * Go through all the policy tables.
	 * There may be multiple LEDs, each with a different policy.
	 */
	for (int i = 0; i < ARRAY_SIZE(policy_table); i++) {
		int led = policy_table[i].led;
		const struct led_policy_entry *e;

		/*
		 * If the LED associated with this policy does
		 * not have auto control on, skip it.
		 */
		if (get_led_auto(led)) {
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
			led = policy_table[i].led;
			if (get_led_auto(led)) {
				set_led(policy_table[i].led, e->action);
			}
			break;
		}
	}
#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_POLICY) */
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

/*
 * Struct to map EC LED ID enums to led handlers.
 */
#define GEN_LED_ID_MAP(n)					\
	COND_CODE_1(DT_NODE_HAS_PROP(n, label),		\
		({						\
		 .id = DT_STRING_UPPER_TOKEN(n, label),	\
		 .handler = LED_HAND_NAME(n)			\
		 }, ),						\
		())

const static struct {
	enum ec_led_id id;
	uint8_t handler;
} led_id_map[] = {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_LED_ID_MAP)
#endif
};

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
		if (led_id_map[i].id == led_id) {
			return led_id_map[i].handler;
		}
	}
	return -1;
}

void led_auto_control(enum ec_led_id led_id, int enable)
{
	int hand = led_id_to_handler(led_id);

	if (hand >= 0) {
		set_led_auto(hand, enable);
	}
}

int led_auto_control_is_enabled(enum ec_led_id led_id)
{
	int hand = led_id_to_handler(led_id);

	if (hand >= 0) {
		return get_led_auto(hand);
	}
	return 0;
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
