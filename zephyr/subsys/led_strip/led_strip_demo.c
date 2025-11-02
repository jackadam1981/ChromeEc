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
#include <zephyr/random/random.h>

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
	if (!K_TIMEOUT_EQ(delay, K_NO_WAIT)) {
		k_sleep(delay);
	}
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

	for (uint32_t i = 0; i < steps && effect_on; i++) {
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
 * @brief Helper to create a color from a 0-255 position on a color wheel.
 */
static void wheel(uint8_t wheel_pos, struct led_rgb *color)
{
	wheel_pos = 255 - wheel_pos;
	if (wheel_pos < 85) {
		*color = (struct led_rgb){255 - wheel_pos * 3, 0, wheel_pos * 3};
	} else if (wheel_pos < 170) {
		wheel_pos -= 85;
		*color = (struct led_rgb){0, wheel_pos * 3, 255 - wheel_pos * 3};
	} else {
		wheel_pos -= 170;
		*color = (struct led_rgb){wheel_pos * 3, 255 - wheel_pos * 3, 0};
	}
}

/**
 * @brief Displays a smoothly moving rainbow across the strip.
 */
static void pattern_rainbow_cycle(k_timeout_t delay)
{
	if (!effect_on) return;

	for (uint16_t j = 0; j < 256 * 4 && effect_on; j++) {
		for (uint16_t i = 0; i < STRIP_NUM_PIXELS; i++) {
			uint8_t wheel_pos = ((i * 256 / STRIP_NUM_PIXELS) + j) & 255;
			wheel(wheel_pos, &pixels[i]);
		}
		led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		k_sleep(delay);
	}
}

/**
 * @brief A comet effect with a fading tail.
 */
static void pattern_comet(const struct led_rgb *color, k_timeout_t delay)
{
	if (!effect_on) return;

	// First, clear the strip
	memset(pixels, 0, sizeof(pixels));

	for (int i = 0; i < STRIP_NUM_PIXELS * 2 && effect_on; i++) {
		// Fade all pixels by a small amount
		for (int j = 0; j < STRIP_NUM_PIXELS; j++) {
			pixels[j].r = MAX(0, pixels[j].r - 10);
			pixels[j].g = MAX(0, pixels[j].g - 10);
			pixels[j].b = MAX(0, pixels[j].b - 10);
		}

		// Draw the comet's head
		if (i < STRIP_NUM_PIXELS) {
			memcpy(&pixels[i], color, sizeof(struct led_rgb));
		}

		led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		k_sleep(delay);
	}
}

/**
 * @brief Randomly lights up pixels to create a twinkling effect.
 */
static void pattern_twinkle(const struct led_rgb *color, int count, k_timeout_t delay)
{
	if (!effect_on) return;

	memset(pixels, 0, sizeof(pixels));

	for (int i = 0; i < count && effect_on; i++) {
		uint32_t p = sys_rand32_get() % STRIP_NUM_PIXELS;
		memcpy(&pixels[p], color, sizeof(struct led_rgb));
		led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		k_sleep(delay);
		memset(&pixels[p], 0, sizeof(struct led_rgb)); // Turn it off
	}
}


/**
 * @brief Helper function for the fire effect.
 */
static void set_pixel_heat_color(int pixel, uint8_t temperature)
{
	// Scale 'heat' to 0-191 to avoid pure white, which doesn't look like fire
	uint8_t t192 = (temperature / 255.0) * 191;

	// calculate ramp up from
	uint8_t heat_ramp = t192 & 0x3F; // 0..63
	heat_ramp <<= 2; // scale up to 0..252

	// figure out which third of the spectrum we're in
	if (t192 > 0x80) { // hottest
		pixels[pixel] = (struct led_rgb){.r = 255, .g = 255, .b = heat_ramp};
	} else if (t192 > 0x40) { // middle
		pixels[pixel] = (struct led_rgb){.r = 255, .g = heat_ramp, .b = 0};
	} else { // coolest
		pixels[pixel] = (struct led_rgb){.r = heat_ramp, .g = 0, .b = 0};
	}
}

/**
 * @brief Simulates a fire effect.
 * @param cooling Cooling factor. A higher number means the fire cools faster. 50-100 is good.
 * @param sparking Sparking factor. A higher number means more sparks. 50-200 is good.
 * @param duration How long the effect should run.
 */
static void pattern_fire(int cooling, int sparking, k_timeout_t duration)
{
	if (!effect_on) return;

	static uint8_t heat[STRIP_NUM_PIXELS];
	int cooldown;
	int64_t start_time = k_uptime_get();

	while (k_uptime_get() - start_time < duration.ticks && effect_on) {
		// Step 1. Cool down every cell a little
		for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
			cooldown = sys_rand32_get() % (((cooling * 10) / STRIP_NUM_PIXELS) + 2);
			heat[i] = (heat[i] > cooldown) ? heat[i] - cooldown : 0;
		}

		// Step 2. Heat from each cell drifts 'up' and diffuses a little
		for (int k = STRIP_NUM_PIXELS - 1; k >= 2; k--) {
			heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
		}

		// Step 3. Randomly ignite new 'sparks' of heat near the bottom
		if ((sys_rand32_get() % 255) < sparking) {
			int y = sys_rand32_get() % 7;
			heat[y] = (heat[y] + ((sys_rand32_get() % 95) + 160)) % 256;
		}

		// Step 4. Map heat values to colors
		for (int j = 0; j < STRIP_NUM_PIXELS; j++) {
			set_pixel_heat_color(j, heat[j]);
		}

		led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		k_sleep(K_MSEC(20));
	}
}


/**
 * @brief Classic theater chase animation with rainbow colors.
 */
static void pattern_theater_chase_rainbow(k_timeout_t delay)
{
	if (!effect_on) return;

	for (int j = 0; j < 256 && effect_on; j += 4) { // 64 cycles of chasing
		for (int q = 0; q < 3; q++) {
			// Turn every third pixel on
			for (int i = 0; i < STRIP_NUM_PIXELS; i = i + 3) {
				wheel((i + j) % 255, &pixels[i + q]);
			}
			led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
			k_sleep(delay);

			// Turn every third pixel off
			for (int i = 0; i < STRIP_NUM_PIXELS; i = i + 3) {
				memset(&pixels[i + q], 0, sizeof(struct led_rgb));
			}
		}
	}
}

#if 0
/**
 * @brief Helper function to fade all pixels towards black.
 */
static void fade_all_pixels(uint8_t fade_amount)
{
	for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
		pixels[i].r = MAX(0, pixels[i].r - fade_amount);
		pixels[i].g = MAX(0, pixels[i].g - fade_amount);
		pixels[i].b = MAX(0, pixels[i].b - fade_amount);
	}
}

/**
 * @brief Draws a smooth, gradient-based pulse of light.
 *
 * The color is brightest at the center and fades to black at the edges.
 * Colors are added to the existing pixel values to allow for blending.
 *
 * @param center_pos_scaled The fixed-point center position of the pulse.
 * @param color The color of the pulse at its center.
 * @param width The number of pixels the pulse extends on either side of the center.
 */
static void draw_pulse(int32_t center_pos_scaled, const struct led_rgb *color, int width)
{
	const int32_t scale = 256;
	int center = center_pos_scaled / scale;

	for (int i = -width; i <= width; i++) {
		int pixel_idx = center + i;

		if (pixel_idx < 0 || pixel_idx >= STRIP_NUM_PIXELS) {
			continue;
		}

		// Calculate brightness based on distance from the center (linear falloff)
		int brightness = ((width - abs(i)) * 255) / width;

		// Add the pulse color, clamping at 255
		pixels[pixel_idx].r = MIN(255, pixels[pixel_idx].r + ((color->r * brightness) / 255));
		pixels[pixel_idx].g = MIN(255, pixels[pixel_idx].g + ((color->g * brightness) / 255));
		pixels[pixel_idx].b = MIN(255, pixels[pixel_idx].b + ((color->b * brightness) / 255));
	}
}

/**
 * @brief "Bouncing Pulses" effect with two interacting lights.
 *
 * @param color1 Color of the first pulse.
 * @param color2 Color of the second pulse.
 * @param duration How long the effect should run.
 */
static void pattern_bouncing_pulses(const struct led_rgb *color1, const struct led_rgb *color2, k_timeout_t duration)
{
	if (!effect_on) return;

	const struct led_rgb bg_color = RGB(0, 0, 10); // Dim blue background
	const struct led_rgb flash_color = RGB(255, 255, 255);
	const int delay_ms = 30;
	const int fade = 25;
	const int PULSE_WIDTH = STRIP_NUM_PIXELS / 8;

	// Use integer fixed-point math for position and speed (8 bits of fraction)
	const int32_t scale = 256;
	int32_t pos1 = 0;
	int32_t pos2 = (STRIP_NUM_PIXELS - 1) * scale;
	int32_t speed1 = scale / 2;
	int32_t speed2 = -speed1;

	// Integer-based breathing effect
	const int breath_max = 64;
	int breath_val = 0;
	int breath_dir = 1;

	int64_t start_time = k_uptime_get();

	while (k_uptime_get() - start_time < duration.ticks && effect_on) {
		// 1. Fade the whole strip to create trails from the last frame.
		fade_all_pixels(fade);

		// 2. Draw the breathing background, ensuring it doesn't erase brighter trails.
		struct led_rgb current_bg = { .r = (bg_color.r * breath_val) / breath_max,
					      .g = (bg_color.g * breath_val) / breath_max,
					      .b = (bg_color.b * breath_val) / breath_max };
		for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
			pixels[i].r = MAX(pixels[i].r, current_bg.r);
			pixels[i].g = MAX(pixels[i].g, current_bg.g);
			pixels[i].b = MAX(pixels[i].b, current_bg.b);
		}

		// Update breathing value for next frame
		breath_val += breath_dir;
		if (breath_val >= breath_max || breath_val <= 0) {
			breath_dir *= -1;
		}

		// Draw the two pulses
		draw_pulse(pos1, color1, PULSE_WIDTH);
		draw_pulse(pos2, color2, PULSE_WIDTH);

		// Move pulses
		pos1 += speed1;
		pos2 += speed2;

		// Collision detection and bounce
		if (pos1 >= pos2) {
			// Swap speeds
			int32_t temp_speed = speed1;
			speed1 = speed2;
			speed2 = temp_speed;

			// Add a flash at the collision point
			draw_pulse(pos1, &flash_color, PULSE_WIDTH);
			// Prevent pulses from getting stuck inside each other
			pos1 += speed1;
			pos2 += speed2;
		}

		// Boundary detection and bounce
		if (pos1 < 0) {
			pos1 = 0;
			speed1 *= -1;
		}
		if ((pos1 / scale) >= STRIP_NUM_PIXELS) {
			pos1 = (STRIP_NUM_PIXELS - 1) * scale;
			speed1 *= -1;
		}
		if (pos2 < 0) {
			pos2 = 0;
			speed2 *= -1;
		}
		if ((pos2 / scale) >= STRIP_NUM_PIXELS) {
			pos2 = (STRIP_NUM_PIXELS - 1) * scale;
			speed2 *= -1;
		}

		led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		k_sleep(K_MSEC(delay_ms));
	}
}
#endif

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

	const struct led_rgb red    = RGB(0x7F, 0x00, 0x00);
	const struct led_rgb green  = RGB(0x00, 0x7F, 0x00);
	const struct led_rgb blue   = RGB(0x00, 0x00, 0x7F);
	const struct led_rgb white  = RGB(0x7F, 0x7F, 0x7F);
	const struct led_rgb black  = RGB(0x00, 0x00, 0x00);

	/* Pulse colors */
	// const struct led_rgb pulse_color1 = RGB(0x00, 0x40, 0xFF); /* Blue */
	// const struct led_rgb pulse_color2 = RGB(0xFF, 0x00, 0x80); /* Magenta */

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
		set_solid_color(&black, K_MSEC(500)); // Pause
						      //
		/* Pattern 2: Full Fade */
		LOG_INF("Pattern: Full Fade");
		pattern_full_fade(256, K_MSEC(20));
		set_solid_color(&black, K_MSEC(500)); // Pause between patterns
						      //
		/* Pattern 3: Comet */
		LOG_INF("Pattern: Comet");
		pattern_comet(&white, K_MSEC(40));
		pattern_comet(&red, K_MSEC(60));
		pattern_comet(&green, K_MSEC(80));
		pattern_comet(&blue, K_MSEC(100));
		set_solid_color(&black, K_MSEC(500));

		/* Pattern 4: Rainbow Cycle */
		LOG_INF("Pattern: Rainbow Cycle");
		pattern_rainbow_cycle(K_MSEC(10));
		set_solid_color(&black, K_MSEC(500));

		/* Pattern 5: Bouncing Pulses */
		// LOG_INF("Pattern: Bouncing Pulses");
		// pattern_bouncing_pulses(&pulse_color1, &pulse_color2, K_SECONDS(1));
		// set_solid_color(&black, K_MSEC(500));

		/* Pattern 6: Theater Chase Rainbow */
		LOG_INF("Pattern: Theater Chase Rainbow");
		pattern_theater_chase_rainbow(K_MSEC(50));
		set_solid_color(&black, K_MSEC(500));

		/* Pattern 7: Twinkle */
		LOG_INF("Pattern: Twinkle");
		pattern_twinkle(&white, 100, K_MSEC(50));
		set_solid_color(&black, K_MSEC(500));

		/* Pattern 8: Fire */
		LOG_INF("Pattern: Fire");
		pattern_fire(55, 120, K_SECONDS(1));
		set_solid_color(&black, K_MSEC(500));
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

static int cmd_effect_count(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Count=%d; ready=%d", effect_count, device_is_ready(strip) ? 1 : 0);
	return 0;
}

static int cmd_effect_on(const struct shell *sh, size_t argc, char **argv)
{
	if (!effect_on) {
		effect_on = true;
		shell_print(sh, "LED animation started.");
	} else {
		shell_print(sh, "LED animation is already running.");
	}
	return 0;
}

static int cmd_effect_off(const struct shell *sh, size_t argc, char **argv)
{
	if (effect_on) {
		effect_on = false;
		// The main loop will check this flag and stop.
		// Wait a moment to let the current effect check the flag.
		shell_print(sh, "LED animation stopping...");
		k_sleep(K_MSEC(100));
	} else {
		shell_print(sh, "LED animation is already off.");
	}

	// Turn all LEDs off
	const struct led_rgb black = RGB(0x00, 0x00, 0x00);
	set_solid_color_unblock(&black, K_NO_WAIT);

	return 0;
}

static int cmd_effect_set(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 4) {
		shell_error(sh, "Usage: set <r> <g> <b> (values 0-255)");
		return -EINVAL;
	}

	// Stop any running animation first. We call our off function.
	cmd_effect_off(sh, 0, NULL);
	k_sleep(K_MSEC(50)); // Give it a moment to take effect.


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
		  cmd_effect_count),
	SHELL_CMD(on, NULL, "Start the LED animation cycle.",
		  cmd_effect_on),
	SHELL_CMD(off, NULL, "Stop the LED animation and turn off LEDs.",
		  cmd_effect_off),
	SHELL_CMD_ARG(set, NULL, "Set a solid color: set <r> <g> <b>",
		      cmd_effect_set, 4, 3),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(eff, &sub_effect, "LED effect control commands",
		   NULL);
