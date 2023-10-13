/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AP hang detect logic */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "power_button.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)

static struct ec_params_hang_detect hdparams;

static uint8_t bootstatus = EC_HANG_DETECT_AP_BOOT_NORMAL;

/**
 * hang detect handlers for reboot.
 */
static void hang_detect_reboot(void)
{
	/* If we're rebooting the AP, stop hang detection */
	CPRINTS("hang detect triggering reboot");
	host_set_single_event(EC_HOST_EVENT_HANG_REBOOT);
	chipset_reset(CHIPSET_RESET_HANG_REBOOT);
	/* if EC rebooted AP due to watchdog timeout, set bootstatus to 1 */
	bootstatus = 1;
}
DECLARE_DEFERRED(hang_detect_reboot);

static void hang_detect_start_reboot(const char *why)
{
	if (hdparams.reboot_timeout_msec) {
		CPRINTS("hang detect started on %s", why);

		hook_call_deferred(&hang_detect_reboot_data,
				   hdparams.reboot_timeout_msec * MSEC);
	}
}

static void hang_detect_cancel_reboot(const char *why)
{
	CPRINTS("hang detect stop on %s", why);
	hook_call_deferred(&hang_detect_reboot_data, -1);
}

/*****************************************************************************/
/* Hooks */

static void hang_detect_resume(void)
{
	if (hdparams.flags & EC_HANG_START_ON_RESUME)
		hang_detect_start_reboot("resume");
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, hang_detect_resume, HOOK_PRIO_DEFAULT);

static void hang_detect_suspend(void)
{
	if (hdparams.flags & EC_HANG_STOP_ON_SUSPEND)
		hang_detect_cancel_reboot("suspend");
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, hang_detect_suspend, HOOK_PRIO_DEFAULT);

static void hang_detect_shutdown(void)
{
	/* Stop the timers */
	hang_detect_cancel_reboot("shutdown/reset");

	/* Disable hang detection; it must be enabled every boot */
	memset(&hdparams, 0, sizeof(hdparams));
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, hang_detect_shutdown, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESET, hang_detect_shutdown, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Host command */

static int
hang_detect_host_command(struct host_cmd_handler_args *args)
{
	const struct ec_params_hang_detect *p = args->params;
	struct ec_params_hang_detect_resp *r = args->response;

	/* Handle starting hang timer on request */
	if (p->flags & EC_HANG_START_NOW) {
		hang_detect_start_reboot("ap request");
		/* Ignore the other params */
		return EC_RES_SUCCESS;
	}

	/* Handle stopping hang timer on request */
	if (p->flags & EC_HANG_STOP_NOW) {
		hang_detect_cancel_reboot("ap request");
		/* Ignore the other params */
		return EC_RES_SUCCESS;
	}

	/* AP is asking if EC has rebooted it */
	if (p->flags & EC_GET_HANG_STATUS) {
		enum chipset_shutdown_reason ec_reason =
			chipset_get_shutdown_reason();
		args->response_size = sizeof(*r);
		/**
		 * chipset_get_shutdown_reason() provides the last reason the EC
		 * has rebooted AP. It is not aware of any AP-initiated reboot
		 * or shutdown. For example, if EC-watchdog triggered the AP
		 * reboot and later the AP was powered off or rebooted (e.g.
		 * with reboot command or powered-off in UI) the
		 * chipset_get_shutdown_reason() will still return the
		 * CHIPSET_RESET_HANG_REBOOT as the last reset reason. To
		 * address this issue, the watchdog kernel module has a shutdown
		 * callback that sends EC_CMD_HANG_DETECT with
		 * EC_CLEAR_HANG_STATUS set every time the AP is shutting down
		 * or rebooting gracefully (gracefully here means "not triggered
		 * by watchdog") to inform that AP is closing normally.
		 */
		if (ec_reason == CHIPSET_RESET_HANG_REBOOT &&
		    bootstatus == EC_HANG_DETECT_AP_BOOT_EC_WTD)
			r->status = EC_HANG_DETECT_AP_BOOT_EC_WTD;
		else
			r->status = EC_HANG_DETECT_AP_BOOT_NORMAL;
		CPRINTS("EC Watchdog status %d", r->status);
		/* Ignore the other params */
		return EC_RES_SUCCESS;
	}

	if (p->flags & EC_CLEAR_HANG_STATUS) {
		CPRINTS("Clearing bootstatus - AP is shutting down gracefully");
		bootstatus = EC_HANG_DETECT_AP_BOOT_NORMAL;
		return EC_RES_SUCCESS;
	}

	/* If hang detect transitioning to disabled, stop timers */
	if (hdparams.flags && !p->flags) {
		hang_detect_cancel_reboot("ap flags=0");
	}

	/* Save new params */
	hdparams = *p;
	CPRINTS("hang detect flags=0x%x, reboot=%d ms", hdparams.flags,
		hdparams.reboot_timeout_msec);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HANG_DETECT, hang_detect_host_command,
		     EC_VER_MASK(0));

/*****************************************************************************/
/* Console command */

static int command_hang_detect(int argc, char **argv)
{
	ccprintf("flags:  0x%x\n", hdparams.flags);

	ccputs("reboot: ");
	if (hdparams.reboot_timeout_msec)
		ccprintf("%d ms\n", hdparams.reboot_timeout_msec);
	else
		ccputs("disabled\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hangdet, command_hang_detect, NULL,
			"Print hang detect state");
