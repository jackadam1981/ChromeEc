
/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Lightbar LED control.
 */
#define DT_DRV_COMPAT cros_ec_led_lightbar
#include "ec_commands.h"
#include "hooks.h"
#include "led.h"
#include "led_common.h"
#include "task.h"
#include "util.h"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightbar_led, LOG_LEVEL_DBG);
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
	     "Exactly one instance of cros-ec-led-lightbar should be defined.");
#define LIGHTBAR_NODE DT_INST(0, DT_DRV_COMPAT)
#define LIGHTBAR0_NODE DT_CHILD(DT_INST(0, DT_DRV_COMPAT), lightbar0)
/*
 * Create a const led_pins_node_t struct for each color child node
 * of the lightbar devicetree node.
 */
#define SET_PIN_NODE(node_id)                                \
	{                                                    \
		.led_color = GET_PROP(node_id, led_color),   \
		.led_id = GET_PROP(LIGHTBAR_NODE, led_id),   \
		.color = {                                 \
			.r = (DT_PROP(node_id, color) >> 16) & 0xFF, \
			.g = (DT_PROP(node_id, color) >> 8) & 0xFF,  \
			.b = DT_PROP(node_id, color) & 0xFF,         \
		 },                                         \
		.pins_count = 0                              \
	}
#define GEN_PINS_NODES(id) \
	struct led_pins_node_t PINS_NODE(id) = SET_PIN_NODE(id);
DT_FOREACH_CHILD(LIGHTBAR0_NODE, GEN_PINS_NODES)
/*
 * Create an array of pointers to the led_pins_node_t structs. This
 * is used by the common LED code to find color definitions.
 */
#define PINS_NODE_PTR(node_id) &PINS_NODE(node_id),
const struct led_pins_node_t *const pins_node[] = {
	DT_FOREACH_CHILD(LIGHTBAR0_NODE, PINS_NODE_PTR)
};
const int pins_node_count = ARRAY_SIZE(pins_node);
static struct led_rgb lightbar_buf[DT_PROP(DT_PHANDLE(LIGHTBAR0_NODE, strip),
    chain_length)];
/* Data for the lightbar instance */
static const struct {
	const struct device *strip;
	uint32_t num_leds;
	enum ec_led_id led_id;
} lb_data = { .strip = DEVICE_DT_GET(DT_PHANDLE(LIGHTBAR0_NODE, strip)),
	      .num_leds = DT_PROP(DT_PHANDLE(LIGHTBAR0_NODE, strip),
				  chain_length),
	      .led_id = GET_PROP(LIGHTBAR_NODE, led_id) };
/*
 * State for the lightbar's color and transitions.
 * The color components are stored as 16.8 fixed-point values to allow
 * for fractional increments during transitions.
 */
static struct {
	struct {
		int32_t r, g, b;
	} color;
	int16_t r_step, g_step, b_step;
} g_lightbar_state;
/*
 * The lightbar task is a standalone thread that handles smooth color
 * transitions.
 */
void lightbar_task(void *u)
{
	struct led_rgb current_color;
    
	while (1) {
		/* Wait for a wake-up event or a 30ms tick for transitions */
		task_wait_event(30 * MSEC);
		/* Apply current color */
		current_color.r = CLAMP(g_lightbar_state.color.g >> 8, 0, 255);
		current_color.g = CLAMP(g_lightbar_state.color.r >> 8, 0, 255);
		current_color.b = CLAMP(g_lightbar_state.color.b >> 8, 0, 255);
		// LOG_DBG("Task Applying color R:%d G:%d B:%d", current_color.r,
		// 	current_color.g, current_color.b);
		for (int i = 0; i < lb_data.num_leds; i++) {
			lightbar_buf[i] = current_color;
		}
		led_strip_update_rgb(lb_data.strip, lightbar_buf,
				     lb_data.num_leds);
		/* Progress to next color for transition */
		g_lightbar_state.color.r += g_lightbar_state.r_step;
		g_lightbar_state.color.g += g_lightbar_state.g_step;
		g_lightbar_state.color.b += g_lightbar_state.b_step;
	}
}
void led_set_color_with_pattern(const struct led_pattern_node_t *pattern)
{
	const struct led_pins_node_t *next_color_node =
		pattern->pattern_color[pattern->cur_color].led_color_node;
	int32_t duration_ms =
		pattern->pattern_color[pattern->cur_color].duration_ms;
	uint8_t prev_color_idx =
		(pattern->cur_color + pattern->pattern_len - 1) %
		pattern->pattern_len;
	const struct led_pins_node_t *prev_color_node =
		pattern->pattern_color[prev_color_idx].led_color_node;
	struct led_rgb prev_color = prev_color_node->color;
	struct led_rgb next_color = next_color_node->color;
	// LOG_DBG("Pattern: prev(r%d,g%d,b%d) -> next(r%d,g%d,b%d) over %dms",
	// 	prev_color.r, prev_color.g, prev_color.b, next_color.r,
	// 	next_color.g, next_color.b, duration_ms);
	if (pattern->transition == LED_TRANSITION_LINEAR && duration_ms != 0) {
		g_lightbar_state.color.r =
			((int64_t)(next_color.r - prev_color.r) << 8) *
				pattern->elapsed_ms / duration_ms +
			(prev_color.r << 8);
		g_lightbar_state.color.g =
			((int64_t)(next_color.g - prev_color.g) << 8) *
				pattern->elapsed_ms / duration_ms +
			(prev_color.g << 8);
		g_lightbar_state.color.b =
			((int64_t)(next_color.b - prev_color.b) << 8) *
				pattern->elapsed_ms / duration_ms +
			(prev_color.b << 8);
		g_lightbar_state.r_step = DIV_ROUND_NEAREST(
			((int64_t)(next_color.r - prev_color.r) << 8) * 30,
			duration_ms);
		g_lightbar_state.g_step = DIV_ROUND_NEAREST(
			((int64_t)(next_color.g - prev_color.g) << 8) * 30,
			duration_ms);
		g_lightbar_state.b_step = DIV_ROUND_NEAREST(
			((int64_t)(next_color.b - prev_color.b) << 8) * 30,
			duration_ms);
		// LOG_DBG("Linear steps: r:%d g:%d b:%d",
		// 	g_lightbar_state.r_step, g_lightbar_state.g_step,
		// 	g_lightbar_state.b_step);
	} else { /* Default blinking or solid color */
		g_lightbar_state.color.r = next_color.r << 8;
		g_lightbar_state.color.g = next_color.g << 8;
		g_lightbar_state.color.b = next_color.b << 8;
		g_lightbar_state.r_step = 0;
		g_lightbar_state.g_step = 0;
		g_lightbar_state.b_step = 0;
	}
}
void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id != lb_data.led_id) {
		return;
	}
	memset(brightness_range, 0, EC_LED_COLOR_COUNT);
	for (int i = 0; i < pins_node_count; i++) {
		int color_idx = pins_node[i]->led_color - 1;
		if (color_idx >= 0 && color_idx < EC_LED_COLOR_COUNT) {
			brightness_range[color_idx] = 100;
		}
	}
}
static void lightbar_set_color_manual(enum led_color color, uint8_t brightness)
{
	struct led_rgb new_color = { 0, 0, 0 };
	if (color != LED_OFF) {
		for (int i = 0; i < pins_node_count; i++) {
			if (pins_node[i]->led_color == color) {
				new_color = pins_node[i]->color;
				break;
			}
		}
	}
	LOG_DBG("Manual color set to R:%d G:%d B:%d at %d%% brightness",
		new_color.r, new_color.g, new_color.b, brightness);
	g_lightbar_state.color.r =
		((uint32_t)new_color.r * brightness / 100) << 8;
	g_lightbar_state.color.g =
		((uint32_t)new_color.g * brightness / 100) << 8;
	g_lightbar_state.color.b =
		((uint32_t)new_color.b * brightness / 100) << 8;
	g_lightbar_state.r_step = 0;
	g_lightbar_state.g_step = 0;
	g_lightbar_state.b_step = 0;
}

void led_set_color(enum led_color color, enum ec_led_id led_id,uint8_t brightness)
{
	struct led_rgb new_color = { 0, 0, 0 };
	if (color != LED_OFF) {
		for (int i = 0; i < pins_node_count; i++) {
			if((pins_node[i]->led_color == color) &&(pins_node[i]->led_id == led_id)) {
				new_color = pins_node[i]->color;
				break;
			}
		}
	}
	LOG_DBG("Manual color set to R:%d G:%d B:%d at %d%% brightness",
		new_color.r, new_color.g, new_color.b, brightness);
	g_lightbar_state.color.r =
		((uint32_t)new_color.r * brightness / 100) << 8;
	g_lightbar_state.color.g =
		((uint32_t)new_color.g * brightness / 100) << 8;
	g_lightbar_state.color.b =
		((uint32_t)new_color.b * brightness / 100) << 8;
	g_lightbar_state.r_step = 0;
	g_lightbar_state.g_step = 0;
	g_lightbar_state.b_step = 0;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	bool color_set = false;
	if (led_id != lb_data.led_id) {
		return EC_ERROR_INVAL;
	}
    if (!led_auto_control_is_enabled(EC_LED_ID_LIGHTBAR_LED)){ 
        return EC_ERROR_INVAL;
    }
	/*
	 * Iterate backwards, so we pick the highest priority color
	 * (highest enum value) if more than one is set, as the lightbar
	 * can only display one color at a time.
	 */
	for (int i = EC_LED_COLOR_COUNT - 1; i >= 0; i--) {
		if (brightness[i] != 0) {
			/* led_color enum is index + 1 */
			lightbar_set_color_manual(i + 1, brightness[i]);
			color_set = true;
			break;
		}
	}
	if (!color_set) {
		lightbar_set_color_manual(LED_OFF, 0);
	}
	led_asynchronous_apply_color(false);
	return EC_SUCCESS;
}
__override int led_is_supported(enum ec_led_id led_id)
{
	return led_id == lb_data.led_id;
}
void led_asynchronous_apply_color(bool has_transitions)
{
	 if (has_transitions) {
	 	task_wake(EC_TASK_LIGHTBAR_PRIO);
	 } else {
		struct led_rgb current_color;
		/* Ensure the lightbar task is not running */
		task_wait_event(0);
		current_color.r = g_lightbar_state.color.g >> 8;
		current_color.g = g_lightbar_state.color.r >> 8;
		current_color.b = g_lightbar_state.color.b >> 8;
		// LOG_DBG("Applying immediate color R:%d G:%d B:%d",
		// 	current_color.r, current_color.g, current_color.b);
		for (int i = 0; i < lb_data.num_leds; i++) {
			lightbar_buf[i] = current_color;
		}
		led_strip_update_rgb(lb_data.strip, lightbar_buf,
				     lb_data.num_leds);
	}
}
/**** Console command *****/
static int cmd_led_count(const struct shell *sh, size_t argc, char **argv)
{
	LOG_DBG("LED count=%d", lb_data.num_leds);
	return 0;
}
static int cmd_color_set(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 4) {
		shell_error(sh, "Usage: set <r> <g> <b> (values 0-255)");
		return -EINVAL;
	}
	struct led_rgb color = {
		.r = (uint8_t)strtol(argv[1], NULL, 0),
		.g = (uint8_t)strtol(argv[2], NULL, 0),
		.b = (uint8_t)strtol(argv[3], NULL, 0),
	};
	// LOG_DBG("Setting the entire lighbar to R:%d G:%d B:%d",
	// 	    color.r, color.g, color.b);
    
    /* disable policy */
    led_auto_control(lb_data.led_id, 0);

    g_lightbar_state.color.r = (uint32_t)color.g << 8;
    g_lightbar_state.color.g = (uint32_t)color.r << 8;
    g_lightbar_state.color.b = (uint32_t)color.b << 8;
    
	for (size_t i = 0; i < lb_data.num_leds; i++) {
		lightbar_buf[i] = color;
	}
	led_strip_update_rgb(lb_data.strip, lightbar_buf,
				     lb_data.num_leds);
	return 0;
}
static int cmd_led_auto(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: auto <on|off>");
		return -EINVAL;
	}
	if (strcmp(argv[1], "on") == 0) {
		led_auto_control(lb_data.led_id, 1);
		shell_print(sh, "Lightbar auto control enabled.");
	} else if (strcmp(argv[1], "off") == 0) {
		led_auto_control(lb_data.led_id, 0);
		shell_print(sh, "Lightbar auto control disabled.");
	} else {
		shell_error(sh, "Usage: auto <on|off>");
		return -EINVAL;
	}
	return 0;
}
SHELL_STATIC_SUBCMD_SET_CREATE(lb_control,
	SHELL_CMD(count, NULL, "Show the number of LEDs in the lightbar strip",
		  cmd_led_count),
	SHELL_CMD_ARG(set, NULL, "Set a color: set <r> <g> <b>",
		      cmd_color_set, 4, 3),
// SHELL_CMD_ARG(set_single, NULL, "Set a color: set_single    ",
// 	      cmd_effect_set_single, 5, 4),
	SHELL_CMD_ARG(auto, NULL, "Set auto control: auto <on|off>",
		      cmd_led_auto, 2, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(lb, &lb_control, "Lightbar control command",
		   NULL);
