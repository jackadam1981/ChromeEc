/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <init.h>
#include <drivers/pwm.h>

#include "led_common.h"

#include "led.h"
#include "led_pwm.h"

#include <logging/log.h>

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)

LOG_MODULE_DECLARE(led, LOG_LEVEL_ERR);

/*
 * LED handlers that drive LED via PWM hardware, defined
 * via the cros_ec_multi_pwm_leds compatible.
 */

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
 * The colors/durations are stored as a byte array.
 * Each set of colour and duration values have the first
 * bytes as the colour values (as a percentage 0-100) matching
 * the number of PWMs, and the next byte is a duration (in tenths of a second)
 * If there is more than a single set of colours/duration,
 * the colours are cycled through, with the duration
 * indicating how long to pause before moving to the next
 * colour. If there is only one set of colour/duration values,
 * the duration is ignored, and no cycling is done.
 *
 * So typically there would be 3 PWMs set up as a RGB outputs to
 * a multi-colour LED.
 * In this case, the colors/duration sets would be 4 bytes each,
 * the first 3 bytes being the RGB values, and the fourth the
 * duration value.
 */
struct led_pwm_action {
	const uint8_t *colors;
	uint8_t color_count;	/* Number of colours/duration entries */
};

/*
 * Multi-PWM LED handler read-only configuration.
 * Stores the DTS originated configuration for each
 * handler for the multi-PWM LED driver.
 */
struct led_multi_pwm {
	uint32_t period_us;	/* Period in microseconds */
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
};

#define LED_PWM_COLOR_LIST(id)	DT_CAT(L_C_L_, id)
#define LED_PWM_COLOR_MAP(id)	DT_CAT(L_C_M_, id)

/*
 * Generate byte arrays containing the LED colour
 * configuration. Separate arrays are generated for
 * each child node belonging to separate multi-PWM LEDs,
 * and named using the node name so they can be referenced later.
 */
#define PWM_COUNT(id)	DT_PROP_LEN(id, pwms)
#define GEN_PWM_COLOR_LIST(id)				\
static const uint8_t LED_PWM_COLOR_LIST(id)[] =		\
	DT_PROP(id, colors)				\
;							\
BUILD_ASSERT((sizeof(LED_PWM_COLOR_LIST(id)) %		\
	     (PWM_COUNT(DT_PARENT(id)) + 1)) == 0,	\
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
	.colors = LED_PWM_COLOR_LIST(id),			\
	.color_count = (sizeof(LED_PWM_COLOR_LIST(id))		\
			/ (PWM_COUNT(DT_PARENT(id)) + 1)),	\
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
	.period_us = USEC_PER_SEC / DT_PROP(id, frequency),	\
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
	const struct pwm_led *pl = pwm->pwms;

	for (int i = 0; i < pwm->pwm_count; i++, pl++, colors++) {
		/*
		 * Calculate PWM percentage duty cycle.
		 */
		uint32_t pulse = *colors * pwm->period_us / 100;

		pwm_pin_set_usec(pl->pwm, pl->chan, pwm->period_us,
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
	 * Cycle to next colour in list. The number of colours matches
	 * the number of PWMs, and there is one more value for the
	 * duration. So for a typical PWM group consisting of 3
	 * PWMs representing RGB, there will be 4 bytes for each
	 * colour group, 3 bytes for RGB and a byte for the duration.
	 */
	state->current_color++;
	if (state->current_color >= ap->color_count) {
		state->current_color = 0;
	}
	cp = &ap->colors[state->current_color * (pwm->pwm_count + 1)];
	k_timer_start(&state->timer, D_TICKS(cp[pwm->pwm_count]), K_FOREVER);
	set_pwm_led_values(pwm, cp);
}

void pwm_get_led_brightness_max(enum led_pwm_hand h, uint8_t *br)
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

void pwm_set_led_brightness(enum led_pwm_hand h, const uint8_t *br)
{
	uint8_t colors[4];
	const struct led_multi_pwm *pwm = &pwm_hand[h];
	const uint8_t *cmp = pwm->color_map;
	/*
	 * Walk through the color_map for the PWM LED and
	 * set the channel according to the
	 * brightness range selected. The color_map
	 * entries are the indices of the supported colors
	 * in the brightness array. The range for
	 * the brightness is 0-255, and this is converted
	 * to 0-100.
	 */
	for (int i = 0; i < pwm->pwm_count; i++) {
		colors[i] = ((int)br[cmp[i]] * 100) / 255;
	}
	set_pwm_led_values(pwm, colors);
}

/*
 * Set the LED behaviour of this multi-PWM LED handler
 * to the action passed. The action identifies the list
 * of colours and duration (if any).
 */
void pwm_set_led_action(enum led_pwm_hand h, int action)
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

/*
 * If action enabled, clear it and stop the timer.
 */
void pwm_led_shutdown(enum led_pwm_hand h)
{
	struct led_multi_pwm_state *state = &pwm_state[h];

	if (state->current_action != 0xFF) {
		/*
		 * Stop the timer.
		 */
		k_timer_stop(&state->timer);
		/*
		 * Clear the action
		 */
		state->current_action = 0xFF;
	}
}

/*
 * Initialise the runtime state of the PWM LED handler.
 */
void pwm_led_init(void)
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
		pwm_state[i].current_action = 0xFF;
	}
}
#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM) */
