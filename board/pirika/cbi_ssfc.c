/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cbi_ssfc.h"
#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

/* Cache SSFC on init since we don't expect it to change in runtime */
static union pirika_cbi_ssfc cached_ssfc;
BUILD_ASSERT(sizeof(cached_ssfc) == sizeof(uint32_t));

enum ec_ssfc_db_type get_cbi_ssfc_db_type(void)
{
	return (enum ec_ssfc_db_type)cached_ssfc.db_type;
}
