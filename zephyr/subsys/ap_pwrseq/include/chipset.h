/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_CHIPSET_H__
#define __AP_PWRSEQ_CHIPSET_H_

enum chipset_state_mask {
	CHIPSET_STATE_HARD_OFF = 0x01,   /* Hard off (G3) */
	CHIPSET_STATE_SOFT_OFF = 0x02,   /* Soft off (S5, S4) */
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

/**
 * Check if chipset is in a given state.
 *
 * @param state_mask	Combination of one or more CHIPSET_STATE_* flags.
 *
 * @return non-zero if the chipset is in one of the states specified in the
 * mask.
 */
int ap_pwrseq_chipset_in_state(int state_mask);

/**
 * Check if chipset is in a given state or if the chipset task is currently
 * transitioning to that state. For example, G3S5, S5, and S3S5 would all count
 * as the S5 state.
 *
 * @param state_mask	Combination of one or more CHIPSET_STATE_* flags.
 *
 * @return non-zero if the chipset is in one of the states specified in the
 * mask.
 */
int ap_pwrseq_chipset_in_or_transitioning_to_state(int state_mask);

/**
 * Ask the chipset to exit the hard off state.
 *
 * Does nothing if the chipset has already left the state, or was not in the
 * state to begin with.
 */
void ap_pwrseq_chipset_exit_hard_off(void);

#endif /* __AP_PWRSEQ_CHIPSET_H_ */
