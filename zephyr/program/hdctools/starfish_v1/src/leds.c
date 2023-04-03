/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio_helper.h"
#include "sim_button.h"
#include "sim.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(leds);

enum LED_LIGHT_MODE {
	LED_LIGHT_OFF,
	LED_LIGHT_ON,
	LED_LIGHT_SLOT_TURNING_OFF,
	LED_LIGHT_SLOT_DESIRED,
};

struct led_cycle {
	/* Cycle period in milliseconds, 0 indicates constant. */
	int period;
	/* Duration in milliseconds LED is on each cycle. */
	int on_time;
};

struct led_ctx {
	/* GPIO Context */
	struct gpio_ctx gpio;
	/* LED Mode. */
	enum LED_LIGHT_MODE light;
};

static struct led_ctx all_leds[] = { GPIO_LIST_CTX(led) };

static void wakeup(void) {
	extern const k_tid_t led_task_id;
	k_wakeup(led_task_id);
}

static void set_led_state(enum GPIO_LABEL label, int idx,
			  enum LED_LIGHT_MODE light)
{
	for (int i = 0; i < ARRAY_SIZE(all_leds); i++) {
		if (all_leds[i].gpio.label != label) {
			continue;
		}
		if (all_leds[i].gpio.idx != idx) {
			continue;
		}
		all_leds[i].light = light;
	}
}

/*
 * Turns the leds on or off based on their state.
 * 
 * If an LED is in the static state then 
 */
static uint32_t ctrl_led(struct led_ctx *led, uint32_t timestamp)
{
	uint32_t remainder = 0;
	uint32_t next_transition;
	const struct led_cycle led_sequence[] = {
		[LED_LIGHT_OFF] = { 0, 0 },
		[LED_LIGHT_ON] = { 0, 1 },
		[LED_LIGHT_SLOT_TURNING_OFF] = { 1000, 200 },
		[LED_LIGHT_SLOT_DESIRED] = { 1000, 800 },
	};
	if (led->light > ARRAY_SIZE(led_sequence)) {
		led->light = LED_LIGHT_OFF;
	}
	struct led_cycle cycle = led_sequence[led->light];
	if (cycle.period) {
		remainder = timestamp % cycle.period;
	}
	if (remainder < cycle.on_time) {
		gpio_pin_set_dt(&led->gpio.spec, 1);
		next_transition = cycle.on_time - remainder;
	} else {
		gpio_pin_set_dt(&led->gpio.spec, 0);
		next_transition = cycle.period - remainder;
	}
	if (!cycle.period) {
		return UINT32_MAX;
	} else {
		return next_transition;
	}
}

void sim_led_update(void) {
	if (sim_button_state()->enabled) {
		set_led_state(GPIO_LABEL_LED_MODE, 0, LED_LIGHT_OFF);
		set_led_state(GPIO_LABEL_LED_MODE, 1, LED_LIGHT_ON);
	} else {
		set_led_state(GPIO_LABEL_LED_MODE, 0, LED_LIGHT_ON);
		set_led_state(GPIO_LABEL_LED_MODE, 1, LED_LIGHT_OFF);
	}
	for (int i = 0; i < 8; i++) {
		set_led_state(GPIO_LABEL_LED_SLOT, i, LED_LIGHT_OFF);
	}
	const struct mux_state *current = sim_slot_get_current_mux();
	const struct mux_state *desired = sim_slot_get_desired_mux();

	if (desired->enabled) {
		set_led_state(GPIO_LABEL_LED_SLOT, desired->slot_number, LED_LIGHT_SLOT_DESIRED);
	}
	if (current->enabled) {
		set_led_state(GPIO_LABEL_LED_SLOT, current->slot_number, LED_LIGHT_ON);
	}
	wakeup();
}

static int led_init(void)
{
	for (int i = 0; i < ARRAY_SIZE(all_leds); i++) {
		gpio_pin_configure_dt(&all_leds[i].gpio.spec, GPIO_OUTPUT_HIGH);
	}
	sim_led_update();
	return 0;
}

static void led_task(void)
{
	while (true) {
		uint32_t timestamp = k_uptime_get_32();
		uint32_t sleep_time = 50;
		sim_led_update();
		for (int i = 0; i < ARRAY_SIZE(all_leds); i++) {
			uint32_t next_transition = ctrl_led(&all_leds[i], timestamp);
			sleep_time = MIN(sleep_time, next_transition);
		}
		k_msleep(sleep_time);
	}
}

SYS_INIT(led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

K_THREAD_DEFINE(led_task_id, 1000, led_task, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);


