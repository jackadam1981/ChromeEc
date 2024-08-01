/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"

#include <zephyr/fff.h>

FAKE_VOID_FUNC(chipset_force_shutdown, enum chipset_shutdown_reason);
