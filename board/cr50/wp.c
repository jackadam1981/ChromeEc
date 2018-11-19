/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ccd_config.h"
#include "console.h"
#include "crc8.h"
#include "extension.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "scratch_reg1.h"
#include "system.h"
#include "system_chip.h"
#include "tpm_nvmem.h"
#include "tpm_nvmem_ops.h"
#include "tpm_registers.h"
#include "util.h"
#include "physical_presence.h"

#define CPRINTS(format, args...) cprints(CC_RBOX, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_RBOX, format, ## args)

#ifdef CR50_DEV
/*
 * Under development mode, use a shorter timer to prove physical presence
 */
#define BATT_REMOVE_PHYSICAL_PRESENCE_TIME	(30 * SECOND)
#define BUTTON_PRESS_TIMEOUT			(30 * SECOND)
#else
#define BUTTON_PRESS_TIMEOUT			(5 * MINUTE)
#define BATT_REMOVE_PHYSICAL_PRESENCE_TIME	(2 * MINUTE)
#endif

/* Board user ownership (UO) states */
enum board_uo_state {
	BOARD_UO_NOT_OWNED,	/* Default state, must be first */
	BOARD_UO_BATT_REMOVAL_WAIT,
	BOARD_UO_BUTTON_PRESS_WAIT,
	BOARD_UO_FAILED,
	BOARD_UO_USER_OWNED,

	BOARD_UO_STATES,	/* must be last */
};

/*
 * For Chromebooks that take less than 2 minutes to open, the board
 * properties set the BOARD_WP_DISABLE_DELAY flag.  This forces
 * a 2 minute delay after battery removal before disabling the
 * write protect to the SPI.
 */
static timestamp_t batt_removal_time;
static timestamp_t button_press_timeout;

enum board_uo_state current_uo_state;

/**
 * Return non-zero if battery is present.  Always reflects
 * the current battery state.
 */
static int board_battery_is_present(void)
{
	/* Invert because battery-present signal is active low */
	return !gpio_get_level(GPIO_BATT_PRES_L);
}

static void batt_removal_done_async(void)
{
	CPRINTS("Battery removal complete, enabling unlock");

	/*
	 * All physical presence requirements met, transition to the
	 * USER_OWNED state.
	 */
	current_uo_state = BOARD_UO_USER_OWNED;
}

/**
 * Return non-zero if the user has proven they are physically present
 * with the unit.
 *
 * For most systems, removal of the battery is sufficient as the case
 * takes more than 2 minutes to open.  On other systems, this routine
 * enforces a 2 minute delay after battery removal and requires the user to
 * press the power button 5 times quickly following the battery removal
 * delay.
 *
 * @return 1 if user physical presence requirements met, 0 otherwise
 *
 */
int board_user_has_ownership(void)
{
	timestamp_t current_time;
	int rv;
	int bp = board_battery_is_present();
	int user_has_ownership;

	/* Battery is present implies no physical ownership */
	if (bp)	{
		/* Abort/cleanup any board ownership actions */
		if (current_uo_state == BOARD_UO_BATT_REMOVAL_WAIT)
			CPRINTS("Battery inserted, unlock aborted");

		if (current_uo_state == BOARD_UO_BUTTON_PRESS_WAIT) {
			/*
			 * Stop the physical button check that was
			 * initiated by this module.
			 */
			CPRINTS("Battery inserted, physical detect abort");
			physical_detect_abort();
		}

		if (ccd_get_flag(CCD_FLAG_MET_OWNERSHIP_REQ)) {
			int rv;
			/*
			 * Clear CCD_FLAG_MET_OWNERSHIP_REQ due to battery
			 * insertion.  The user must restart the process
			 * to enable an additional unlock.
			 *
			 * ccd_set_flag() can fail, especially if the TPM
			 * is using the NVMEM.  However, in this case the
			 * CCD_FLAG_MET_OWNERSHIP_REQ flag will remain set
			 * and this code will attempt to clear the flag
			 * the next time HOOK_SECOND runs.
			 */
			rv = ccd_set_flag(CCD_FLAG_MET_OWNERSHIP_REQ, 0);
			if (rv == EC_SUCCESS)
				CPRINTS("Ownership state cleared");
		}

		if (board_wp_delay_required())
			enable_deep_sleep(DEEP_SLEEP_MASK_BOARD_OWNERSHIP);

		current_uo_state = BOARD_UO_NOT_OWNED;
		batt_removal_time.val = 0;
		button_press_timeout.val = 0;
		return 0;
	}

	/*
	 * Battery removal immediately indicates user ownership if the board
	 * is hard to open.
	 *
	 * The CCD_FLAG_MET_OWNERSHIP_REQ stored in NVMEM indicates that the
	 * user proved ownership on a previous boot and the battery is still
	 * removed.
	 */
	if (!board_wp_delay_required() ||
	    ccd_get_flag(CCD_FLAG_MET_OWNERSHIP_REQ))
		current_uo_state = BOARD_UO_USER_OWNED;

	/*
	 * Board options require a delay after battery removal
	 * removal is detected prior to allowing actions that require
	 * a physical presence/
	 */
	current_time = get_time();

	/* All intermediate states indicate no ownership */
	user_has_ownership = 0;

	switch (current_uo_state) {
	case BOARD_UO_USER_OWNED:
		if ((board_wp_delay_required()) &&
		    !ccd_get_flag(CCD_FLAG_MET_OWNERSHIP_REQ)) {
			int rv;
			/*
			 * Set the CCD_FLAG_MET_OWNERSHIP_REQ.  This prevents
			 * the physical presence check process from restarting
			 * following a reboot or power cycle.
			 *
			 * The TPM reset code temporarily locks the NVMEM
			 * after detecting an AP power on.  The ccd_set_flag()
			 * will fail with EC_ERROR_BUSY until the TPM finishes
			 * starting up.  The CCD_FLAG_MET_OWNERSHIP_REQ flag
			 * remains clear until ccd_set_flag() is successful,
			 * so this state will continue trying to save the
			 * ownership state each time HOOK_SECOND runs.
			 */
			rv = ccd_set_flag(
				CCD_FLAG_MET_OWNERSHIP_REQ, 1);

			if (rv == EC_SUCCESS)
				CPRINTS("Ownership state saved");
		}
		user_has_ownership = 1;
		enable_deep_sleep(DEEP_SLEEP_MASK_BOARD_OWNERSHIP);
		break;

	case BOARD_UO_FAILED:
	default:
		/*
		 * The user failed to press the power button following the
		 * battery removal delay.  This state is also possible if the
		 * user had started a CCD open and then removed the battery.
		 */
		enable_deep_sleep(DEEP_SLEEP_MASK_BOARD_OWNERSHIP);
		break;

	case BOARD_UO_NOT_OWNED:
		/* New transition of battery presence TRUE to FALSE */
		batt_removal_time.val = current_time.val +
				BATT_REMOVE_PHYSICAL_PRESENCE_TIME;

		CPRINTS("Battery Removal: %d seconds delay before unlock",
				BATT_REMOVE_PHYSICAL_PRESENCE_TIME / SECOND);

		current_uo_state = BOARD_UO_BATT_REMOVAL_WAIT;

		disable_deep_sleep(DEEP_SLEEP_MASK_BOARD_OWNERSHIP);

		break;

	case BOARD_UO_BATT_REMOVAL_WAIT:
		if (timestamp_expired(batt_removal_time, &current_time)) {
			/*
			 * Battery removal time expired. Now require the user
			 * to tap the power button to prove ownership.  Use the
			 * existing physical detect with the short format which
			 * only requires 5 power button presses within 5
			 * seconds.
			 *
			 * TODO: add a new physical detection type that
			 * requires 5 button presses within 5 seconds, but has
			 * an overall timeout of several minutes.
			 */
			CPRINTF("Battery removal requirement met");
			rv = physical_detect_start(PP_DETECT_EXTENDED,
				batt_removal_done_async);
			if (rv != EC_SUCCESS) {
				/*
				 * Physical presence check via the power button
				 * is already in progress, mark the battery
				 * removal process as failed.  This state will
				 * only be cleared by a battery insertion or a
				 * reboot.
				 */
				CPRINTF("Physical detect already in progress, "
					"battery removal unlock failed");
				current_uo_state = BOARD_UO_FAILED;
			} else {
				button_press_timeout.val = current_time.val +
					BUTTON_PRESS_TIMEOUT;

				current_uo_state = BOARD_UO_BUTTON_PRESS_WAIT;
			}
		}
		break;

	case BOARD_UO_BUTTON_PRESS_WAIT:
		if (timestamp_expired(button_press_timeout, &current_time)) {
			/*
			 * Timeout waiting for the user to press the
			 * power button to complete proof of ownership
			 */
			physical_detect_abort();
			current_uo_state = BOARD_UO_FAILED;
		}
		break;
	}

	return user_has_ownership;
}

#ifdef CR50_DEV

static const char * const user_ownership_state[] = {
	[BOARD_UO_NOT_OWNED]		= "not_owned",
	[BOARD_UO_BATT_REMOVAL_WAIT]	= "batt_removal_wait",
	[BOARD_UO_BUTTON_PRESS_WAIT]	= "button_press_wait",
	[BOARD_UO_FAILED]		= "failed",
	[BOARD_UO_USER_OWNED]		= "user_owned",
};
BUILD_ASSERT(ARRAY_SIZE(user_ownership_state) == BOARD_UO_STATES);

static int command_bp(int argc, char **argv)
{
	int bp = board_battery_is_present();

	if (argc > 1) {
		/*
		 * For development builds only, override the board properties
		 * to enable the WP disable delay.
		 */
		if (strncasecmp(argv[1], "force", strlen("force")) == 0)
			board_properties_enable_wp_delay();
	}

	CPRINTF("Battery present state    = %d\n", bp);
	CPRINTF("Board WP Delay           = %d\n", board_wp_delay_required());
	CPRINTF("Current UO state         = %d - %s\n", current_uo_state,
		user_ownership_state[current_uo_state]);
	CPRINTF("CCD Flag Met Ownership   = %d\n",
		ccd_get_flag(CCD_FLAG_MET_OWNERSHIP_REQ));
	CPRINTF("Current time             = %d\n", (get_time().val/SECOND));
	CPRINTF("Battery Removal Time     = %d\n",
		(batt_removal_time.val/SECOND));
	CPRINTF("Button Press Timeout     = %d\n",
		(button_press_timeout.val/SECOND));

	return EC_SUCCESS;
}

DECLARE_SAFE_CONSOLE_COMMAND(bp, command_bp,
			     "[force]",
			     "Get the battery state and user ownership info");
#endif

/**
 * Set the current write protect state in RBOX and long life scratch register.
 *
 * @param asserted: 0 to disable write protect, otherwise enable write protect.
 */
static void set_wp_state(int asserted)
{
	/* Enable writing to the long life register */
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 1);

	if (asserted) {
		GREG32(PMU, LONG_LIFE_SCRATCH1) |= BOARD_WP_ASSERTED;
		GREG32(RBOX, EC_WP_L) = 0;
	} else {
		GREG32(PMU, LONG_LIFE_SCRATCH1) &= ~BOARD_WP_ASSERTED;
		GREG32(RBOX, EC_WP_L) = 1;
	}

	/* Disable writing to the long life register */
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 0);
}

/**
 * Return the current WP state
 *
 * @return 0 if WP deasserted, 1 if WP asserted
 */
int wp_is_asserted(void)
{
	/* Signal is active low, so invert */
	return !GREG32(RBOX, EC_WP_L);
}

static void check_wp_battery_presence(void)
{
	int user_present = board_user_has_ownership();

	/* If we're forcing WP, ignore battery detect/physical presence */
	if (GREG32(PMU, LONG_LIFE_SCRATCH1) & BOARD_FORCING_WP)
		return;

	/* Otherwise, set WP state the opposite of the user ownership state */
	if (user_present == wp_is_asserted()) {
		CPRINTS("WP %d", !user_present);
		set_wp_state(!user_present);
	}
}
DECLARE_HOOK(HOOK_SECOND, check_wp_battery_presence, HOOK_PRIO_DEFAULT);

/**
 * Force write protect state or follow battery presence.
 *
 * @param force: Force write protect to wp_en if non-zero, otherwise use battery
 *               presence as the source.
 * @param wp_en: 0: Deassert WP. 1: Assert WP.
 */
static void force_write_protect(int force, int wp_en)
{
	/* Enable writing to the long life register */
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 1);

	if (force) {
		/* Force WP regardless of battery presence. */
		GREG32(PMU, LONG_LIFE_SCRATCH1) |= BOARD_FORCING_WP;
	} else {
		/* Stop forcing write protect. */
		GREG32(PMU, LONG_LIFE_SCRATCH1) &= ~BOARD_FORCING_WP;
		/* Set write protect to opposite the user ownership state */
		wp_en = !board_user_has_ownership();
	}

	/* Disable writing to the long life register */
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 0);

	/* Update the WP state. */
	set_wp_state(wp_en);
}

static enum vendor_cmd_rc vc_set_wp(enum vendor_cmd_cc code,
				    void *buf,
				    size_t input_size,
				    size_t *response_size)
{
	uint8_t response = 0;

	*response_size = 0;
	/* There shouldn't be any args */
	if (input_size)
		return VENDOR_RC_BOGUS_ARGS;

	/* Get current wp settings */
	if (GREG32(PMU, LONG_LIFE_SCRATCH1) & BOARD_FORCING_WP)
		response |= WPV_FORCE;
	if (wp_is_asserted())
		response |= WPV_ENABLE;
	/* Get atboot wp settings */
	if (ccd_get_flag(CCD_FLAG_OVERRIDE_WP_AT_BOOT)) {
		response |= WPV_ATBOOT_SET;
		if (ccd_get_flag(CCD_FLAG_OVERRIDE_WP_STATE_ENABLED))
			response |= WPV_ATBOOT_ENABLE;
	}
	((uint8_t *)buf)[0] = response;
	*response_size = sizeof(response);
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_WP, vc_set_wp);

static int command_wp(int argc, char **argv)
{
	int val = 1;
	int forced = 1;

	if (argc > 1) {
		/* Make sure we're allowed to override WP settings */
		if (!ccd_is_cap_enabled(CCD_CAP_OVERRIDE_WP))
			return EC_ERROR_ACCESS_DENIED;

		/* Update WP */
		if (strncasecmp(argv[1], "follow_batt_pres", 16) == 0)
			forced = 0;
		else if (parse_bool(argv[1], &val))
			forced = 1;
		else
			return EC_ERROR_PARAM1;

		force_write_protect(forced, val);

		if (argc > 2 && !strcasecmp(argv[2], "atboot")) {
			/* Change override at boot to match */
			ccd_set_flag(CCD_FLAG_OVERRIDE_WP_AT_BOOT, forced);
			ccd_set_flag(CCD_FLAG_OVERRIDE_WP_STATE_ENABLED, val);
		}
	}

	forced = GREG32(PMU, LONG_LIFE_SCRATCH1) & BOARD_FORCING_WP;
	ccprintf("Flash WP: %s%s\n", forced ? "forced " : "",
		 wp_is_asserted() ? "enabled" : "disabled");

	ccprintf(" at boot: ");
	if (ccd_get_flag(CCD_FLAG_OVERRIDE_WP_AT_BOOT))
		ccprintf("forced %s\n",
			 ccd_get_flag(CCD_FLAG_OVERRIDE_WP_STATE_ENABLED)
			 ? "enabled" : "disabled");
	else
		ccprintf("follow_batt_pres\n");

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(wp, command_wp,
			     "[<BOOLEAN>/follow_batt_pres [atboot]]",
			     "Get/set the flash HW write-protect signal");

void set_wp_follow_ccd_config(void)
{
	if (ccd_get_flag(CCD_FLAG_OVERRIDE_WP_AT_BOOT)) {
		/* Reset to at-boot state specified by CCD */
		force_write_protect(1, ccd_get_flag
				    (CCD_FLAG_OVERRIDE_WP_STATE_ENABLED));
	} else {
		/* Reset to WP based on battery-present (val is ignored) */
		force_write_protect(0, 1);
	}
}

void init_wp_state(void)
{
	/* Check system reset flags after CCD config is initially loaded */
	if ((system_get_reset_flags() & RESET_FLAG_HIBERNATE) &&
	    !system_rollback_detected()) {
		/*
		 * Deep sleep resume without rollback, so reload the WP state
		 * that was saved to the long-life registers before the deep
		 * sleep instead of going back to the at-boot default.
		 */
		if (GREG32(PMU, LONG_LIFE_SCRATCH1) & BOARD_FORCING_WP) {
			/* Temporarily forcing WP */
			set_wp_state(GREG32(PMU, LONG_LIFE_SCRATCH1) &
				     BOARD_WP_ASSERTED);
		} else {
			/* Write protected if battery is present */
			set_wp_state(!board_user_has_ownership());
		}
	} else {
		set_wp_follow_ccd_config();
	}
}

/**
 * Wipe the TPM
 *
 * @return EC_SUCCESS, or non-zero if error.
 */
int board_wipe_tpm(void)
{
	int rc;

	/*
	 * Blindly zapping the TPM space while the AP is awake and poking at
	 * it will bork the TPM task and the AP itself, so force the whole
	 * system off by holding the EC in reset.
	 */
	CPRINTS("%s: force EC off", __func__);
	assert_ec_rst();

	/* Wipe the TPM's memory and reset the TPM task. */
	rc = tpm_reset_request(1, 1);
	if (rc != EC_SUCCESS) {
		/*
		 * If anything goes wrong (which is unlikely), we REALLY don't
		 * want to unlock the console. It's possible to fail without
		 * the TPM task ever running, so rebooting is probably our best
		 * bet for fixing the problem.
		 */
		CPRINTS("%s: Couldn't wipe nvmem! (rc %d)", __func__, rc);
		cflush();
		system_reset(SYSTEM_RESET_MANUALLY_TRIGGERED |
			     SYSTEM_RESET_HARD);

		/*
		 * That should never return, but if it did, release EC reset
		 * and pass through the error we got.
		 */
		deassert_ec_rst();
		return rc;
	}

	CPRINTS("TPM is erased");

	/* Tell the TPM task to re-enable NvMem commits. */
	tpm_reinstate_nvmem_commits();

	/* Let the rest of the system boot. */
	CPRINTS("%s: release EC reset", __func__);
	deassert_ec_rst();

	return EC_SUCCESS;
}

/****************************************************************************/
/* Verified boot TPM NVRAM space support */

/*
 * These definitions and the structure layout were manually copied from
 * src/platform/vboot_reference/firmware/lib/include/rollback_index.h. at
 * git sha c7282f6.
 */
#define FWMP_HASH_SIZE		    32
#define FWMP_DEV_DISABLE_CCD_UNLOCK (1 << 6)
#define FIRMWARE_FLAG_DEV_MODE      0x02

struct RollbackSpaceFirmware {
	/* Struct version, for backwards compatibility */
	uint8_t struct_version;
	/* Flags (see FIRMWARE_FLAG_* above) */
	uint8_t flags;
	/* Firmware versions */
	uint32_t fw_versions;
	/* Reserved for future expansion */
	uint8_t reserved[3];
	/* Checksum (v2 and later only) */
	uint8_t crc8;
} __packed;

/* Firmware management parameters */
struct RollbackSpaceFwmp {
	/* CRC-8 of fields following struct_size */
	uint8_t crc;
	/* Structure size in bytes */
	uint8_t struct_size;
	/* Structure version */
	uint8_t struct_version;
	/* Reserved; ignored by current reader */
	uint8_t reserved0;
	/* Flags; see enum fwmp_flags */
	uint32_t flags;
	/* Hash of developer kernel key */
	uint8_t dev_key_hash[FWMP_HASH_SIZE];
} __packed;

#ifndef CR50_DEV
static int lock_enforced(const struct RollbackSpaceFwmp *fwmp)
{
	uint8_t crc;

	/* Let's verify that the FWMP structure makes sense. */
	if (fwmp->struct_size != sizeof(*fwmp)) {
		CPRINTS("%s: fwmp size mismatch (%d)\n", __func__,
			fwmp->struct_size);
		return 1;
	}

	crc = crc8(&fwmp->struct_version, sizeof(struct RollbackSpaceFwmp) -
		   offsetof(struct RollbackSpaceFwmp, struct_version));
	if (fwmp->crc != crc) {
		CPRINTS("%s: fwmp crc mismatch\n", __func__);
		return 1;
	}

	return !!(fwmp->flags & FWMP_DEV_DISABLE_CCD_UNLOCK);
}
#endif

int board_fwmp_allows_unlock(void)
{
#ifdef CR50_DEV
	return 1;
#else
	/* Let's see if FWMP disables console activation. */
	struct RollbackSpaceFwmp fwmp;
	int allows_unlock;

	switch (read_tpm_nvmem(FWMP_NV_INDEX,
			       sizeof(struct RollbackSpaceFwmp), &fwmp)) {
	default:
		/* Something is messed up, let's not allow console unlock. */
		allows_unlock = 0;
		break;

	case tpm_read_not_found:
		allows_unlock = 1;
		break;

	case tpm_read_success:
		allows_unlock = !lock_enforced(&fwmp);
		break;
	}

	CPRINTS("Console unlock %sallowed", allows_unlock ? "" : "not ");

	return allows_unlock;
#endif
}

int board_vboot_dev_mode_enabled(void)
{
	struct RollbackSpaceFirmware fw;

	if (tpm_read_success ==
	    read_tpm_nvmem(FIRMWARE_NV_INDEX, sizeof(fw), &fw)) {
		return !!(fw.flags & FIRMWARE_FLAG_DEV_MODE);
	}

	/* If not found or other error, assume dev mode is disabled */
	return 0;
}

/****************************************************************************/
/* TPM vendor-specific commands */

static enum vendor_cmd_rc vc_lock(enum vendor_cmd_cc code,
				  void *buf,
				  size_t input_size,
				  size_t *response_size)
{
	uint8_t *buffer = buf;

	if (code == VENDOR_CC_GET_LOCK) {
		/*
		 * Get the state of the console lock.
		 *
		 *   Args: none
		 *   Returns: one byte; true (locked) or false (unlocked)
		 */
		if (input_size != 0) {
			*response_size = 0;
			return VENDOR_RC_BOGUS_ARGS;
		}

		buffer[0] = console_is_restricted() ? 0x01 : 0x00;
		*response_size = 1;
		return VENDOR_RC_SUCCESS;
	}

	/* I have no idea what you're talking about */
	*response_size = 0;
	return VENDOR_RC_NO_SUCH_COMMAND;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_GET_LOCK, vc_lock);

/*
 * TODO(rspangler): The old concept of 'lock the console' really meant
 * something closer to 'reset CCD config', not the CCD V1 meaning of 'ccdlock'.
 * This command is no longer supported, so will fail.  It was defined this
 * way:
 *
 * DECLARE_VENDOR_COMMAND(VENDOR_CC_SET_LOCK, vc_lock);
 */
