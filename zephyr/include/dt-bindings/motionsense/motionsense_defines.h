/*
 * Copyright 2021 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef DT_BINDINGS_MOTIONSENSE_DEFINES_H
#define DT_BINDINGS_MOTIONSENSE_DEFINES_H

#include <dt-bindings/utils.h>

/*
 * Chipset state mask
 *
 * Note that this is a non-exhaustive list of states which the main chipset can
 * be in, and is potentially one-to-many for real, underlying chipset states.
 * That's why chipset_in_state() asks "Is the chipset in something
 * approximating this state?" and not "Tell me what state the chipset is in and
 * I'll compare it myself with the state(s) I want."
 * This is from chipset.h
 */
#define CHIPSET_STATE_HARD_OFF		0x01	/* Hard off (G3) */
#define CHIPSET_STATE_SOFT_OFF		0x02	/* Soft off (S5) */
#define CHIPSET_STATE_SUSPEND		0x04	/* Suspend (S3) */
#define CHIPSET_STATE_ON		0x08	/* On (S0) */
#define CHIPSET_STATE_STANDBY		0x10	/* Standby (S0ix) */
/* Common combinations */
#define CHIPSET_STATE_ANY_OFF	(CHIPSET_STATE_HARD_OFF |	\
				 CHIPSET_STATE_SOFT_OFF) /* Any off state */
/* This combination covers any kind of suspend i.e. S3 or S0ix. */
#define CHIPSET_STATE_ANY_SUSPEND	(CHIPSET_STATE_SUSPEND |	\
					 CHIPSET_STATE_STANDBY)

/* This is from motion_sense.h */
#define SENSOR_ACTIVE_S5 (CHIPSET_STATE_SOFT_OFF | CHIPSET_STATE_HARD_OFF)
#define SENSOR_ACTIVE_S3 CHIPSET_STATE_ANY_SUSPEND
#define SENSOR_ACTIVE_S0 CHIPSET_STATE_ON
#define SENSOR_ACTIVE_S0_S3 (SENSOR_ACTIVE_S3 | SENSOR_ACTIVE_S0)
#define SENSOR_ACTIVE_S0_S3_S5 (SENSOR_ACTIVE_S0_S3 | SENSOR_ACTIVE_S5)

#endif /* DT_BINDINGS_MOTIONSENSE_DEFINES_H */
