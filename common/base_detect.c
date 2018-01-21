/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common detachable base detection code. */

#include "adc.h"
#include "base_detect.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#define BASE_SEEMS_ATTACHED(att_pin, det_pin)		\
	((att_pin != ADC_READ_ERROR) &&			\
	 (det_pin != ADC_READ_ERROR) &&			\
	 (base_seems_attached(att_pin, det_pin)))
#define BASE_SEEMS_DETACHED(att_pin, det_pin)		\
	((att_pin != ADC_READ_ERROR) &&			\
	 (det_pin != ADC_READ_ERROR) &&			\
	 (base_seems_detached(att_pin, det_pin)))

#define DEFAULT_POLL_TIMEOUT_US (250 * MSEC)
#define DEBOUNCE_TIMEOUT_US (20 * MSEC)

static enum base_detect_state state = BASE_DETACHED;
static int timeout = DEFAULT_POLL_TIMEOUT_US;
static int debug;

static char *state_names[] = { "DETACHED", "ATTACHED_DEBOUNCE",
			       "ATTACHED", "DETACHED_DEBOUNCE",
};

static void set_state(enum base_detect_state new_state)
{
	if (new_state != state) {
		CPRINTS("BD: st%d %s", new_state,
			debug ? state_names[new_state] : "");
		state = new_state;
	}
}

static void read_pin(int *reading, enum adc_channel pin)
{
	int read_val = adc_read_channel(pin);

	if (debug)
		CPRINTS("BD: %sach: %dmV",
			pin == base_pin_cfg.attach_pin ? "att" : "det",
			read_val);

	*reading = read_val;
}

static void base_detect_deferred(void);
DECLARE_DEFERRED(base_detect_deferred);
static void base_detect_deferred(void)
{
	int attach_reading;
	int detach_reading;

	read_pin(&attach_reading, base_pin_cfg.attach_pin);
	read_pin(&detach_reading, base_pin_cfg.detach_pin);

	switch (state) {
	case BASE_DETACHED:
		/* Check to see if a base may be attached. */
		if (BASE_SEEMS_ATTACHED(attach_reading, detach_reading)) {
			timeout = DEBOUNCE_TIMEOUT_US;
			set_state(BASE_ATTACHED_DEBOUNCE);
		}
		break;

	case BASE_ATTACHED_DEBOUNCE:
		/* Check to see if it's still attached. */
		if (BASE_SEEMS_ATTACHED(attach_reading, detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_ATTACHED);
			hook_notify(HOOK_BASE_DETECT_CHANGE);
		} else if (BASE_SEEMS_DETACHED(attach_reading,
					       detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_DETACHED);
		}
		break;

	case BASE_ATTACHED:
		/* Check to see if a base may be detached. */
		if (BASE_SEEMS_DETACHED(attach_reading, detach_reading)) {
			timeout = DEBOUNCE_TIMEOUT_US;
			set_state(BASE_DETACHED_DEBOUNCE);
		}
		break;

	case BASE_DETACHED_DEBOUNCE:
		/* Check to see if a base is still detached. */
		if (BASE_SEEMS_DETACHED(attach_reading, detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_DETACHED);
			hook_notify(HOOK_BASE_DETECT_CHANGE);
		} else if (BASE_SEEMS_ATTACHED(attach_reading,
					       detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_ATTACHED);
		}
		break;
		/* do you want to add an interrupt? */

	default:
		break;
	};

	/* Check again in the appropriate time. */
	hook_call_deferred(&base_detect_deferred_data, timeout);
};
DECLARE_HOOK(HOOK_INIT, base_detect_deferred, HOOK_PRIO_DEFAULT);

enum base_detect_state base_get_detect_state(void)
{
	return state;
}

static int command_basedetectdebug(int argc, char **argv)
{
	if (argc < 1)
		return EC_ERROR_PARAM_COUNT;

	if (!parse_bool(argv[1], &debug))
		return EC_ERROR_PARAM1;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(basedet, command_basedetectdebug, "[enable|disable]",
			"Enable/Disable base detection debug info.");
