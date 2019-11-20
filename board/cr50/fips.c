/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "fips.h"
#include "fips_rand.h"

/**
 * Fatal FIPS failure global error. If set, FIPS crypto is
 * disabled.
 * default value is  = FIPS_UNINITIALIZED
 */
uint32_t fips_status;

void _throw_fips_err(enum fips_err err, const char *func, int line)
{
	fips_status |= err;
	if (fips_status & FIPS_ERROR_MASK)
		ccprintf("%s:%d fips err 0x%08x\n", func, line, fips_status);
}

/* should be called on board init */
void fips_init_clear(void)
{
	/* make sure on power-on / resume it's cleared */
	fips_status = FIPS_UNINITIALIZED;
}

/**
 * Initialization
 * Single point of initialization for all FIPS-compliant
 * cryptography. Responsible for KATs, TRNG testing, and signalling a
 * fatal error.
 */
int init_fips(void)
{
	fips_trng_startup();
	/* TODO(sukhomlinov): add KAT tests */
	fips_status |= FIPS_KAT_TEST_PASSED;
	return EC_SUCCESS;
}

static int cmd_fips_status(int argc, char **argv)
{
	if (fips_status == FIPS_UNINITIALIZED)
		ccprintf("FIPS mode not initialized\n");
	else if (fips_status & FIPS_ERROR_MASK)
		ccprintf("FIPS err 0x%08x\n", fips_status);
	else if (fips_status & FIPS_INITIALIZED)
		ccprintf("FIPS mode active 0x%08x\n", fips_status);
	else
		ccprintf("FIPS not active, status 0x%08x\n", fips_status);

	cflush();

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(fips, cmd_fips_status, NULL, NULL);
