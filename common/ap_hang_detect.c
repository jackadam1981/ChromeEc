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

/* Console output macro */
#define CPRINTS(format, args...) cprints(CC_CHIPSET, "APHD: " format, ##args)

static struct ec_hang_detect_params hdparams;
static uint8_t bootstatus = EC_HANG_DETECT_AP_BOOT_NORMAL;

/**
 * hang detect handlers for reboot.
 */
static void hang_detect_reboot(void)
{
	/* If we're rebooting the AP, stop hang detection */
	CPRINTS("Triggering reboot");
	chipset_reset(CHIPSET_RESET_HANG_REBOOT);
	bootstatus = EC_HANG_DETECT_AP_BOOT_EC_WDT;
}
DECLARE_DEFERRED(hang_detect_reboot);

static void hang_detect_reload(const char *why)
{
	CPRINTS("Reloaded on %s (timeout: %ds)", why,
		hdparams.reboot_timeout_sec);
	hook_call_deferred(&hang_detect_reboot_data,
			   hdparams.reboot_timeout_sec * SECOND);
}

static void hang_detect_cancel(const char *why)
{
	CPRINTS("Stop on %s", why);
	hook_call_deferred(&hang_detect_reboot_data, -1);
}

/*****************************************************************************/
/* Hooks */

static void hang_detect_resume(void)
{
	if (hdparams.args & EC_HANG_DETECT_START_ON_RESUME)
		hang_detect_reload("resume");
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, hang_detect_resume, HOOK_PRIO_DEFAULT);

static void hang_detect_suspend(void)
{
	if (hdparams.args & EC_HANG_DETECT_STOP_ON_SUSPEND)
		hang_detect_cancel("suspend");
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, hang_detect_suspend, HOOK_PRIO_DEFAULT);

static void hang_detect_shutdown(void)
{
	/* Stop the timer */
	hang_detect_cancel("shutdown/reset");

	/* Clear parameters - those must be set every boot */
	memset(&hdparams, 0, sizeof(hdparams));
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, hang_detect_shutdown, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESET, hang_detect_shutdown, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Host command */

static enum ec_status
hang_detect_host_command(struct host_cmd_handler_args *args)
{
	const struct ec_hang_detect_req *p = args->params;
	struct ec_hang_detect_resp *r = args->response;
	enum ec_status ret = EC_RES_SUCCESS;

	if (p->command != EC_HANG_DETECT_CMD_SET_PARAMS &&
	    (p->params.args != 0 || p->params.reboot_timeout_sec != 0)) {
		/* Only EC_HANG_DETECT_CMD_SET_PARAMS command can set parameters
		 */
		CPRINTS("Wrong params for command (%04x)", p->command);
		return EC_RES_INVALID_PARAM;
	}

	switch (p->command) {
	case EC_HANG_DETECT_CMD_RELOAD:
		/* Handle starting hang timer on request */
		hang_detect_reload("ap request");
		break;

	case EC_HANG_DETECT_CMD_CANCEL:
		/* Handle stopping hang timer on request */
		hang_detect_cancel("ap request");
		break;

	case EC_HANG_DETECT_CMD_SET_PARAMS:
		if ((p->params.args & ~(EC_HANG_DETECT_START_ON_RESUME |
					EC_HANG_DETECT_STOP_ON_SUSPEND)) != 0) {
			CPRINTS("Wrong args (%04x)", p->params.args);
			ret = EC_RES_INVALID_PARAM;
			break;
		}

		/* Save new params */
		hdparams = p->params;
		CPRINTS("args: %04x, reboot timeout: %d(s)", hdparams.args,
			hdparams.reboot_timeout_sec);
		break;

	case EC_HANG_DETECT_CMD_GET_STATUS:
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
		 * EC_HANG_DETECT_CMD_CLEAR_STATUS set every time the AP is
		 * shutting down or rebooting gracefully (gracefully here means
		 * "not triggered by watchdog") to inform that AP is closing
		 * normally.
		 */
		if (ec_reason == CHIPSET_RESET_HANG_REBOOT &&
		    bootstatus == EC_HANG_DETECT_AP_BOOT_EC_WDT)
			r->status = EC_HANG_DETECT_AP_BOOT_EC_WDT;
		else
			r->status = EC_HANG_DETECT_AP_BOOT_NORMAL;
		CPRINTS("EC Watchdog status %d", r->status);
		break;

	case EC_HANG_DETECT_CMD_CLEAR_STATUS:
		CPRINTS("Clearing bootstatus");
		bootstatus = EC_HANG_DETECT_AP_BOOT_NORMAL;
		break;

	default:
		CPRINTS("Unknown command (%04x)", p->command);
		ret = EC_RES_INVALID_PARAM;
		break;
	}

	return ret;
}
DECLARE_HOST_COMMAND(EC_CMD_HANG_DETECT, hang_detect_host_command,
		     EC_VER_MASK(0));

/*****************************************************************************/
/* Console command */

static int command_hang_detect(int argc, const char **argv)
{
	ccprintf("args: %04x\n", hdparams.args);
	ccprintf("reboot timeout: %d(s)\n", hdparams.reboot_timeout_sec);
	ccprintf("bootstatus: %02x\n", bootstatus);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hangdet, command_hang_detect, NULL,
			"Print hang detect state");
