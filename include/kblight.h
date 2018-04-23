/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Keyboard Backlight API for Chrome EC */

#ifndef __CROS_EC_KBLIGHT_H
#define __CROS_EC_KBLIGHT_H

#include "common.h"
#include "gpio.h"

struct kblight_drv {
	void (*init)(void);
	void (*preserve_state)(void);
	void (*set)(int percent);
	int (*get)(void);
	void (*enable)(int);
	int (*state)(void);
};

int kblight_driver_register(struct kblight_drv *);
int kblight_get(void);
void kblight_set(int);
int kblight_state(void);

#endif  /* __CROS_EC_KBLIGHT_H */
