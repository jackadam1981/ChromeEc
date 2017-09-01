/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Host event commands for Chrome EC */

#include "atomic.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "lpc.h"
#include "mkbp_event.h"
#include "system.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_EVENTS, outstr)
#define CPRINTS(format, args...) cprints(CC_EVENTS, format, ## args)

/*
 * Maintain two copies of the events that are set.
 *
 * The primary copy is mirrored in mapped memory and used to trigger interrupts
 * on the host via ACPI/SCI/SMI/GPIO.
 *
 * The secondary (B) copy is used to track events at a non-interrupt level (for
 * example, so a user-level process can find out what events have happened
 * since the last call, even though a kernel-level process is consuming events
 * from the first copy).
 *
 * Setting an event sets both copies.  Copies are cleared separately.
 */
static uint32_t events;
static uint32_t events_copy_b;

static uint32_t prev_sleep_wake_mask;

/* Lazy wake masks */
static struct lazy_wake_masks {
	uint32_t s3_lazy_wm;
	uint32_t s5_lazy_wm;
#ifdef CONFIG_POWER_S0IX
	uint32_t s0ix_lazy_wm;
#endif
} lazy_wm;

uint32_t host_get_events(void)
{
	return events;
}

void host_set_events(uint32_t mask)
{
	/* ignore host events the rest of board doesn't care about */
	mask &= CONFIG_HOST_EVENT_REPORT_MASK;

	/* exit now if nothing has changed */
	if (!((events & mask) != mask || (events_copy_b & mask) != mask))
		return;

	CPRINTS("event set 0x%08x", mask);

	atomic_or(&events, mask);
	atomic_or(&events_copy_b, mask);

#ifdef CONFIG_LPC
	lpc_set_host_event_state(events);
#else
	*(uint32_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = events;
#ifdef CONFIG_MKBP_EVENT
#ifdef CONFIG_MKBP_USE_HOST_EVENT
#error "Config error: MKBP must not be on top of host event"
#endif
	mkbp_send_event(EC_MKBP_EVENT_HOST_EVENT);
#endif  /* CONFIG_MKBP_EVENT */
#endif  /* !CONFIG_LPC */
}

void host_clear_events(uint32_t mask)
{
	/* ignore host events the rest of board doesn't care about */
	mask &= CONFIG_HOST_EVENT_REPORT_MASK;

	/* return early if nothing changed */
	if (!(events & mask))
		return;

	CPRINTS("event clear 0x%08x", mask);

	atomic_clear(&events, mask);

#ifdef CONFIG_LPC
	lpc_set_host_event_state(events);
#else
	*(uint32_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = events;
#ifdef CONFIG_MKBP_EVENT
	mkbp_send_event(EC_MKBP_EVENT_HOST_EVENT);
#endif
#endif  /* !CONFIG_LPC */
}

#ifndef CONFIG_LPC
static int host_get_next_event(uint8_t *out)
{
	uint32_t event_out = events;
	memcpy(out, &event_out, sizeof(event_out));
	atomic_clear(&events, event_out);
	*(uint32_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = events;
	return sizeof(event_out);
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_HOST_EVENT, host_get_next_event);
#endif

/**
 * Clear one or more host event bits from copy B.
 *
 * @param mask          Event bits to clear (use EC_HOST_EVENT_MASK()).
 *                      Write 1 to a bit to clear it.
 */
static void host_clear_events_b(uint32_t mask)
{
	/* Only print if something's about to change */
	if (events_copy_b & mask)
		CPRINTS("event clear B 0x%08x", mask);

	atomic_clear(&events_copy_b, mask);
}

/**
 * Politely ask the CPU to enable/disable its own throttling.
 *
 * @param throttle	Enable (!=0) or disable(0) throttling
 */
test_mockable void host_throttle_cpu(int throttle)
{
	if (throttle)
		host_set_single_event(EC_HOST_EVENT_THROTTLE_START);
	else
		host_set_single_event(EC_HOST_EVENT_THROTTLE_STOP);
}

/*****************************************************************************/
/* Console commands */
static int command_host_event(int argc, char **argv)
{
	/* Handle sub-commands */
	if (argc == 3) {
		char *e;
		int i = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		if (!strcasecmp(argv[1], "set"))
			host_set_events(i);
		else if (!strcasecmp(argv[1], "clear"))
			host_clear_events(i);
		else if (!strcasecmp(argv[1], "clearb"))
			host_clear_events_b(i);
#ifdef CONFIG_LPC
		else if (!strcasecmp(argv[1], "smi"))
			lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, i);
		else if (!strcasecmp(argv[1], "sci"))
			lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, i);
		else if (!strcasecmp(argv[1], "wake"))
			lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, i);
#endif
		else
			return EC_ERROR_PARAM1;
	}

	/* Print current SMI/SCI status */
	ccprintf("Events:    0x%08x\n", host_get_events());
	ccprintf("Events-B:  0x%08x\n", events_copy_b);
#ifdef CONFIG_LPC
	ccprintf("SMI mask:  0x%08x\n",
		 lpc_get_host_event_mask(LPC_HOST_EVENT_SMI));
	ccprintf("SCI mask:  0x%08x\n",
		 lpc_get_host_event_mask(LPC_HOST_EVENT_SCI));
	ccprintf("Wake mask: 0x%08x\n",
		 lpc_get_host_event_mask(LPC_HOST_EVENT_WAKE));
#endif
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hostevent, command_host_event,
			"[set | clear | clearb | smi | sci | wake] [mask]",
			"Print / set host event state");

/*****************************************************************************/
/* Host commands */

#ifdef CONFIG_LPC

static int host_event_get_smi_mask(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_mask *r = args->response;

	r->mask = lpc_get_host_event_mask(LPC_HOST_EVENT_SMI);
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_GET_SMI_MASK,
		     host_event_get_smi_mask,
		     EC_VER_MASK(0));

static int host_event_get_sci_mask(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_mask *r = args->response;

	r->mask = lpc_get_host_event_mask(LPC_HOST_EVENT_SCI);
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_GET_SCI_MASK,
		     host_event_get_sci_mask,
		     EC_VER_MASK(0));

static int host_event_get_wake_mask(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_mask *r = args->response;

	r->mask = lpc_get_host_event_mask(LPC_HOST_EVENT_WAKE);
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_GET_WAKE_MASK,
		     host_event_get_wake_mask,
		     EC_VER_MASK(0));

static int host_event_set_smi_mask(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event_mask *p = args->params;

	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_SET_SMI_MASK,
		     host_event_set_smi_mask,
		     EC_VER_MASK(0));

static int host_event_set_sci_mask(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event_mask *p = args->params;

	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_SET_SCI_MASK,
		     host_event_set_sci_mask,
		     EC_VER_MASK(0));

static int host_event_set_wake_mask(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event_mask *p = args->params;

	lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_SET_WAKE_MASK,
		     host_event_set_wake_mask,
		     EC_VER_MASK(0));

#endif  /* CONFIG_LPC */

static int host_event_get_b(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_mask *r = args->response;

	r->mask = events_copy_b;
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_GET_B,
		     host_event_get_b,
		     EC_VER_MASK(0));

static int host_event_clear(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event_mask *p = args->params;

	host_clear_events(p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_CLEAR,
		     host_event_clear,
		     EC_VER_MASK(0));

static int host_event_clear_b(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event_mask *p = args->params;

	host_clear_events_b(p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_CLEAR_B,
		     host_event_clear_b,
		     EC_VER_MASK(0));


static int program_host_event_get(struct host_cmd_handler_args *args)
{
	struct ec_response_program_host_event *r = args->response;
	const struct ec_params_program_host_event  *p = args->params;
	int result = EC_RES_SUCCESS;

	args->response_size = sizeof(*r);

	switch (p->mask) {
	case EC_HOST_EVENT_B:
		r->value = events_copy_b;
		break;
#ifdef CONFIG_LPC
	case EC_HOST_EVENT_SCI_MASK:
		r->value = lpc_get_host_event_mask(LPC_HOST_EVENT_SCI);
		break;
	case EC_HOST_EVENT_SMI_MASK:
		r->value = lpc_get_host_event_mask(LPC_HOST_EVENT_SMI);
		break;
	case EC_HOST_EVENT_ACTIVE_WAKE_MASK:
		r->value = lpc_get_host_event_mask(LPC_HOST_EVENT_WAKE);
		break;
#endif
	case EC_HOST_EVENT_PREV_SLEEP_WAKE_MASK:
		r->value = prev_sleep_wake_mask;
		break;
#ifdef CONFIG_POWER_S0IX
	case EC_HOST_EVENT_LAZY_WAKE_MASK_S0IX:
		r->value = lazy_wm.s0ix_lazy_wm;
		break;
#endif
	case EC_HOST_EVENT_LAZY_WAKE_MASK_S3:
		r->value = lazy_wm.s3_lazy_wm;
		break;
	case EC_HOST_EVENT_LAZY_WAKE_MASK_S5:
		r->value = lazy_wm.s5_lazy_wm;
		break;
	default:
		result = EC_RES_INVALID_PARAM;
		break;
	}

	return result;
}

static int program_host_event_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_program_host_event  *p = args->params;
	int result = EC_RES_SUCCESS;

	switch (p->mask) {
#ifdef CONFIG_LPC
	case EC_HOST_EVENT_SCI_MASK:
		lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, p->value);
		break;
	case EC_HOST_EVENT_SMI_MASK:
		lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, p->value);
		break;
	case EC_HOST_EVENT_ACTIVE_WAKE_MASK:
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, p->value);
		break;
#endif
#ifdef CONFIG_POWER_S0IX
	case EC_HOST_EVENT_LAZY_WAKE_MASK_S0IX:
		lazy_wm.s0ix_lazy_wm = p->value;
		break;
#endif
	case EC_HOST_EVENT_LAZY_WAKE_MASK_S3:
		lazy_wm.s3_lazy_wm = p->value;
		break;
	case EC_HOST_EVENT_LAZY_WAKE_MASK_S5:
		lazy_wm.s5_lazy_wm = p->value;
		break;
	default:
		result = EC_RES_INVALID_PARAM;
		break;
	}

	return result;
}

static int program_host_event_clear(struct host_cmd_handler_args *args)
{
	const struct ec_params_program_host_event  *p = args->params;
	int result = EC_RES_SUCCESS;

	switch (p->mask) {
	case EC_HOST_EVENT_MAIN:
		host_clear_events(p->value);
		break;
	case EC_HOST_EVENT_B:
		host_clear_events_b(p->value);
		break;
	default:
		result = EC_RES_INVALID_PARAM;
	}

	return result;
}

static int host_command_program_host_event(struct host_cmd_handler_args *args)
{
	const struct ec_params_program_host_event  *p = args->params;

	switch (p->action) {
	case EC_HOST_EVENT_GET:
		return program_host_event_get(args);
	case EC_HOST_EVENT_SET:
		return program_host_event_set(args);
	case EC_HOST_EVENT_CLEAR:
		return program_host_event_clear(args);
	default:
		return EC_RES_INVALID_PARAM;
	}
}

DECLARE_HOST_COMMAND(EC_CMD_PROGRAM_HOST_EVENT,
		     host_command_program_host_event,
		     EC_VER_MASK(0));

void set_prev_sleep_wake_mask(uint32_t mask)
{
	prev_sleep_wake_mask = mask;
}



#define LAZY_WAKE_MASK_SYSJUMP_TAG		0x4C4D /* LM - Lazy Wakemask*/
#define LAZY_WAKE_MASK_HOOK_VERSION		1

uint32_t get_lazy_wake_mask(enum power_state state)
{
	switch (state) {
	case POWER_S5:
		return lazy_wm.s5_lazy_wm;
	case POWER_S3:
		return lazy_wm.s3_lazy_wm;
#ifdef CONFIG_POWER_S0IX
	case POWER_S0ix:
		return lazy_wm.s0ix_lazy_wm;
#endif
	default:
		return 0;
	}
}

static void preserve_lazy_wm(void)
{
	system_add_jump_tag(LAZY_WAKE_MASK_SYSJUMP_TAG,
			    LAZY_WAKE_MASK_HOOK_VERSION,
			    sizeof(lazy_wm),
			    &lazy_wm);
}
DECLARE_HOOK(HOOK_SYSJUMP, preserve_lazy_wm,
	     HOOK_PRIO_DEFAULT);

static void restore_lazy_wm(void)
{
	const struct lazy_wake_masks *wm_state;
	int version, size;

	wm_state = (const struct lazy_wake_masks *)
			system_get_jump_tag(LAZY_WAKE_MASK_SYSJUMP_TAG,
				 &version, &size);

	if (wm_state && (version == LAZY_WAKE_MASK_HOOK_VERSION) &&
	    (size == sizeof(lazy_wm))) {
		lazy_wm = *wm_state;
	}
}
DECLARE_HOOK(HOOK_INIT, restore_lazy_wm,
	     HOOK_PRIO_INIT_CHIPSET + 1);

