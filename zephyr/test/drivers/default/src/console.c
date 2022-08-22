/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/zephyr.h>
#include <zephyr/ztest.h>
#include <zephyr/shell/shell_dummy.h> /* nocheck */

#include "console.h"
#include "builtin/stdio.h"
#include "test/drivers/test_state.h"

ZTEST_USER(console, printf_overflow)
{
	char buffer[10];

	zassert_equal(-EC_ERROR_OVERFLOW,
		      crec_snprintf(buffer, 4, "1234567890"), NULL);
	zassert_equal(0, strcmp(buffer, "123"), "got '%s'", buffer);
	zassert_equal(-EC_ERROR_OVERFLOW,
		      crec_snprintf(buffer, 4, "%%%%%%%%%%"), NULL);
	zassert_equal(0, strcmp(buffer, "%%%"), "got '%s'", buffer);
}

ZTEST_USER(console, shell_fprintf_full)
{
	const struct shell *shell_zephyr = get_ec_shell();
	const char *buffer =
		"This is a very long string, it will cause a buffer flush at "
		"some point while printing to the shell. Long long text. Blah "
		"blah. Long long text. Blah blah. Long long text. Blah blah.";
	const char *outbuffer;
	size_t buffer_size;

	zassert_true(strlen(buffer) >= shell_zephyr->fprintf_ctx->buffer_size,
		     "buffer is too short, fix test.");

	shell_backend_dummy_clear_output(shell_zephyr); /* nocheck */
	shell_fprintf(shell_zephyr, SHELL_NORMAL, "%s", buffer);

	outbuffer = shell_backend_dummy_get_output(shell_zephyr, /* nocheck */
						   &buffer_size);
	zassert_true(strncmp(outbuffer, buffer, strlen(buffer)) == 0,
		     "Invalid console output %s", outbuffer);
}

ZTEST_SUITE(console, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
