/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"
#include "registers.h"

void system_pre_init(void)
{

}

/* TODO(wfrichar): How do we force a reset? */
void system_reset(int flags)
{

}

const char *system_get_chip_vendor(void)
{
	return "g";
}

const char *system_get_chip_name(void)
{
	return "cr50";
}

const char *system_get_chip_revision(void)
{
	return G_REVISION_STR;
}

/* TODO(wfrichar): Where can we store stuff persistently? */

int system_get_vbnvcontext(uint8_t *block)
{
	return 0;
}

int system_set_vbnvcontext(const uint8_t *block)
{
	return 0;
}
