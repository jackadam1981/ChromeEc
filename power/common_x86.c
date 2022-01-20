/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common X86 chipset power control module for Chrome EC */

#include "power/common_x86.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ##args)

#ifdef CONFIG_POWER_S0IX
/*
 * Backup copies of SCI and SMI mask to preserve across S0ix suspend/resume
 * cycle. If the host uses S0ix, BIOS is not involved during suspend and resume
 * operations and hence SCI/SMI masks are programmed only once during boot-up.
 *
 * These backup variables are set whenever host expresses its interest to
 * enter S0ix and then lpc_host_event_mask for SCI and SMI are cleared. When
 * host resumes from S0ix, masks from backup variables are copied over to
 * lpc_host_event_mask for SCI and SMI.
 */
static host_event_t backup_sci_mask;
static host_event_t backup_smi_mask;

/*
 * Clear host event masks for SMI and SCI when host is entering S0ix. This is
 * done to prevent any SCI/SMI interrupts when the host is in suspend. Since
 * BIOS is not involved in the suspend path, EC needs to take care of clearing
 * these masks.
 */
static void lpc_s0ix_suspend_clear_masks(void)
{
	backup_sci_mask = lpc_get_host_event_mask(LPC_HOST_EVENT_SCI);
	backup_smi_mask = lpc_get_host_event_mask(LPC_HOST_EVENT_SMI);

	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, 0);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, 0);
}

/*
 * Restore host event masks for SMI and SCI when host exits S0ix. This is done
 * because BIOS is not involved in the resume path and so EC needs to restore
 * the masks from backup variables.
 */
void lpc_s0ix_resume_restore_masks(void)
{
	/*
	 * No need to restore SCI/SMI masks if both backup_sci_mask and
	 * backup_smi_mask are zero. This indicates that there was a failure to
	 * enter S0ix(SLP_S0# assertion) and hence SCI/SMI masks were never
	 * backed up.
	 */
	if (!backup_sci_mask && !backup_smi_mask)
		return;

	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, backup_sci_mask);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, backup_smi_mask);

	backup_sci_mask = backup_smi_mask = 0;
}

void lpc_s0ix_hang_detected(void)
{
	/*
	 * Wake up the AP so they don't just chill in a non-suspended state and
	 * burn power. Overload a vaguely related event bit since event bits are
	 * at a premium. If the system never entered S0ix, then manually set the
	 * wake mask to pretend it did, so that the hang detect event wakes the
	 * system.
	 */
	if (power_get_state() == POWER_S0) {
		host_event_t sleep_wake_mask;

		get_lazy_wake_mask(POWER_S0ix, &sleep_wake_mask);
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, sleep_wake_mask);
	}

	CPRINTS("Warning: Detected sleep hang! Waking host up!");
	host_set_single_event(EC_HOST_EVENT_HANG_DETECT);
}

static void handle_chipset_suspend(void)
{
	/* Clear masks before any hooks are run for suspend. */
	lpc_s0ix_suspend_clear_masks();
}

DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, handle_chipset_suspend, HOOK_PRIO_FIRST);

static void handle_chipset_reset(void)
{
	if (chipset_in_state(CHIPSET_STATE_STANDBY)) {
		CPRINTS("chipset reset: exit s0ix");
		power_reset_host_sleep_state();
		task_wake(TASK_ID_CHIPSET);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESET, handle_chipset_reset, HOOK_PRIO_FIRST);

void power_reset_host_sleep_state(void)
{
	power_set_host_sleep_state(HOST_SLEEP_EVENT_DEFAULT_RESET);
	sleep_reset_tracking();
	power_chipset_handle_host_sleep_event(HOST_SLEEP_EVENT_DEFAULT_RESET,
					      NULL);
}

#endif /* CONFIG_POWER_S0IX */
