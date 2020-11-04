/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "peripheral_charger.h"
#include "queue.h"
#include "stdbool.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Peripheral Charge Manager */

#define CPRINTS(fmt, args...) cprints(CC_PCHG, "PCHG: " fmt, ##args)

#define TASK_ID_TO_PERIPH_CHG_PORT(id) ((id) - TASK_ID_PERI_CHG0)

static void pchg_queue_event(struct pchg *ctx, enum pchg_event event)
{
	/* TODO: Make it atomic. */
	if (queue_add_unit(&ctx->events, &event) == 0)
		CPRINTS("ERR: Queue is full");
}

static const char *_text_state(enum pchg_state state)
{
	/* TODO: Use "S%d" for normal build. */
	static const char * const state_names[] = {
		[PCHG_STATE_RESET] = "RESET",
		[PCHG_STATE_INITIALIZED] = "INITIALIZED",
		[PCHG_STATE_ENABLED] = "ENABLED",
		[PCHG_STATE_DISABLED] = "DISABLED",
		[PCHG_STATE_DETECTED] = "DETECTED",
		[PCHG_STATE_CHARGING] = "CHARGING",
		[PCHG_STATE_PAUSED] = "PAUSED",
	};

	if (state >= sizeof(state_names))
		return "UNDEFINED";

	return state_names[state];
}

static const char *_text_event(enum pchg_event event)
{
	/* TODO: Use "S%d" for normal build. */
	static const char * const event_names[] = {
		[PCHG_EVENT_NONE] = "NONE",
		[PCHG_EVENT_IRQ] = "IRQ",
		[PCHG_EVENT_INITIALIZED] = "INITIALIZED",
		[PCHG_EVENT_ENABLED] = "ENABLED",
		[PCHG_EVENT_DISABLED] = "DISABLED",
		[PCHG_EVENT_DEVICE_DETECTED] = "DEVICE_DETECTED",
		[PCHG_EVENT_DEVICE_LOST] = "DEVICE_LOST",
		[PCHG_EVENT_CHARGE_STARTED] = "CHARGE_STARTED",
		[PCHG_EVENT_CHARGE_UPDATE] = "CHARGE_UPDATE",
		[PCHG_EVENT_CHARGE_ERROR] = "CHARGE_ERROR",
		[PCHG_EVENT_CHARGE_END] = "CHARGE_END",
		[PCHG_EVENT_INITIALIZE] = "INITIALIZE",
		[PCHG_EVENT_ENABLE] = "ENABLE",
		[PCHG_EVENT_DISABLE] = "DISABLE",
		[PCHG_EVENT_PAUSE] = "PAUSE",
		[PCHG_EVENT_UNPAUSE] = "UNPAUSE",
	};

	if (event >= sizeof(event_names))
		return "UNDEFINED";

	return event_names[event];
}

static enum pchg_state pchg_state_reset(struct pchg *ctx)
{
	enum pchg_state state = PCHG_STATE_RESET;
	int rv;

	switch (ctx->event) {
	case PCHG_EVENT_INITIALIZE:
		gpio_enable_interrupt(ctx->irq_pin);
		rv = ctx->drv->init(ctx);
		if (rv == EC_SUCCESS) {
			pchg_queue_event(ctx, PCHG_EVENT_ENABLE);
			state = PCHG_STATE_INITIALIZED;
		} else if (rv != EC_SUCCESS_PENDING) {
			gpio_disable_interrupt(ctx->irq_pin);
			CPRINTS("ERR: Failed to initialize");
		}
		break;
	case PCHG_EVENT_INITIALIZED:
		pchg_queue_event(ctx, PCHG_EVENT_ENABLE);
		state = PCHG_STATE_INITIALIZED;
		break;
	default:
		break;
	}

	return state;
}

static enum pchg_state pchg_state_initialized(struct pchg *ctx)
{
	enum pchg_state state = PCHG_STATE_INITIALIZED;
	int rv;

	switch (ctx->event) {
	case PCHG_EVENT_ENABLE:
		rv = ctx->drv->enable(ctx, true);
		if (rv == EC_SUCCESS)
			state = PCHG_STATE_ENABLED;
		else if (rv != EC_SUCCESS_PENDING)
			CPRINTS("ERR: Failed to enable");
		break;
	case PCHG_EVENT_ENABLED:
		state = PCHG_STATE_ENABLED;
		break;
	default:
		break;
	}

	return state;
}

static enum pchg_state pchg_state_disabled(struct pchg *ctx)
{
	enum pchg_state state = PCHG_STATE_DISABLED;
	int rv;

	/* Should spin here until error condition is cleared. */
	if (ctx->drv->get_error_info(ctx) || ctx->error)
		return PCHG_STATE_DISABLED;

	switch (ctx->event) {
	case PCHG_EVENT_ENABLE:
		rv = ctx->drv->enable(ctx, true);
		if (rv == EC_SUCCESS)
			state = PCHG_STATE_ENABLED;
		else if (rv != EC_SUCCESS_PENDING)
			CPRINTS("ERR: Failed to enable");
		break;
	case PCHG_EVENT_ENABLED:
	case PCHG_EVENT_DEVICE_LOST:
		state = PCHG_STATE_ENABLED;
		break;
	case PCHG_EVENT_DEVICE_DETECTED:
		state = PCHG_STATE_DETECTED;
		break;
	default:
		break;
	}

	return state;
}

static enum pchg_state pchg_state_enabled(struct pchg *ctx)
{
	enum pchg_state state = PCHG_STATE_ENABLED;

	switch (ctx->event) {
	case PCHG_EVENT_DISABLE:
		if (ctx->drv->enable(ctx, false) == EC_SUCCESS)
			state = PCHG_STATE_DISABLED;
		break;
	case PCHG_EVENT_DEVICE_DETECTED:
		state = PCHG_STATE_DETECTED;
		break;
	case PCHG_EVENT_CHARGE_STARTED:
		state = PCHG_STATE_CHARGING;
		break;
	default:
		break;
	}

	return state;
}

static enum pchg_state pchg_state_detected(struct pchg *ctx)
{
	enum pchg_state state = PCHG_STATE_DETECTED;

	switch (ctx->event) {
	case PCHG_EVENT_PAUSE:
		if (ctx->drv->pause(ctx, true))
			state = PCHG_STATE_PAUSED;
		break;
	case PCHG_EVENT_DISABLE:
		if (ctx->drv->enable(ctx, false))
			state = PCHG_STATE_DISABLED;
		break;
	case PCHG_EVENT_CHARGE_STARTED:
		state = PCHG_STATE_CHARGING;
		break;
	default:
		break;
	}

	return state;
}

static enum pchg_state pchg_state_charging(struct pchg *ctx)
{
	enum pchg_state state = PCHG_STATE_CHARGING;

	switch (ctx->event) {
	case PCHG_EVENT_CHARGE_UPDATE:
		CPRINTS("Battery %d%%", ctx->battery_percent);
		break;
	case PCHG_EVENT_PAUSE:
		if (ctx->drv->pause(ctx, true))
			state = PCHG_STATE_PAUSED;
		break;
	case PCHG_EVENT_DEVICE_LOST:
		state = PCHG_STATE_ENABLED;
		break;
	case PCHG_EVENT_CHARGE_ERROR:
		state = PCHG_STATE_DISABLED;
		break;
	case PCHG_EVENT_CHARGE_END:
		state = PCHG_STATE_DETECTED;
		break;
	default:
		break;
	}

	return state;
}

static enum pchg_state pchg_state_paused(struct pchg *ctx)
{
	switch (ctx->event) {
	case PCHG_EVENT_UNPAUSE:
		if (ctx->drv->pause(ctx, false))
			return PCHG_STATE_DETECTED;
		break;
	case PCHG_EVENT_DEVICE_LOST:
		return PCHG_STATE_ENABLED;
	case PCHG_EVENT_CHARGE_ERROR:
		return PCHG_STATE_DISABLED;
	default:
		break;
	}

	return PCHG_STATE_PAUSED;
}

static void pchg_run(struct pchg *ctx)
{
	enum pchg_state previous_state = ctx->state;
	int port = PCHG_CTX_TO_PORT(ctx);
	int rv;

	CPRINTS("P%d Run in %s", port, _text_state(ctx->state));

	if (!queue_remove_unit(&ctx->events, &ctx->event)) {
		CPRINTS("P%d No event in queue", port);
		return;
	}

	if (ctx->event == PCHG_EVENT_IRQ) {
		rv = ctx->drv->get_event(ctx);
		if (rv) {
			CPRINTS("ERR: get_event (%d)", rv);
			return;
		}
		CPRINTS("IRQ %s", _text_event(ctx->event));
	} else {
		CPRINTS("EVT %s", _text_event(ctx->event));
	}

	switch (ctx->state) {
	case PCHG_STATE_RESET:
		ctx->state = pchg_state_reset(ctx);
		break;
	case PCHG_STATE_INITIALIZED:
		ctx->state = pchg_state_initialized(ctx);
		break;
	case PCHG_STATE_DISABLED:
		ctx->state = pchg_state_disabled(ctx);
		break;
	case PCHG_STATE_ENABLED:
		ctx->state = pchg_state_enabled(ctx);
		break;
	case PCHG_STATE_DETECTED:
		ctx->state = pchg_state_detected(ctx);
		break;
	case PCHG_STATE_CHARGING:
		ctx->state = pchg_state_charging(ctx);
		break;
	case PCHG_STATE_PAUSED:
		ctx->state = pchg_state_paused(ctx);
		break;
	default:
		CPRINTS("ERR: Unknown state (%d)", ctx->state);
		break;
	}

	if (previous_state != ctx->state)
		CPRINTS("->%s", _text_state(ctx->state));

	ctx->event = PCHG_EVENT_NONE;
	CPRINTS("Done");
}

void pchg_irq(enum gpio_signal signal)
{
	struct pchg *ctx;
	int i;

	for (i = 0; i < pchg_count; i++) {
		ctx = &pchgs[i];
		if (signal == ctx->irq_pin) {
			pchg_queue_event(ctx, PCHG_EVENT_IRQ);
			task_set_event(TASK_ID_PCHG, BIT(i), 0);
			task_wake(TASK_ID_PCHG);
			return;
		}
	}
}

void pchg_task(void *u)
{
	struct pchg *ctx;
	int port;
	int i;

	/* TODO: i2c is wedged for a while after reset. investigate. */
	msleep(500);

	for (i = 0; i < pchg_count; i++) {
		ctx = &pchgs[i];
		ctx->state = PCHG_STATE_RESET;
		queue_init(&ctx->events);
		pchg_queue_event(ctx, PCHG_EVENT_INITIALIZE);
		task_set_event(TASK_ID_PCHG, BIT(i), 0);
	}

	while (true) {
		const uint32_t evt = task_wait_event(-1);
		uint32_t port_mask = evt & GENMASK(15, 0);

		/* Process pending events from all ports. */
		while (port_mask) {
			port = get_next_bit(&port_mask);
			ctx = &pchgs[port];
			pchg_run(ctx);
			if (!queue_is_empty(&ctx->events))
				port_mask |= BIT(port);
		}
	}
}

static int cc_pchg(int argc, char **argv)
{
	int port;
	char *end;
	struct pchg *ctx;

	if (argc < 2 || 3 < argc)
		return EC_ERROR_PARAM_COUNT;

	port = strtoi(argv[1], &end, 0);
	if (*end || port < 0 || port >= pchg_count)
		return EC_ERROR_PARAM2;
	ctx = &pchgs[port];

	if (argc == 2) {
		ccprintf("P%d %s %s\n", port,
			 _text_state(ctx->state), _text_event(ctx->event));
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[2], "init")) {
		ctx->state = PCHG_STATE_RESET;
		pchg_queue_event(ctx, PCHG_EVENT_INITIALIZE);
	} else if (!strcasecmp(argv[2], "enable")) {
		if (ctx->state != PCHG_STATE_INITIALIZED)
			return EC_ERROR_INVAL;
		pchg_queue_event(ctx, PCHG_EVENT_ENABLE);
	} else {
		return EC_ERROR_PARAM1;
	}

	task_set_event(TASK_ID_PCHG, BIT(port), 0);
	task_wake(TASK_ID_PCHG);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pchg, cc_pchg,
			"<port> [init/enable]",
			"Control peripheral chargers");
