/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions for blinking LEDs.
 */

#include <logging/log.h>
#include <devicetree.h>
#include <kernel.h>
#include <zephyr.h>

#include "console.h"
#include "drivers/cros_led.h"
#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "led_common.h"
#include "util.h"

#define LED_AUTO_CONTROL_FLAG(id) (1 << (id))

static const struct device *cros_led_dev;
static uint32_t led_auto_control_flags = ~0x00;

const enum ec_led_id cros_supported_led_ids[] = {
#if DT_NODE_EXISTS(DT_PATH(gpio_led, led_id))
    DT_FOREACH_CHILD(DT_PATH(gpio_led, led_id), LED_ENUM_WITH_COMMA)
#endif
};

const int cros_supported_led_ids_count = ARRAY_SIZE(cros_supported_led_ids);

static int led_is_supported(enum ec_led_id led_id)
{
	int i;
	static int supported_leds = -1;

	if (supported_leds == -1) {
		supported_leds = 0;

		for (i = 0; i < cros_supported_led_ids_count; i++)
			supported_leds |= (1 << cros_supported_led_ids[i]);
	}

	return ((1 << (int)led_id) & supported_leds);
}

void led_auto_control(enum ec_led_id led_id, int enable)
{
	if (enable)
		led_auto_control_flags |= LED_AUTO_CONTROL_FLAG(led_id);
	else
		led_auto_control_flags &= ~LED_AUTO_CONTROL_FLAG(led_id);
}

int led_auto_control_is_enabled(enum ec_led_id led_id)
{
	if (!led_is_supported(led_id))
		return 0;

	return (led_auto_control_flags & LED_AUTO_CONTROL_FLAG(led_id)) != 0;
}

void led_control(enum ec_led_id led_id, enum ec_led_state state)
{
    cros_led_control(cros_led_dev, led_id, state);
}

/* Called by hook task every TICK */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		cros_board_led_set_battery(cros_led_dev);
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);

static enum ec_status led_command_control(struct host_cmd_handler_args *args)
{
	const struct ec_params_led_control *p = args->params;
	struct ec_response_led_control *r = args->response;
	int i;

	args->response_size = sizeof(*r);
	memset(r->brightness_range, 0, sizeof(r->brightness_range));

	if (!led_is_supported(p->led_id))
		return EC_RES_INVALID_PARAM;

	cros_led_get_brightness_range(cros_led_dev, p->led_id, r->brightness_range);
	if (p->flags & EC_LED_FLAGS_QUERY)
		return EC_RES_SUCCESS;

	for (i = 0; i < EC_LED_COLOR_COUNT; i++)
		if (r->brightness_range[i] == 0 && p->brightness[i] != 0)
			return EC_RES_INVALID_PARAM;

	if (p->flags & EC_LED_FLAGS_AUTO) {
		led_auto_control(p->led_id, 1);
	} else {
		if (cros_led_set_brightness(cros_led_dev, p->led_id, p->brightness) != EC_SUCCESS)
			return EC_RES_INVALID_PARAM;
		led_auto_control(p->led_id, 0);
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_LED_CONTROL, led_command_control, EC_VER_MASK(1));

/* static int system_init_led(const struct device *unused)
{
    ARG_UNUSED(unused);

    cros_led_dev = device_get_binding(CROS_LED_LABEL);

    if (!cros_led_dev) {
        LOG_ERR("Error: LED device is not ready");
        return -1;
    }

    cros_led_init(cros_led_dev);

    return 0;
}
SYS_INIT(system_init_led, PRE_KERNEL_1, )
*/
