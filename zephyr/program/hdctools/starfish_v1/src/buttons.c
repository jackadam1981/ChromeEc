/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Defines a software button debounce interface:
 *
 * Multiple buttons can be pressed at one time and buttons can generate
 * short or long press events.
 *
 * When a button is pressed, we mark the button as BUTTON_HELD and
 * increment the total time pressed markers. The BUTTON_HELD state
 * from any button keeps the task in a higher power mode to sample quickly.
 *
 * Releasing a button which was in the BUTTON_HELD state starts a timer
 * recording the duration released. Once it has reached BUTTON_RELEASED_MS
 * we will process the button event and identify if the button was held for
 * long enough to be a short or long press.
 *
 * The button task will then service the buttons by recording the events,
 * which button they came from, and then clearing the state.
 */

#include "buttons.h"

#include "sim_button.h"

#include <zephyr/shell/shell.h>

#define SLEEP_ACTIVE_MS 50
#define SLEEP_INACTIVE_MS 10000

#define MAX_RELEASED_MS 50
#define MAX_PRESSED_MS 5000

#define BUTTON_PRESS_SHORT_MS 200
#define BUTTON_PRESS_LONG_MS 2000

LOG_MODULE_REGISTER(button, LOG_LEVEL_DBG);

struct button_ctx {
	/* GPIO Context */
	struct gpio_ctx gpio;
	/* GPIO callback */
	struct gpio_callback cb;
	/* Stores GPIO's last pressed state so can handle interrupts. */
	bool gpio_pressed;
	/* Current button state. */
	enum BUTTON_STATE state;
	/* Number of milliseconds the button was released. */
	unsigned int released_ms;
	/* Number of milliseconds the button was pressed.*/
	unsigned int pressed_ms;
	/* Length of longest press.*/
	unsigned int max_pressed_ms;
};

/* Generates all button state-machines using the device tree. */
static struct button_ctx all_buttons[] = { GPIO_LIST_CTX(button) };

static void button_wakup() {
	extern const k_tid_t button_thread_id;
	k_wakeup(button_thread_id);
}


/*
 * Interrupt callback which wakes the button thread on either edge.
 */
static void callback(const struct device *dev, struct gpio_callback *gpio_cb,
		     uint32_t pins)
{
	button_wakup();
}

/*
 * Initialize the GPIO and context states for a button
 */
static void configure_button(struct button_ctx *ctx)
{
	gpio_pin_configure_dt(&ctx->gpio.spec, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&ctx->gpio.spec, GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&ctx->cb, callback, BIT(ctx->gpio.spec.pin));
	gpio_add_callback(ctx->gpio.spec.port, &ctx->cb);
	ctx->gpio_pressed = false;
	ctx->state = BUTTON_INACTIVE;
	ctx->max_pressed_ms = 0;
	ctx->pressed_ms = 0;
	ctx->released_ms = 0;
}

/*
 * Handle GPIO Button Off -> On transition
 *
 * Marks the start of a press event.
 */
static void handle_press(struct button_ctx *ctx, int delta_ms)
{
	ctx->released_ms = 0;
	ctx->pressed_ms = 0;
	ctx->state = BUTTON_HELD;
	LOG_DBG("GPIO:%d, Pressed", ctx->gpio.label);
}

/*
 * Handle GPIO Button Off -> On state
 *
 * Record the time the button has been held down.
 */
static void handle_hold(struct button_ctx *ctx, int delta_ms)
{
	ctx->pressed_ms += delta_ms;
	ctx->pressed_ms = MIN(ctx->pressed_ms, MAX_PRESSED_MS);
}

/*
 * Handle GPIO Button On -> Off transition
 *
 * Records the maximum hold time.
 */
static void handle_release(struct button_ctx *ctx, int delta_ms)
{
	ctx->max_pressed_ms = MAX(ctx->pressed_ms, ctx->max_pressed_ms);
	ctx->pressed_ms = 0;
	LOG_DBG("GPIO:%d, Released", ctx->gpio.label);
}

/*
 * Handle GPIO Button Off -> Off state
 *
 * Identifies the new button state after the MAX_RELEASED_MS timeout.
 */
static void handle_inactive(struct button_ctx *ctx, int delta_ms)
{
	if (ctx->state != BUTTON_HELD) {
		return;
	}
	ctx->released_ms += delta_ms;
	if (ctx->released_ms < MAX_RELEASED_MS) {
		return;
	}
	if (ctx->max_pressed_ms >= BUTTON_PRESS_LONG_MS) {
		ctx->state = BUTTON_LONG;
	} else if (ctx->max_pressed_ms >= BUTTON_PRESS_SHORT_MS) {
		ctx->state = BUTTON_SHORT;
	} else {
		ctx->state = BUTTON_INACTIVE;
	}
	LOG_DBG("GPIO:%d, State:%d", ctx->gpio.label, ctx->state);
}

/*
 * Updates the button state.
 *
 * Queries the GPIO state and compares against the last value to identify
 * the state transition required.
 */
static void button_update(struct button_ctx *ctx, int delta_ms)
{
	/* After the interrupt fires, need to know the last state. */
	bool prior_pressed = ctx->gpio_pressed;

	ctx->gpio_pressed = gpio_pin_get_dt(&ctx->gpio.spec) != 0;

	if (ctx->gpio_pressed) {
		if (prior_pressed) {
			handle_hold(ctx, delta_ms);
		} else {
			handle_press(ctx, delta_ms);
		}
	} else {
		if (prior_pressed) {
			handle_release(ctx, delta_ms);
		} else {
			handle_inactive(ctx, delta_ms);
		}
	}
}

/*
 * Clears the button press indicating we have processed the event.
 */
static void button_clear_press(struct button_ctx *ctx)
{
	ctx->max_pressed_ms = 0;
	ctx->state = BUTTON_INACTIVE;
}

static void button_thread(void)
{
	int64_t last_ts = k_uptime_get();
	int delta_ms = 0;

	for (int i = 0; i < ARRAY_SIZE(all_buttons); i++) {
		configure_button(&all_buttons[i]);
	}

	while (true) {
		int64_t ts = k_uptime_get();
		int event_count = 0;
		int held_count = 0;
		struct button_ctx *events[ARRAY_SIZE(all_buttons)];

		/* Time delta times */
		delta_ms = (int)(ts - last_ts);
		last_ts = ts;

		/* Fetch all button states and compute button_ctx states. */
		for (int i = 0; i < ARRAY_SIZE(all_buttons); i++) {
			struct button_ctx *cur = &all_buttons[i];

			button_update(cur, delta_ms);
			if (cur->state >= BUTTON_PENDING_EVENT_START) {
				events[event_count] = cur;
				event_count++;
			}
			if (cur->state == BUTTON_HELD) {
				held_count++;
			}
		}
		/* All buttons have been released, handle presses */
		if (held_count == 0) {
			/* Clear any presses */
			for (int i = 0; i < event_count; i++) {
				struct button_ctx *cur = events[i];
				if (event_count == 1) {
					sim_button_event(cur->gpio.label, cur->state);
				}
				button_clear_press(cur);
			}
		}
		if (event_count || held_count) {
			k_msleep(SLEEP_ACTIVE_MS);
		} else {
			k_msleep(SLEEP_INACTIVE_MS);
		}
	}
}

K_THREAD_DEFINE(button_thread_id, 2000, button_thread, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

