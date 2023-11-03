/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DEBUG_H
#define __CROS_EC_DEBUG_H

#include "common.h"
#include "stdbool.h"

/*
 * Indicates if a debugger is actively connected.
 */
__override_proto bool debugger_is_connected(void);

/*
 * This function looks for signs that a debugger was attached. If we
 * see that a debugger was attached, we know that the chip's security features
 * may function as if the debugger is still attached.
 *
 * This should be true while a debugger is actively connected, too.
 */
__override_proto bool debugger_was_connected(void);

/*
 * Disable the debugger interface, in the context of security lockdown.
 */
__override_proto void debugger_disable(void);

/*
 * Enable the debugger interface.
 *
 * This is a separate function from debugger_disable to ensure that the
 * enable code path can be completely removed during the build process, if
 * unused, to improve security posture.
 */
__override_proto void debugger_enable(void);

/*
 * Optionally disable the debugger on early boot.
 *
 * This function should be implemented as board specific behavior.
 *
 * The logic in this function must not be dependent on any other sub-system
 * being initialized during boot, since this will be called at the earlier
 * point in boot. For example, this function cannot rely on the common
 * system_is_locked, since it relies on gpio init and high level flash state.
 */
__override_proto void debugger_disable_on_boot(void);

#endif /* __CROS_EC_DEBUG_H */
