/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_COMMON_PWRSEQ_H__
#define __X86_COMMON_PWRSEQ_H__

#include <drivers/espi.h>
#include <drivers/gpio.h>
#include <logging/log.h>

/**
 * @brief System power states for Non Deep Sleep Well
 * EC is an always on device in a Non Deep Sx system except when EC
 * is hibernated or all the VRs are turned off.
 */
enum power_states_ndsx {
	/*
	 * Actual power states
	 */
	/* AP is off & EC is on */
	SYS_POWER_STATE_G3,
	/* AP is in soft off state */
	SYS_POWER_STATE_S5,
	/* AP is suspended to Non-volatile disk */
	SYS_POWER_STATE_S4,
	/* AP is suspended to RAM */
	SYS_POWER_STATE_S3,
	/* AP is in active state */
	SYS_POWER_STATE_S0,

	/*
	 * Intermediate power up states
	 */
	/* Determine if the AP's power rails are turned on */
	SYS_POWER_STATE_G3S5,
	/* Determine if AP is suspended from sleep */
	SYS_POWER_STATE_S5S4,
	/* Determine if Suspend to Disk is de-asserted */
	SYS_POWER_STATE_S4S3,
	/* Determine if Suspend to RAM is de-asserted */
	SYS_POWER_STATE_S3S0,

	/*
	 * Intermediate power down states
	 */
	/* Determine if the AP's power rails are turned off */
	SYS_POWER_STATE_S5G3,
	/* Determine if AP is suspended to sleep */
	SYS_POWER_STATE_S4S5,
	/* Determine if Suspend to Disk is asserted */
	SYS_POWER_STATE_S3S4,
	/* Determine if Suspend to RAM is asserted */
	SYS_POWER_STATE_S0S3,
};

/*
 * AP hard shutdowns are logged on the same path as resets.
 */
enum pwrseq_chipset_shutdown_reason {
	PWRSEQ_CHIPSET_SHUTDOWN_POWERFAIL,
	/* Forcing a shutdown as part of EC initialization */
	PWRSEQ_CHIPSET_SHUTDOWN_INIT,
	/* Forcing shutdown with command */
	PWRSEQ_CHIPSET_SHUTDOWN_CONSOLE_CMD,
	/* Forcing a shutdown to effect entry to G3. */
	PWRSEQ_CHIPSET_SHUTDOWN_G3,
	/* Force a chipset shutdown from the power button through EC */
	PWRSEQ_CHIPSET_SHUTDOWN_BUTTON,

	PWRSEQ_CHIPSET_SHUTDOWN_COUNT,
};

/* This encapsulates the attributes of the state machine */
struct pwrseq_context {
	/* On power-on start boot up sequence */
	enum power_states_ndsx power_state;
};

/* Chipset state mask */
enum chipset_state_mask {
	CHIPSET_STATE_HARD_OFF = 0x01,   /* Hard off (G3) */
	CHIPSET_STATE_SOFT_OFF = 0x02,   /* Soft off (S5) */
	CHIPSET_STATE_SUSPEND  = 0x04,   /* Suspend (S3) */
	CHIPSET_STATE_ON       = 0x08,   /* On (S0) */
	CHIPSET_STATE_STANDBY  = 0x10,   /* Standby (S0ix) */
	/* Common combinations */
	CHIPSET_STATE_ANY_OFF = (CHIPSET_STATE_HARD_OFF |
				 CHIPSET_STATE_SOFT_OFF),  /* Any off state */
	/* This combination covers any kind of suspend i.e. S3 or S0ix. */
	CHIPSET_STATE_ANY_SUSPEND = (CHIPSET_STATE_SUSPEND |
				     CHIPSET_STATE_STANDBY),
};

#endif /* __X86_COMMON_H__ */
