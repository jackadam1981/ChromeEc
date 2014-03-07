/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Tiny charger configuration */

#include "common.h"
#include "registers.h"
#include "util.h"

static void hardware_init(void)
{
	/* Pin usage */
	/* User button : PA0 */
	/* Blue LED    : PB6 */
	/* Green LED   : PB7 */
}

int main(void)
{
	hardware_init();
	while (1) {
	}
}
