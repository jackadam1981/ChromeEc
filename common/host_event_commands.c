/* Copyright 2012 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Host event commands for Chrome EC */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "lpc.h"
#include "mkbp_event.h"
#include "power.h"
#include "system.h"
#include "task.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_EVENTS, outstr)
#define CPRINTS(format, args...) cprints(CC_EVENTS, format, ## args)

/*
 * This is used to avoid 64-bit shifts which might require a new library
 * function.
 */
#define HOST_EVENT_32BIT_MASK(x)	(1UL << ((x) - 1))
static void host_event_set_bit(host_event_t *ev, uint8_t bit)
{
	uint32_t *ptr = (uint32_t *)ev;

	*ev = 0;

	/*
	 * Host events are 1-based, so return early if event 0 is requested to
	 * be set.
	 */
	if (bit == 0)
		return;

#ifdef CONFIG_HOST_EVENT64
	if (bit > 32)
		*(ptr + 1) = HOST_EVENT_32BIT_MASK(bit - 32);
	else
#endif
		*ptr = HOST_EVENT_32BIT_MASK(bit);
}


/*
 * Maintain two copies of the events that are set.
 *
 * The primary copy is mirrored in mapped memory and used to trigger interrupts
 * on the host via ACPI/SCI/SMI/GPIO.
 *
 * The secondary (B) copy is used by entities other than ACPI to query the state
 * of host events on EC. Currently events_copy_b is used for
 *      1. Logging recovery mode switch in coreboot.
 *      2. Used by depthcharge on devices with no 8042 and no MKBP interrupt.
 *      3. Logging wake reason in coreboot.
 * Current query of a event from copy_b is immediately followed by clear of the
 * same event. Further uses of copy_b should make sure this semantics is
 * followed and none of the above mentioned use cases are broken.
 *
 * Setting an event sets both copies.  Copies are cleared separately.
 */
static host_event_t events;
static host_event_t events_copy_b;

static void host_events_atomic_or(host_event_t *e, host_event_t m)
{
	uint32_t *ptr = (uint32_t *)e;

	atomic_or(ptr, (uint32_t)m);
#ifdef CONFIG_HOST_EVENT64
	atomic_or(ptr + 1, (uint32_t)(m >> 32));
#endif
}

static void host_events_atomic_clear(host_event_t *e, host_event_t m)
{
	uint32_t *ptr = (uint32_t *)e;

	atomic_clear(ptr, (uint32_t)m);
#ifdef CONFIG_HOST_EVENT64
	atomic_clear(ptr + 1, (uint32_t)(m >> 32));
#endif
}

#if defined(CONFIG_MKBP_EVENT)
static void host_events_send_mkbp_event(host_event_t e)
{
#ifdef CONFIG_HOST_EVENT64
	/*
	 * If event bits in the upper 32-bit are set, indicate 64-bit host
	 * event.
	 */
	if (!(uint32_t)e)
		mkbp_send_event(EC_MKBP_EVENT_HOST_EVENT64);
	else
#endif
		mkbp_send_event(EC_MKBP_EVENT_HOST_EVENT);
}
#endif

host_event_t host_get_events(void)
{
	return events;
}

void host_set_events(host_event_t mask)
{
	/* ignore host events the rest of board doesn't care about */
#ifdef CONFIG_HOST_EVENT64
	mask &= CONFIG_HOST_EVENT64_REPORT_MASK;
#else
	mask &= CONFIG_HOST_EVENT_REPORT_MASK;
#endif

	/* exit now if nothing has changed */
	if (!((events & mask) != mask || (events_copy_b & mask) != mask))
		return;

	HOST_EVENT_CPRINTS("event set", mask);

	host_events_atomic_or(&events, mask);
	host_events_atomic_or(&events_copy_b, mask);

	*(host_event_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = events;
#ifdef CONFIG_MKBP_EVENT
#ifdef CONFIG_MKBP_USE_HOST_EVENT
#error "Config error: MKBP must not be on top of host event"
#endif
	host_events_send_mkbp_event(events);
#endif  /* CONFIG_MKBP_EVENT */
}

void host_set_single_event(enum host_event_code event)
{
	host_event_t ev = 0;

	host_event_set_bit(&ev, event);
	host_set_events(ev);
}

int host_is_event_set(enum host_event_code event)
{
	host_event_t ev = 0;

	host_event_set_bit(&ev, event);
	return events & ev;
}

void host_clear_events(host_event_t mask)
{
	/* ignore host events the rest of board doesn't care about */
#ifdef CONFIG_HOST_EVENT64
	mask &= CONFIG_HOST_EVENT64_REPORT_MASK;
#else
	mask &= CONFIG_HOST_EVENT_REPORT_MASK;
#endif

	/* return early if nothing changed */
	if (!(events & mask))
		return;

	HOST_EVENT_CPRINTS("event clear", mask);

	host_events_atomic_clear(&events, mask);

	*(host_event_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = events;
#ifdef CONFIG_MKBP_EVENT
	host_events_send_mkbp_event(events);
#endif
}

static int host_get_next_event(uint8_t *out)
{
	uint32_t event_out = (uint32_t)events;
	memcpy(out, &event_out, sizeof(event_out));
	host_events_atomic_clear(&events, event_out);
	*(host_event_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = events;
	return sizeof(event_out);
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_HOST_EVENT, host_get_next_event);

#ifdef CONFIG_HOST_EVENT64
static int host_get_next_event64(uint8_t *out)
{
	host_event_t event_out = events;

	memcpy(out, &event_out, sizeof(event_out));
	host_events_atomic_clear(&events, event_out);
	*(host_event_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = events;
	return sizeof(event_out);
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_HOST_EVENT64, host_get_next_event64);
#endif

/**
 * Clear one or more host event bits from copy B.
 *
 * @param mask          Event bits to clear (use EC_HOST_EVENT_MASK()).
 *                      Write 1 to a bit to clear it.
 */
static void host_clear_events_b(host_event_t mask)
{
	/* Only print if something's about to change */
	if (events_copy_b & mask)
		HOST_EVENT_CPRINTS("event clear B", mask);

	host_events_atomic_clear(&events_copy_b, mask);
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

/*
 * Events copy b is used by coreboot for logging the wake reason. For this to
 * work, events_copy_b needs to be cleared on every suspend.
 */
void clear_events_copy_b(void)
{
	events_copy_b = 0;
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, clear_events_copy_b, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Console commands */
static int command_host_event(int argc, char **argv)
{
	/* Handle sub-commands */
	if (argc == 3) {
		char *e;
		host_event_t i = strtoul(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		if (!strcasecmp(argv[1], "set"))
			host_set_events(i);
		else if (!strcasecmp(argv[1], "clear"))
			host_clear_events(i);
		else if (!strcasecmp(argv[1], "clearb"))
			host_clear_events_b(i);
		else
			return EC_ERROR_PARAM1;
	}

	/* Print current SMI/SCI status */
	HOST_EVENT_CCPRINTF("Events:             ", host_get_events());
	HOST_EVENT_CCPRINTF("Events-B:           ", events_copy_b);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hostevent, command_host_event,
			"[set | clear | clearb | smi | sci | wake | always_report] [mask]",
			"Print / set host event state");

/*****************************************************************************/
/* Host commands */


static enum ec_status host_event_get_b(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_mask *r = args->response;

	r->mask = events_copy_b;
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_GET_B,
		     host_event_get_b,
		     EC_VER_MASK(0));

static enum ec_status host_event_clear(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event_mask *p = args->params;

	host_clear_events(p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_CLEAR,
		     host_event_clear,
		     EC_VER_MASK(0));

static enum ec_status host_event_clear_b(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event_mask *p = args->params;

	host_clear_events_b(p->mask);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT_CLEAR_B,
		     host_event_clear_b,
		     EC_VER_MASK(0));

static enum ec_status host_event_action_get(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event *r = args->response;
	const struct ec_params_host_event *p = args->params;
	int result = EC_RES_SUCCESS;

	args->response_size = sizeof(*r);
	memset(r, 0, sizeof(*r));

	switch (p->mask_type) {
	case EC_HOST_EVENT_B:
		r->value = events_copy_b;
		break;
	default:
		result = EC_RES_INVALID_PARAM;
		break;
	}

	return result;
}

static enum ec_status host_event_action_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event *p = args->params;
	int result = EC_RES_SUCCESS;
	host_event_t mask_value __unused = (host_event_t)(p->value);

	switch (p->mask_type) {
	default:
		result = EC_RES_INVALID_PARAM;
		break;
	}

	return result;
}

static enum ec_status
host_event_action_clear(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event *p = args->params;
	int result = EC_RES_SUCCESS;
	host_event_t mask_value = (host_event_t)(p->value);

	switch (p->mask_type) {
	case EC_HOST_EVENT_MAIN:
		host_clear_events(mask_value);
		break;
	case EC_HOST_EVENT_B:
		host_clear_events_b(mask_value);
		break;
	default:
		result = EC_RES_INVALID_PARAM;
	}

	return result;
}

static enum ec_status
host_command_host_event(struct host_cmd_handler_args *args)
{
	const struct ec_params_host_event *p = args->params;

	args->response_size = 0;

	switch (p->action) {
	case EC_HOST_EVENT_GET:
		return host_event_action_get(args);
	case EC_HOST_EVENT_SET:
		return host_event_action_set(args);
	case EC_HOST_EVENT_CLEAR:
		return host_event_action_clear(args);
	default:
		return EC_RES_INVALID_PARAM;
	}
}

DECLARE_HOST_COMMAND(EC_CMD_HOST_EVENT,
		     host_command_host_event,
		     EC_VER_MASK(0));

#define LAZY_WAKE_MASK_SYSJUMP_TAG		0x4C4D /* LM - Lazy Mask*/
#define LAZY_WAKE_MASK_HOOK_VERSION		1
