/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Chipset module for emulator */

#include <stdio.h>
#include "chipset.h"
#include "common.h"

test_mockable void chipset_reset(int cold_reset)
{
	/* Nothing */
	printf("Chipset reset!\n");
}
