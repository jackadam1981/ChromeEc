/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __POWER_HOST_SLEEP_H
#define __POWER_HOST_SLEEP_H

/*
 * This file is for Zephyr ap_pwrseq to reuse legacy EC code.
 * Eventually this file should be removed.
 *
 * TODO: Any declaration in this file should be removed once equivalents
 * are defined in Zephyr code.
 */

#if defined(CONFIG_AP_PWRSEQ)

/*
 * From ec_commands.h
 * Host event codes. ACPI query EC command uses code 0 to mean "no event
 * pending".  We explicitly specify each value in the enum listing so they won't
 * change if we delete/insert an item or rearrange the list (it needs to be
 * stable across platforms, not just within a single compiled instance).
 */
enum host_event_code {
	EC_HOST_EVENT_LID_OPEN = 2,
	EC_HOST_EVENT_MODE_CHANGE = 29
};

/* Host event mask */
#define EC_HOST_EVENT_MASK(event_code) BIT64((event_code) - 1)

enum host_sleep_event {
	HOST_SLEEP_EVENT_DEFAULT_RESET = 0,
	HOST_SLEEP_EVENT_S3_SUSPEND   = 1,
	HOST_SLEEP_EVENT_S3_RESUME    = 2,
	HOST_SLEEP_EVENT_S0IX_SUSPEND = 3,
	HOST_SLEEP_EVENT_S0IX_RESUME  = 4,
	/* S3 suspend with additional enabled wake sources */
	HOST_SLEEP_EVENT_S3_WAKEABLE_SUSPEND = 5,
};

/********************************************************************/
/* power.h */
enum power_state {
	/* Steady states */
	POWER_G3 = 0,	/*
			 * System is off (not technically all the way into G3,
			 * which means totally unpowered...)
			 */
	POWER_S5,		/* System is soft-off */
	POWER_S4,		/* System is suspended to disk */
	POWER_S3,		/* Suspend; RAM on, processor is asleep */
	POWER_S0,		/* System is on */
#if defined(CONFIG_PLATFORM_EC_POWERSEQ_S0IX)
	POWER_S0ix,
#endif
	/* Transitions */
	POWER_G3S5,	/* G3 -> S5 (at system init time) */
	POWER_S5S3,	/* S5 -> S3 (skips S4 on non-Intel systems) */
	POWER_S3S0,	/* S3 -> S0 */
	POWER_S0S3,	/* S0 -> S3 */
	POWER_S3S5,	/* S3 -> S5 (skips S4 on non-Intel systems) */
	POWER_S5G3,	/* S5 -> G3 */
	POWER_S3S4,	/* S3 -> S4 */
	POWER_S4S3,	/* S4 -> S3 */
	POWER_S4S5,	/* S4 -> S5 */
	POWER_S5S4,	/* S5 -> S4 */
#if defined(CONFIG_PLATFORM_EC_POWERSEQ_S0IX)
	POWER_S0ixS0,   /* S0ix -> S0 */
	POWER_S0S0ix,   /* S0 -> S0ix */
#endif
};

#if defined(CONFIG_PLATFORM_EC_POWERSEQ_HOST_SLEEP)
/* Context to pass to a host sleep command handler. */
struct host_sleep_event_context {
	uint32_t sleep_transitions; /* Number of sleep transitions observed */
	uint16_t sleep_timeout_ms;  /* Timeout in milliseconds */
};

void ap_power_chipset_handle_host_sleep_event(
		enum host_sleep_event state,
		struct host_sleep_event_context *ctx);
enum host_sleep_event power_get_host_sleep_state(void);
void power_set_host_sleep_state(enum host_sleep_event state);
#endif /* CONFIG_PLATFORM_EC_POWERSEQ_HOST_SLEEP */

/* host_command.h */
typedef uint64_t host_event_t;
extern uint8_t lpc_is_active_wm_set_by_host(void);
extern int get_lazy_wake_mask(enum power_state state, host_event_t *mask);

/* lpc.h */
/* Types of host events */
enum lpc_host_event_type {
	LPC_HOST_EVENT_SMI = 0,
	LPC_HOST_EVENT_SCI,
	LPC_HOST_EVENT_WAKE,
	LPC_HOST_EVENT_ALWAYS_REPORT,
	LPC_HOST_EVENT_COUNT,
};
extern host_event_t lpc_get_host_event_mask(
	enum lpc_host_event_type type);
extern void lpc_set_host_event_mask(
	enum lpc_host_event_type type, host_event_t mask);

#endif /* CONFIG_AP_PWRSEQ */

#endif /* __POWER_HOST_SLEEP_H */
