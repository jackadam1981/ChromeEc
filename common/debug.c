/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "debug.h"
#include "panic.h"
#include "stdbool.h"

/* Debugger internal-only panic routine for debugger_disable. */
void debugger_internal_panic_unimplemented(void);

__overridable bool debugger_is_connected(void)
{
	return false;
}

__overridable bool debugger_was_connected(void)
{
	return false;
}

/*
 * This should never be called on a platform that doesn't implement it,
 * considering this is a security critical function.
 */
__overridable void debugger_disable(void)
{
	panic("Called unimplemented security function debugger_disable");
	__builtin_unreachable();
}

__overridable void debugger_enable(void)
{
}

__overridable void debugger_disable_on_boot(void)
{
	/* By default, we do not disable the debugger, unless overridden. */
}
