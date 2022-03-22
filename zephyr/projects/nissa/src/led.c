/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <init.h>
#include <drivers/pwm.h>
#include <ap_power/ap_power.h>

#include "battery.h"
#include "charger.h"
#include "charge_state_v2.h"

#include "nissa_common.h"

#include <logging/log.h>

#define COMPAT_PWM	cros_ec_multi_pwm_leds
#define COMPAT_POLICY	cros_ec_led_policy

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/*
 * This file is basically in 2 parts.
 *
 * The first deals with the LED handlers that drive
 * the LED hardware via PWM (or GPIOs, TODO), and
 * defined via the cros_ec_multi_pwm_leds compat.
 * At some point this can be expanded to include GPIO
 * based LEDs.
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
 * Defines a single PWM output driving a LED.
 * These are used as an array representing
 * multiple PWM outputs to a multi-colour LED.
 */
struct pwm_led {
	const struct device *pwm;
	uint8_t chan;
	pwm_flags_t flags;
};

/*
 * LED action (or LED behaviour).
 * Represents an action or behaviour for this LED handler.
 * For each set of PWM LED outputs, a value between
 * 0 and 100 is used to drive the PWM output, representing
 * off state to on state. Depending on the value of the
 * colors elements, different colors can be represented.
 * A blink value can be defined, with an on and off time
 * respectively (in milliseconds). The blink operates by
 * turning all the outputs off during the off phase.
 */
struct led_action {
	uint8_t colors[4];
	uint16_t blink[2];
};

/*
 * Multi-PWM LED handler read-only configuration.
 * Stores the DTS originated configuration for each
 * handler for the multi-PWM LED driver.
 */
struct led_multi_pwm {
	uint32_t frequency;
	uint8_t pwm_count;	/* Number of PWMs */
	uint8_t action_count;	/* Number of LED actions */
	const struct pwm_led *pwms;
	const struct led_action *actions;
};

/*
 * Multi-PWM state.
 * Stores the current state of the multi-PWM LED handler.
 * Used to hold the timer for blinking.
 */
struct led_multi_pwm_state {
	struct k_timer timer;
	uint8_t current_action;
	uint8_t current_state;
};

/*
 * Generate enums used to reference LED handlers and actions.
 * These enums are used entirely internally to this file
 * to allow the policy tables to reference the LED actions and
 * handler tables. The enum is generated from the DTS node name
 * prepended with a unique string.
 */

#define LED_HANDLER(id)	DT_CAT(L_H_, id)
#define LED_ACTION(id)	DT_CAT(L_A_, id)
#define LED_HAND_TAB(id)	DT_CAT(L_H_T_, id)
#define LED_ACTION_TAB(id)	DT_CAT(L_A_T_, id)

#define GEN_HAND_ENUM(id)	LED_HANDLER(id),

/*
 * Enum representing the multi-PWM LED handlers.
 * One is created for each instance of the multi-PWM LED
 * handlers.
 */
enum led_hand {
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_HAND_ENUM)
#endif
	LED_HAND_COUNT
};

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)

/*
 * Generate enums for the action indices for
 * each of the LED handlers. These are not typed
 * as they are only used internally as an index.
 */
#define GEN_ACTION_ENUM(id)	LED_ACTION(id),

#define GEN_ACTION_ENUM_LIST(id)					\
enum {								\
	DT_FOREACH_CHILD(id, GEN_ACTION_ENUM)			\
};

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_ACTION_ENUM_LIST)

/*
 * Generate arrays containing the LED action
 * configuration. Separate arrays are generated for
 * each set of child nodes belonging to separate
 * multi-PWM LEDs, and named using the node name so
 * they can be reference later.
 */
#define GEN_ACTION_TAB_ENTRY(id)				\
{								\
	.colors = DT_PROP(id, colors),				\
	.blink = DT_PROP(id, blink),				\
},

#define GEN_ACTION_TABLE(id)					\
static const struct led_action LED_ACTION_TAB(id)[] = {		\
	DT_FOREACH_CHILD(id, GEN_ACTION_TAB_ENTRY)		\
};

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_ACTION_TABLE)

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
 * Generate handler structures. These represent the
 * top level multi-PWM configuration, and contains
 * references to the LED actions and the list
 * of PWMs for this handler.
 */

#define GEN_HAND_TABLE(id)					\
{								\
	.frequency = DT_PROP(id, frequency),			\
	.pwm_count = DT_PROP_LEN(id, pwms),			\
	.action_count = ARRAY_SIZE(LED_ACTION_TAB(id)),		\
	.pwms = LED_HAND_TAB(id),				\
	.actions = LED_ACTION_TAB(id),				\
},

static const struct led_multi_pwm pwm_hand[] = {
DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_HAND_TABLE)
};

/*
 * Contains the run-time status of the handlers.
 */
static struct led_multi_pwm_state pwm_state[ARRAY_SIZE(pwm_hand)];

/*
 * Update the LED PWM settings using the color values provided.
 * The color values are defined as a percentage (0-100), and
 * this is used to calculate the duty cycle time of the PWM.
 */
static void set_pwm_led_values(const struct led_multi_pwm *pwm,
			       const uint8_t *colors)
{
	uint32_t period = 1000*1000; /* one second */
	const struct pwm_led *pl = pwm->pwms;

	for (int i = 0; i < pwm->pwm_count; i++, pl++, colors++) {
		/*
		 * Calculate PWM percentage duty cycle.
		 */
		uint32_t pulse = *colors * (1000*1000/100);

		pwm_pin_set_usec(pl->pwm, pl->chan, period,
				 pulse, pl->flags);
	}
}

/*
 * BLink timer callback, flip the state of the LEDs and
 * restart the timer.
 */
static void pwm_led_timer(struct k_timer *timer)
{
	struct led_multi_pwm_state *state = k_timer_user_data_get(timer);
	int index = state - pwm_state;
	const struct led_multi_pwm *pwm = &pwm_hand[index];
	const struct led_action *ap = &pwm->actions[state->current_action];
	static const uint8_t off[4];	/* off values of 0 */

	if (state->current_state) {
		state->current_state = 0;
		k_timer_start(&state->timer, K_MSEC(ap->blink[0]),
			      K_FOREVER);
		set_pwm_led_values(pwm, ap->colors);
	} else {
		state->current_state = 1;
		k_timer_start(&state->timer, K_MSEC(ap->blink[1]),
			      K_FOREVER);
		set_pwm_led_values(pwm, off);
	}
}

/*
 * Initialise the state of the PWM LED handler.
 */
static void pwm_led_init(void)
{
	/*
	 * Initialise timer.
	 */
	for (int i = 0; i < ARRAY_SIZE(pwm_state); i++) {
		k_timer_init(&pwm_state[i].timer,
			     pwm_led_timer,
			     NULL);
		k_timer_user_data_set(&pwm_state[i].timer,
				      &pwm_state[i]);
	}
}

#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM) */

/*
 * Set the LED behaviour of this multi-PWM LED handler
 * to the action passed. The action defines the
 * duty cycle for each PWM (and thus the colour), and also
 * any blink settings.
 */
void pwm_set_led(enum led_hand h, int action)
{
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	const struct led_multi_pwm *pwm = &pwm_hand[h];
	struct led_multi_pwm_state *state = &pwm_state[h];
	const struct led_action *ap = &pwm->actions[action];
	/*
	 * Check whether it is the same action, and
	 * skip setting the PWMs if the same.
	 */
	if (action == state->current_action) {
		return;
	}
	state->current_action = action;
	/*
	 * Stop timer, if operational.
	 */
	k_timer_stop(&state->timer);
	set_pwm_led_values(pwm, ap->colors);
	/*
	 * Check whether timer needs to be enabled for blinking.
	 */
	if (ap->blink[0] && ap->blink[1]) {
		state->current_state = 0;
		/* One shot timer */
		k_timer_start(&state->timer, K_MSEC(ap->blink[0]),
			      K_FOREVER);
	}
#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM) */
}

/*
 * LED policy configuration. This is the second part of the
 * LED handling, which defines the policy input processing.
 *
 * The overall idea is that when changes occur such as the
 * state of the AP changing, the new state will be matched against
 * the different policy comparisons, and the first matching policy will
 * be used - each policy references a LED action, which defines for that
 * LED handler what it should do.
 */

/*
 * LED policy input structure. The policy is the input enum that
 * is used for matching.
 * The action is the action index for the LED handler associated with
 * this policy.
 * value is used as a parameter in matching, such as battery charge etc.
 */
struct led_policy_entry {
	uint8_t policy;
	uint8_t action;
	uint8_t value;
};

/*
 * The policy for a particular LED handler.
 * Each LED handler should have only a single policy associated
 * with it, which defines what the action that handler should take.
 */
struct led_policy {
	uint8_t led;	/* enum for this LED handler */
	uint8_t count;	/* The number of policy comparison entries */
	const struct led_policy_entry *entries;
};

/*
 * Each instance of a policy node (and child nodes) is associated
 * with a single LED handler. Each policy node has an array
 * of policy comparisons which are checked sequentially
 * and the first matching entry is used as the action for that
 * LED handler.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_POLICY)

/*
 * Inputs for LED policy control.
 * These inputs define the policy inputs used to
 * drive the LEDs.
 */
enum led_input {
	LED_INPUT_SUSPENDED,	/* AP is suspended */
	LED_INPUT_RUNNING,	/* AP is running */
	LED_INPUT_POWER_OFF,	/* AP is powered off */
	LED_INPUT_BATTERY,	/* Battery charge check */
};

/*
 * The current state of the AP expressed as a LED input enum.
 */
static uint8_t cpu_state = LED_INPUT_POWER_OFF;

/*
 * Define macros to map node names to identifiers for
 * the policy nodes and child nodes.
 */
#define POLICY_CHECK_TAB(id)	DT_CAT(P_C_T_, id)
#define POLICY_TAB(id)	DT_CAT(P_T_, id)

#define PCHECK_NAME(nm)	DT_CAT(LED_INPUT_, nm)

/*
 * Generate the policy comparison arrays.
 */
#define GEN_CHECK_TAB_ENTRY(id)					\
{								\
	.policy = PCHECK_NAME(DT_STRING_TOKEN(id, input)),	\
	.action = LED_ACTION(DT_PHANDLE(id, action)),		\
	.value = DT_PROP(id, value),				\
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
	.led = LED_HANDLER(DT_PHANDLE(id, led)),		\
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
 * Some event has occurred which may require updating of the
 * LEDs. Iterate through the LED policies and update
 * the LED handlers if necessary.
 */
static void update_leds(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_POLICY)
	for (int i = 0; i < ARRAY_SIZE(policy_table); i++) {
		/*
		 * Iterate through the policy checks for this
		 * policy and see if any match.
		 */
		const struct led_policy_entry *e =
			policy_table[i].entries;
		for (int j = 0; j < policy_table[i].count; j++, e++) {
			/*
			 * TODO: Check battery power.
			 */
			if (cpu_state == e->policy) {
				/*
				 * Found a matching policy.
				 * Call the LED handler with this
				 * action.
				 */
				pwm_set_led(policy_table[i].led,
					    e->action);
				break;
			}
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
		cpu_state = LED_INPUT_RUNNING;
		break;

	case AP_POWER_SUSPEND:
		cpu_state = LED_INPUT_SUSPENDED;
		break;

	case AP_POWER_SHUTDOWN:
		cpu_state = LED_INPUT_POWER_OFF;
		break;
	}
	update_leds();
}

/*
 * Initialise the LED processing.
 */
static int init_led(const struct device *unused)
{
	static struct ap_power_ev_callback cb;

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
	pwm_led_init();
#endif
	update_leds();
	ap_power_ev_init_callback(&cb, cpu_update,
				  AP_POWER_RESUME |
				  AP_POWER_SUSPEND |
				  AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);
	return 0;
}

SYS_INIT(init_led, APPLICATION, 1);
