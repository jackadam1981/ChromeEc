/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2024 TOKITA Hiroshi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led_strip_demo);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#define STRIP_NODE		DT_ALIAS(led_strip)

#if DT_NODE_HAS_PROP(DT_ALIAS(led_strip), chain_length)
#define STRIP_NUM_PIXELS	DT_PROP(DT_ALIAS(led_strip), chain_length)
#else
#error Unable to determine length of LED strip
#endif

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

/* --- Global variables --- */

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixels[STRIP_NUM_PIXELS];

static bool effect_on = true;

/* --- Animation Functions --- */

/**
 * @brief Sets all pixels to a single color and updates the strip.
 *
 * @param color The color to set.
 * @param delay Time to hold the color.
 */
static void set_solid_color_unblock(const struct led_rgb *color, k_timeout_t delay)
{
	for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
		memcpy(&pixels[i], color, sizeof(struct led_rgb));
	}
	led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
	k_sleep(delay);
}

static void set_solid_color(const struct led_rgb *color, k_timeout_t delay)
{
	if (!effect_on)
		return;

	set_solid_color_unblock(color, delay);
}

/**
 * @brief Fades the entire strip through a color wheel.
 *
 * @param steps Number of steps in the color wheel.
 * @param delay Time between each step.
 */
static void pattern_full_fade(uint32_t steps, k_timeout_t delay)
{
	struct led_rgb color;
	uint8_t phase;

	for (uint32_t i = 0; i < steps; i++) {
		phase = (i * 255) / steps;

		// Simple RGB fade: R -> G -> B -> R
		if (phase < 85) {
			color = (struct led_rgb){ .r = 255 - phase * 3, .g = phase * 3, .b = 0 };
		} else if (phase < 170) {
			phase -= 85;
			color = (struct led_rgb){ .r = 0, .g = 255 - phase * 3, .b = phase * 3 };
		} else {
			phase -= 170;
			color = (struct led_rgb){ .r = phase * 3, .g = 0, .b = 255 - phase * 3 };
		}

		set_solid_color(&color, delay);
	}
}

/**
 * @brief Wipes a color across the strip from left to right.
 *
 * @param color The color to wipe.
 * @param delay Time between each pixel update.
 */
static void pattern_color_wipe(const struct led_rgb *color, k_timeout_t delay)
{
	if (!effect_on)
		return;

	for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
		memcpy(&pixels[i], color, sizeof(struct led_rgb));
		led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		k_sleep(delay);
	}
}

/* --- Main LED Thread --- */

int effect_count;

void led_strip_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (!device_is_ready(strip)) {
		LOG_ERR("Device %s is not ready", strip->name);
		return;
	}

	LOG_INF("LED strip demo thread started");

	const struct led_rgb red    = RGB(0xFF, 0x00, 0x00);
	const struct led_rgb green  = RGB(0x00, 0xFF, 0x00);
	const struct led_rgb blue   = RGB(0x00, 0x00, 0xFF);
	const struct led_rgb white  = RGB(0xFF, 0xFF, 0xFF);
	const struct led_rgb black  = RGB(0x00, 0x00, 0x00);

	// Loop until the 'off' command suspends us.
	while (1) {
		if (!effect_on) {
			k_sleep(K_MSEC(100));
			continue;
		}
		effect_count++;

		/* Pattern 1: Cycle through solid colors */
		LOG_INF("Pattern: Solid Colors");
		set_solid_color(&white, K_SECONDS(1));
		set_solid_color(&red, K_SECONDS(1));
		set_solid_color(&green, K_SECONDS(1));
		set_solid_color(&blue, K_SECONDS(1));

		/* Pattern 2: Fade all LEDs through the spectrum */
		LOG_INF("Pattern: Full Fade");
		pattern_full_fade(256, K_MSEC(20));
		set_solid_color(&black, K_MSEC(500)); // Pause between patterns

		/* Pattern 3: Wipe colors across the strip */
		LOG_INF("Pattern: Color Wipe");
		pattern_color_wipe(&red, K_MSEC(50));
		pattern_color_wipe(&green, K_MSEC(50));
		pattern_color_wipe(&blue, K_MSEC(50));
		pattern_color_wipe(&white, K_MSEC(50));
		set_solid_color(&black, K_MSEC(500)); // Pause
	}
}

K_THREAD_DEFINE(
    led_strip_tid,                          // 1. Thread's ID (a new variable name)
    2048,                                   // 2. Stack size in bytes
    led_strip_thread,                       // 3. The function to run
    NULL, NULL, NULL,                       // 4. Three optional parameters for the function
    K_HIGHEST_THREAD_PRIO,                  // 5. Thread priority (0=highest, 15=lowest)
    0,                                      // 6. Thread options (e.g., K_FP_REGS for floating point)
    0                                       // 7. Delay to start after boot (in ms)
);


/* --- Shell Command Implementation --- */

static int cmd_effect_count(const struct shell *sh)
{
	shell_print(sh, "Count=%d; ready=%d", effect_count, device_is_ready(strip) ? 1 : 0);
	return 0;
}

static int cmd_effect_on(const struct shell *sh)
{
	if (!effect_on) {
		effect_on = true;
		shell_print(sh, "LED animation started.");
	} else {
		shell_print(sh, "LED animation is already running.");
	}
	return 0;
}

static int cmd_effect_off(const struct shell *sh)
{
	if (effect_on) {
		effect_on = false;
		shell_print(sh, "LED animation stopping...");
		k_sleep(K_MSEC(100));
	} else {
		shell_print(sh, "LED animation is already off.");
	}

	// Turn all LEDs off
	const struct led_rgb black = RGB(0x00, 0x00, 0x00);
	set_solid_color(&black, K_NO_WAIT);

	return 0;
}

static int cmd_effect_set(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 4) {
		shell_error(sh, "Usage: set <r> <g> <b> (values 0-255)");
		return -EINVAL;
	}

	// Stop any running animation first
	cmd_effect_off(sh);

	struct led_rgb color = {
		.r = (uint8_t)strtol(argv[1], NULL, 0),
		.g = (uint8_t)strtol(argv[2], NULL, 0),
		.b = (uint8_t)strtol(argv[3], NULL, 0),
	};

	shell_print(sh, "Setting strip to R:%d G:%d B:%d",
		    color.r, color.g, color.b);
	set_solid_color_unblock(&color, K_NO_WAIT);

	return 0;
}

/* Create the shell command structure */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_effect,
	SHELL_CMD(count, NULL, "Show the counter.",
		  (void *)cmd_effect_count),
	SHELL_CMD(on, NULL, "Start the LED animation cycle.",
		  (void *)cmd_effect_on),
	SHELL_CMD(off, NULL, "Stop the LED animation and turn off LEDs.",
		  (void *)cmd_effect_off),
	SHELL_CMD_ARG(set, NULL, "Set a solid color: set <r> <g> <b>",
		      cmd_effect_set, 4, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(effect, &sub_effect, "LED effect control commands",
		   NULL);
