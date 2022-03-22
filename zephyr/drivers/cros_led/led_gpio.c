/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <init.h>
#include <drivers/gpio.h>

#include "led_common.h"

#include "led.h"
#include "led_gpio.h"

#include <logging/log.h>

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO)

LOG_MODULE_DECLARE(led, LOG_LEVEL_ERR);

/*
 * LED handlers that drive LED via GPIOs, defined
 * via the cros_ec_gpio_leds compatible.
 */

/*
 * Led action, which holds a list of colours/durations.
 * The colors/durations are stored as a byte array.
 * Each set of colour and duration values have the first
 * bytes as the off/on (0,1) settings for the GPIOs, matching
 * the number of GPIOs, and the next byte is a duration (in tenths of a second)
 * If there is more than a single set of colours/duration,
 * the colours are cycled through, with the duration
 * indicating how long to pause before moving to the next
 * colour. If there is only one set of colour/duration values,
 * the duration is ignored, and no cycling is done.
 *
 * So for example if there are 2 GPIOs set up as BLUE and YELLOW outputs,
 * the colors/duration sets would be 3 bytes each,
 * the first 2 bytes being the 0 or 1 GPIO outputs, and the third the
 * duration value.
 */
struct led_gpio_action {
	const uint8_t *colors;
	uint8_t color_count;	/* Number of colours/duration entries */
};

/*
 * GPIO LED handler read-only configuration.
 * Stores the DTS originated configuration for each
 * handler for the GPIO LED driver.
 */
struct led_gpio {
	uint8_t gpio_count;	/* Number of GPIOs */
	uint8_t action_count;	/* Number of LED actions */
	const struct gpio_dt_spec *gpios;
	const struct led_gpio_action *actions;
	const uint8_t *color_map;
};

/*
 * GPIO r/w state.
 * Stores the current state of the GPIO LED handler.
 * Used to hold the timer for blinking.
 */
struct led_gpio_state {
	struct k_timer timer;
	uint8_t current_action;
	uint8_t current_color;
};

#define LED_GPIO_COLOR_LIST(id)	DT_CAT(L_C_L_, id)
#define LED_GPIO_COLOR_MAP(id)	DT_CAT(L_C_M_, id)

/*
 * Generate byte arrays containing the LED colour
 * configuration. Separate arrays are generated for
 * each child node belonging to separate GPIO LEDs,
 * and named using the node name so they can be referenced later.
 */
#define GPIO_COUNT(id)	DT_PROP_LEN(id, gpios)
#define GEN_GPIO_COLOR_LIST(id)				\
static const uint8_t LED_GPIO_COLOR_LIST(id)[] =	\
	DT_PROP(id, colors)				\
;							\
BUILD_ASSERT((sizeof(LED_GPIO_COLOR_LIST(id)) %		\
	     (GPIO_COUNT(DT_PARENT(id)) + 1)) == 0,	\
	     "Wrong number of color values");

#define GEN_GPIO_COLOR_LISTS(id)				\
	DT_FOREACH_CHILD(id, GEN_GPIO_COLOR_LIST)	\

DT_FOREACH_STATUS_OKAY(COMPAT_GPIO, GEN_GPIO_COLOR_LISTS)

/*
 * Generate a led_gpio_action array, with one entry for
 * each colors list.
 */
#define GEN_GPIO_ACTION_LIST(id)				\
{								\
	.colors = LED_GPIO_COLOR_LIST(id),			\
	.color_count = (sizeof(LED_GPIO_COLOR_LIST(id))		\
			/ (GPIO_COUNT(DT_PARENT(id)) + 1)),	\
},

#define GEN_GPIO_ACTION_TAB(id)					\
static const struct led_gpio_action LED_ACTION_TAB(id)[] = {		\
	DT_FOREACH_CHILD(id, GEN_GPIO_ACTION_LIST)			\
};

DT_FOREACH_STATUS_OKAY(COMPAT_GPIO, GEN_GPIO_ACTION_TAB)

/*
 * Generate the GPIO list for each handler.
 * This list captures the GPIO configuration for
 * each GPIO used to drive the LEDs.
 * The list is named using a unique name based on the node name
 * so that it can be referenced later.
 */
#define GEN_GPIO_ENTRY(id, p, idx)				\
	GPIO_DT_SPEC_GET_BY_IDX(id, p, idx),			\

#define GEN_GPIO_TABLE(id)					\
static const struct gpio_dt_spec LED_HAND_TAB(id)[] = {		\
	DT_FOREACH_PROP_ELEM(id, gpios, GEN_GPIO_ENTRY)		\
};

DT_FOREACH_STATUS_OKAY(COMPAT_GPIO, GEN_GPIO_TABLE)

/*
 * Generate byte array holding color map for
 * EC host command brightness mapping.
 */
#define GEN_GPIO_COLOR_MAP(id)					\
static const uint8_t LED_GPIO_COLOR_MAP(id)[] = DT_PROP(id, color_list);

DT_FOREACH_STATUS_OKAY(COMPAT_GPIO, GEN_GPIO_COLOR_MAP)

/*
 * Generate handler structures. These represent the
 * top level GPIO LED configuration, and contains
 * references to the LED actions and the list
 * of GPIOs for each handler.
 */
#define GEN_HAND_TABLE(id)					\
{								\
	.gpio_count = DT_PROP_LEN(id, gpios),			\
	.action_count = ARRAY_SIZE(LED_ACTION_TAB(id)),		\
	.gpios = LED_HAND_TAB(id),				\
	.actions = LED_ACTION_TAB(id),				\
	.color_map = LED_GPIO_COLOR_MAP(id),			\
},

static const struct led_gpio gpio_hand[] = {
DT_FOREACH_STATUS_OKAY(COMPAT_GPIO, GEN_HAND_TABLE)
};

/*
 * Contains the run-time status of the handlers,
 * such as the current action, and the current color index
 * in the event of colors being cycled (blinking etc.)
 */
static struct led_gpio_state gpio_state[ARRAY_SIZE(gpio_hand)];

/*
 * Update the LED GPIO settings using the GPIO outputs provided.
 */
static void set_gpio_led_values(const struct led_gpio *gpio,
				const uint8_t *colors)
{
	const struct gpio_dt_spec *gp = gpio->gpios;

	for (int i = 0; i < gpio->gpio_count; i++, gp++) {
		gpio_pin_set_dt(gp, *colors++);
	}
}

/*
 * Colour cycle timer callback. Move to the next colour,
 * wrapping to the beginning when finished, and restart the timer
 * with the new duration.
 */
static void gpio_led_timer(struct k_timer *timer)
{
	struct led_gpio_state *state = k_timer_user_data_get(timer);
	const struct led_gpio *gpio = &gpio_hand[state - gpio_state];
	const struct led_gpio_action *ap =
		&gpio->actions[state->current_action];
	const uint8_t *cp;

	/*
	 * Cycle to next colour in list. The number of colours matches
	 * the number of GPIOs, and there is one more value for the
	 * duration.
	 */
	state->current_color++;
	if (state->current_color >= ap->color_count) {
		state->current_color = 0;
	}
	cp = &ap->colors[state->current_color * (gpio->gpio_count + 1)];
	k_timer_start(&state->timer, D_TICKS(cp[gpio->gpio_count]), K_FOREVER);
	set_gpio_led_values(gpio, cp);
}

void gpio_get_led_brightness(enum led_gpio_hand h, uint8_t *br)
{
	const struct led_gpio *gpio = &gpio_hand[h];
	const uint8_t *cmp = gpio->color_map;
	/*
	 * Walk through the color_map for the GPIO LED and
	 * set the brightness range to 1 for each
	 * of the supported colors.
	 */
	for (int i = 0; i < gpio->gpio_count; i++) {
		br[cmp[i]] = 1;
	}
}

void gpio_set_led_brightness(enum led_gpio_hand h, const uint8_t *br)
{
	uint8_t colors[4];
	const struct led_gpio *gpio = &gpio_hand[h];
	const uint8_t *cmp = gpio->color_map;
	/*
	 * Walk through the color_map for the GPIO LED and
	 * set the GPIO value to 0 or 1.
	 */
	for (int i = 0; i < gpio->gpio_count; i++) {
		colors[i] = br[cmp[i]];
	}
	set_gpio_led_values(gpio, colors);
}

/*
 * Set the LED behaviour of this GPIO LED handler
 * to the action passed. The action identifies the list
 * of colours and duration (if any).
 */
void gpio_set_led_action(enum led_gpio_hand h, int action)
{
	struct led_gpio_state *state = &gpio_state[h];
	const struct led_gpio *gpio;
	const struct led_gpio_action *ap;

	/*
	 * Check whether it is the same action, and
	 * skip setting the PWMs if the action hasn't changed.
	 */
	if (action == state->current_action) {
		return;
	}
	state->current_action = action;
	gpio = &gpio_hand[h];
	ap = &gpio->actions[action];
	/*
	 * Stop timer. No-op if timer is already stopped.
	 */
	k_timer_stop(&state->timer);
	/*
	 * Set GPIO values.
	 */
	set_gpio_led_values(&gpio_hand[h], ap->colors);
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
void gpio_led_shutdown(enum led_gpio_hand h)
{
	struct led_gpio_state *state = &gpio_state[h];

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
 * Initialise the runtime state of the GPIO LED handler.
 */
void gpio_led_init(void)
{
	/*
	 * Initialise timer and configure GPIOs.
	 */
	for (int i = 0; i < ARRAY_SIZE(gpio_hand); i++) {
		const struct gpio_dt_spec *gp = gpio_hand[i].gpios;

		for (int j = 0; j < gpio_hand[i].gpio_count; j++, gp++) {
			gpio_pin_configure_dt(gp, GPIO_OUTPUT_LOW);
		}
		k_timer_init(&gpio_state[i].timer,
			     gpio_led_timer,
			     NULL);
		k_timer_user_data_set(&gpio_state[i].timer,
				      &gpio_state[i]);
		gpio_state[i].current_action = 0xFF;
	}
}
#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_GPIO) */
