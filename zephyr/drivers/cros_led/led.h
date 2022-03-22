/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_LED_H__
#define __CROS_EC_LED_H__

/*
 * Common macros and defines used by both PWM and GPIO handlers.
 */
/*
 * Duration is stored in tenths of a second.
 */
#define D_TICKS(d)	K_MSEC((d) * 100)

/*
 * Generate names or enums used to reference the LED handler structures.
 * These names are used entirely internally to this file
 * to allow the policy tables to reference the LED actions and
 * handler tables. The names are generated from the DTS node name
 * prepended with a string.
 */
#define LED_TYPE_INDEX(id)	DT_CAT(L_H_I_, id)
#define LED_ACTION(id)	DT_CAT(L_A_, id)
#define LED_HAND_TAB(id)	DT_CAT(L_H_T_, id)
#define LED_ACTION_TAB(id)	DT_CAT(L_A_T_, id)

#define GEN_TYPE_INDEX_ENUM(id)	LED_TYPE_INDEX(id),

/*
 * Generate enums for the action indices for
 * each of the LED handlers. These are not typed
 * as they are only used internally as an index.
 */
#define GEN_ACTION_ENUM(id)	LED_ACTION(id),

#define GEN_ACTION_ENUM_LIST(id)				\
enum {							\
	DT_FOREACH_CHILD(id, GEN_ACTION_ENUM)			\
};

#endif /* __CROS_EC_LED_H__ */
