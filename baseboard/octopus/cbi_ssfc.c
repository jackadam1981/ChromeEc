/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cbi_ssfc.h"
#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"

/****************************************************************************
 * Octopus CBI Second Source Factory Cache
 */

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

/* Cache SSFC on init since we don't expect it to change in runtime */
static uint32_t cached_ssfc;

static void cbi_ssfc_init(void)
{
	if (cbi_get_ssfc(&cached_ssfc) != EC_SUCCESS)
		/* Default to 0 when CBI isn't populated */
		cached_ssfc = 0;

	CPRINTS("CBI SSFC: 0x%04X", cached_ssfc);
}
DECLARE_HOOK(HOOK_INIT, cbi_ssfc_init, HOOK_PRIO_FIRST);

enum ssfc_tcpc_p1 get_cbi_ssfc_tcpc_p1(void)
{
	return ((cached_ssfc & SSFC_TCPC_P1_MASK) >> SSFC_TCPC_P1_OFFSET);
}

#if defined CONFIG_FACTORY_SSFC_PROBE
void set_cbi_ssfc_tcpc_p1_ps8xxx(uint16_t product_id)
{
	int changed = 1;
	int tcpc_p1;
	int res;

	switch (product_id) {
	case PS8755_PRODUCT_ID:
		tcpc_p1 = TCPC_P1_PS8751;
		break;
	case PS8751_PRODUCT_ID:
		tcpc_p1 = TCPC_P1_PS8755;
		break;
	default:
		changed = 0;
		break;
	}

	if (!changed)
		return;

	cached_ssfc &= ~SSFC_TCPC_P1_MASK;
	cached_ssfc |= tcpc_p1 << SSFC_TCPC_P1_OFFSET;

	res = cbi_set_board_info(CBI_TAG_SSFC, (const uint8_t *)&cached_ssfc,
				 sizeof(cached_ssfc));

	if (res != EC_SUCCESS) {
		CPRINTS("CBI SSFC: new value 0x%04X can't be updated to CBI.",
			cached_ssfc);
		return;
	}

	cbi_write();
}
#endif
