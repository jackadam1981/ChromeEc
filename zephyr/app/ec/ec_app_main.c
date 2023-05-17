/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power_interface.h"
#include "button.h"
#include "chipset.h"
#include "cros_board_info.h"
#include "ec_app_main.h"
#include "ec_tasks.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "lpc.h"
#include "system.h"
#include "vboot.h"
#include "watchdog.h"
#include "zephyr_espi_shim.h"
#include "panic_defs.h"
#include "panic.h"

#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/printk.h>

static struct k_timer no_sleep_boot_timer;
static void boot_allow_sleep(struct k_timer *timer)
{
	pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
}

/* For testing purposes this is not named main. See main_shim.c for the real
 * main() function.
 */
void ec_app_main(void)
{
	/*
	 * Initialize reset logs. This needs to be done before any updates of
	 * reset logs because we need to verify if the values remain the same
	 * after every EC reset.
	 */
	if (IS_ENABLED(CONFIG_CMD_AP_RESET_LOG)) {
		init_reset_log();
	}

	system_print_banner();

	if (IS_ENABLED(CONFIG_WATCHDOG) &&
	    !IS_ENABLED(CONFIG_WDT_DISABLE_AT_BOOT)) {
		int err = watchdog_init();
		cprints(CC_SYSTEM, "I initialized the watchdog! err %d", err);
	}

	if (IS_ENABLED(CONFIG_PLATFORM_EC_BOOT_NO_SLEEP)) {
		k_timeout_t duration =
			K_MSEC(CONFIG_PLATFORM_EC_BOOT_NO_SLEEP_MS);

		k_timer_init(&no_sleep_boot_timer, boot_allow_sleep, NULL);
		k_timer_start(&no_sleep_boot_timer, duration, K_NO_WAIT);

		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE,
					 PM_ALL_SUBSTATES);
	}

	/*
	 * Keyboard scan init/Button init can set recovery events to
	 * indicate to host entry into recovery mode. Before this is
	 * done, LPC_HOST_EVENT_ALWAYS_REPORT mask needs to be initialized
	 * correctly.
	 */
	if (IS_ENABLED(CONFIG_HOSTCMD_X86)) {
		lpc_init_mask();
	}

	if (IS_ENABLED(HAS_TASK_KEYSCAN)) {
		keyboard_scan_init();
	}

	if (IS_ENABLED(CONFIG_DEDICATED_RECOVERY_BUTTON) ||
	    IS_ENABLED(CONFIG_VOLUME_BUTTONS)) {
		button_init();
	}

	if (IS_ENABLED(CONFIG_PLATFORM_EC_VBOOT_EFS2)) {
		/*
		 * For RO, it behaves as follows:
		 *   In recovery, it enables PD communication and returns.
		 *   In normal boot, it verifies and jumps to RW.
		 * For RW, it returns immediately.
		 */
		vboot_main();
	}
	cprints(CC_SYSTEM, "Calling init funcs");
	struct panic_data* panic_data_ptr = panic_get_data();
	cprints(CC_SYSTEM, "Panic Data PTR");
	cprints(CC_SYSTEM, "arch 0x%02x", panic_data_ptr->arch);
	cprints(CC_SYSTEM, "struct_version 0x%02x", panic_data_ptr->struct_version);
	cprints(CC_SYSTEM, "flags 0x%02x", panic_data_ptr->flags);
	cprints(CC_SYSTEM, "reserved 0x%02x", panic_data_ptr->reserved);

	for(int i =0;i<NUM_CORTEX_PANIC_REGISTERS;i++)
	{
		cprints(CC_SYSTEM, "regs[%d] 0x%08x", i, panic_data_ptr->cm.regs[i]);
	}
	for(int i =0;i<NUM_CORTEX_PANIC_FRAME_REGISTERS;i++)
	{
		cprints(CC_SYSTEM, "frame[%d] 0x%08x", i, panic_data_ptr->cm.frame[i]);
	}

	cprints(CC_SYSTEM, "eax 0x%08x", panic_data_ptr->cm.cfsr);
	cprints(CC_SYSTEM, "ebx 0x%08x", panic_data_ptr->cm.bfar);
	cprints(CC_SYSTEM, "ecx 0x%08x", panic_data_ptr->cm.mfar);
	cprints(CC_SYSTEM, "edx 0x%08x", panic_data_ptr->cm.shcsr);
	cprints(CC_SYSTEM, "esi 0x%08x", panic_data_ptr->cm.hfsr);
	cprints(CC_SYSTEM, "edi 0x%08x", panic_data_ptr->cm.dfsr);
	#if 0 // x86
	cprints(CC_SYSTEM, "Vector 0x%08x", panic_data_ptr->x86.vector);
	cprints(CC_SYSTEM, "error_code 0x%08x", panic_data_ptr->x86.error_code);
	cprints(CC_SYSTEM, "eip 0x%08x", panic_data_ptr->x86.eip);
	cprints(CC_SYSTEM, "cs 0x%08x", panic_data_ptr->x86.cs);
	cprints(CC_SYSTEM, "eflags 0x%08x", panic_data_ptr->x86.eflags);
	cprints(CC_SYSTEM, "eax 0x%08x", panic_data_ptr->x86.eax);
	cprints(CC_SYSTEM, "ebx 0x%08x", panic_data_ptr->x86.ebx);
	cprints(CC_SYSTEM, "ecx 0x%08x", panic_data_ptr->x86.ecx);
	cprints(CC_SYSTEM, "edx 0x%08x", panic_data_ptr->x86.edx);
	cprints(CC_SYSTEM, "esi 0x%08x", panic_data_ptr->x86.esi);
	cprints(CC_SYSTEM, "edi 0x%08x", panic_data_ptr->x86.edi);
	cprints(CC_SYSTEM, "task_id 0x%02x", panic_data_ptr->x86.task_id);
	#endif
	/* Call init hooks before main tasks start */
	if (IS_ENABLED(CONFIG_PLATFORM_EC_HOOKS)) {
		cprints(CC_SYSTEM, "Calling platform init hooks");
		hook_notify(HOOK_INIT);
		cprints(CC_SYSTEM, "Called platform init hooks");
	}

	/*
	 * If the EC has exclusive control over the CBI EEPROM WP signal, have
	 * the EC set the WP if appropriate.  Note that once the WP is set, the
	 * EC must be reset via EC_RST_ODL in order for the WP to become unset.
	 */
	if (IS_ENABLED(CONFIG_PLATFORM_EC_EEPROM_CBI_WP) && system_is_locked())
	{
		cprints(CC_SYSTEM, "Calling latch cbi funcs");
		cbi_latch_eeprom_wp();
	}

	/*
	 * Print the init time.  Not completely accurate because it can't take
	 * into account the time before timer_init(), but it'll at least catch
	 * the majority of the time.
	 */
	cprints(CC_SYSTEM, "Inits done");

	/* Start the EC tasks after performing all main initialization */
	if (IS_ENABLED(CONFIG_SHIMMED_TASKS)) {
		cprints(CC_SYSTEM, "Starting EC task");
		start_ec_tasks();
	}
	if (IS_ENABLED(CONFIG_AP_PWRSEQ)) {
		ap_pwrseq_task_start();
	}
}
