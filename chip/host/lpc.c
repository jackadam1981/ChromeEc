/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LPC module for Chrome EC emulator */

#include "lpc.h"

test_mockable int hostcmdx86_keyboard_has_char(void)
{
	return 0;
}

test_mockable int hostcmdx86_keyboard_input_pending(void)
{
	return 0;
}

test_mockable void hostcmdx86_keyboard_put_char(uint8_t chr, int send_irq)
{
	/* Do nothing */
}

test_mockable void hostcmdx86_keyboard_clear_buffer(void)
{
	/* Do nothing */
}

test_mockable void hostcmdx86_keyboard_resume_irq(void)
{
	/* Do nothing */
}
