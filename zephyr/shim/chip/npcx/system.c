/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/cros_bbram.h>
#include <drivers/cros_system.h>
#include <logging/log.h>

#include "watchdog.h"
#include "system.h"

#define GET_BBRAM_OFFSET(node) \
	DT_PROP(DT_PATH(named_bbram_regions, node), offset)
#define GET_BBRAM_SIZE(node) DT_PROP(DT_PATH(named_bbram_regions, node), size)

LOG_MODULE_REGISTER(shim_npcx_system, LOG_LEVEL_ERR);

const struct device *bbram_dev;
const struct device *sys_dev;

void chip_save_reset_flags(uint32_t flags)
{
	if (bbram_dev == NULL) {
		LOG_ERR("bbram_dev doesn't binding");
		return;
	}

	cros_bbram_write(bbram_dev, GET_BBRAM_OFFSET(saved_reset_flags),
			 GET_BBRAM_SIZE(saved_reset_flags), (uint8_t *)&flags);
}

uint32_t chip_read_reset_flags(void)
{
	uint32_t flags;

	if (bbram_dev == NULL) {
		LOG_ERR("bbram_dev doesn't binding");
		return 0;
	}

	cros_bbram_read(bbram_dev, GET_BBRAM_OFFSET(saved_reset_flags),
			GET_BBRAM_SIZE(saved_reset_flags), (uint8_t *)&flags);

	return flags;
}

static int check_reset_cause(void)
{
	uint32_t chip_flags = 0; /* used to write back to the BBRAM */
	uint32_t system_flags = chip_read_reset_flags(); /* system reset flag */
	int chip_reset_cause = 0; /* chip-level reset cause */

	chip_reset_cause = cros_system_get_reset_cause(sys_dev);
	if (chip_reset_cause < 0) {
		LOG_ERR("read chip reset cause failed");
		return -1;
	}

	/*
	 * TODO: CONFIG_POWER_BUTTON_INIT_IDLE & CONFIG_BOARD_FORCE_RESET_PIN &
	 * hibernate
	 */

	switch (chip_reset_cause) {
	case POWERUP:
		system_flags |= EC_RESET_FLAG_POWER_ON;
		if (IS_ENABLED(CONFIG_BOARD_RESET_AFTER_POWER_ON)) {
			/*
			 * Power-on restart, so set a flag and save it for the
			 * next imminent reset. Later code will check for this
			 * flag and wait for the second reset.
			 */
			system_flags |= EC_RESET_FLAG_INITIAL_PWR;
			chip_flags |= EC_RESET_FLAG_INITIAL_PWR;
		}
		break;

	case VCC1_RST_PIN:
		/*
		 * If configured, check the saved flags to see whether the
		 * previous restart was a power-on, in which case treat this
		 * restart as a power-on as well. This is to workaround the fact
		 * that the H1 will reset the EC at power up.
		 */
		if (IS_ENABLED(CONFIG_BOARD_RESET_AFTER_POWER_ON)) {
			if (system_flags & EC_RESET_FLAG_INITIAL_PWR) {
				/*
				 * The previous restart was a power-on so treat
				 * this restart as that, and clear the flag so
				 * later code will not wait for the second
				 * reset.
				 */
				system_flags = (system_flags &
						~EC_RESET_FLAG_INITIAL_PWR) |
					       EC_RESET_FLAG_POWER_ON;
			} else {
				/*
				 * No previous reset flag, so this is a
				 * subsequent restart i.e any restarts after the
				 * second restart caused by the H1.
				 */
				system_flags |= EC_RESET_FLAG_RESET_PIN;
			}
		} else {
			system_flags |= EC_RESET_FLAG_RESET_PIN;
		}
		break;

	case DEBUG_RST:
		system_flags |= EC_RESET_FLAG_SOFT;
		break;

	case WATCHDOG_RST:
		/*
		 * Don't set EC_RESET_FLAG_WATCHDOG flag if watchdog is issued
		 * by system_reset in order to distinguish reset cause is panic
		 * reason or not.
		 */
		if (!(system_flags & (EC_RESET_FLAG_SOFT | EC_RESET_FLAG_HARD)))
			system_flags |= EC_RESET_FLAG_WATCHDOG;
		break;
	}

	/* Clear & set the BBRAM for the following reset. */
	chip_save_reset_flags(chip_flags);

	/* Set the system reset flags. */
	system_set_reset_flags(system_flags);

	return 0;
}

void system_reset(int flags)
{
	int err;
	uint32_t save_flags;

	if (!sys_dev)
		LOG_ERR("sys_dev get binding failed");

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable_all();

	/*  Get flags to be saved in BBRAM */
	system_encode_save_flags(flags, &save_flags);

	/* Store flags to battery backed RAM. */
	chip_save_reset_flags(save_flags);

	/* If WAIT_EXT is set, then allow 10 seconds for external reset */
	if (flags & SYSTEM_RESET_WAIT_EXT) {
		int i;

		/* Wait 10 seconds for external reset */
		for (i = 0; i < 1000; i++) {
			watchdog_reload();
			udelay(10000);
		}
	}

	err = cros_system_soc_reset(sys_dev);

	if (err < 0)
		LOG_ERR("soc reset failed");

	/* should never return */
	while (1)
		;
}

void chip_bbram_status_check(void)
{
	if (!bbram_dev)
		LOG_ERR("bbram_dev doesn't binding");

	if (cros_bbram_get_ibbr(bbram_dev)) {
		LOG_ERR("VBAT power drop!");
		cros_bbram_reset_ibbr(bbram_dev);
	}
	if (cros_bbram_get_vsby(bbram_dev)) {
		LOG_ERR("VSBY power drop!");
		cros_bbram_reset_vsby(bbram_dev);
	}
	if (cros_bbram_get_vcc1(bbram_dev)) {
		LOG_ERR("VCC1 power drop!");
		cros_bbram_reset_vcc1(bbram_dev);
	}
}

static int chip_system_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	bbram_dev = device_get_binding(DT_LABEL(DT_NODELABEL(bbram)));
	if (bbram_dev == NULL) {
		LOG_ERR("bbram_dev gets binding failed");
		return -1;
	}

	sys_dev = device_get_binding("CROS_SYSTEM");
	if (sys_dev == NULL) {
		LOG_ERR("sys_dev gets binding failed");
		return -1;
	}

	/* check the BBRAM status */
	chip_bbram_status_check();

	/* check the reset cause */
	if (check_reset_cause() != 0)
		return -1;

	/*
	 * For some boards on power-on, the EC is reset by the H1 after
	 * power-on, so the EC sees 2 resets. This config enables the EC to save
	 * a flag on the first power-up restart, and then wait for the second
	 * reset before any other setup is done (such as GPIOs, timers, UART
	 * etc.) On the second reset, the saved flag is used to detect the
	 * previous power-on, and treat the second reset as a power-on instead
	 * of a reset.
	 */
	if (IS_ENABLED(CONFIG_BOARD_RESET_AFTER_POWER_ON) &&
	    system_get_reset_flags() & EC_RESET_FLAG_INITIAL_PWR) {
		/* TODO: change to delay 2S. */
		while (1)
			;
		/* Shouldn't get here, but proceeding anyway... */
	}

	return 0;
}

/*
 * The priority should set with the following:
 * 1. For the CONFIG_BOARD_RESET_AFTER_POWER_ON feature, the priority should
 * higher than init_gpios().
 * 2. The priority should lower than cros_system_init & cros_bbram_init.
 */
SYS_INIT(chip_system_init, PRE_KERNEL_1, 22);
