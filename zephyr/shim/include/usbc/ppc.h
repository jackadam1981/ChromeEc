/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_USBC_PPC_H
#define ZEPHYR_CHROME_USBC_PPC_H

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include "usbc/ppc_rt1739.h"
#include "usbc/ppc_nx20p348x.h"
#include "usbc/ppc_sn5s330.h"
#include "usbc/ppc_syv682x.h"
#include "usbc/utils.h"
#include "usbc_ppc.h"

extern struct ppc_config_t ppc_chips_alt[];

#define ALT_PPC_CHIP_CHK(usbc_id) || DT_NODE_HAS_PROP(usbc_id, ppc_alt)

#define ALT_PPC_CHIP_CPY(usbc_id)                                            \
	COND_CODE_1(                                                         \
		DT_NODE_HAS_PROP(usbc_id, ppc),                              \
		(COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, ppc_alt),             \
			     (memcpy(&ppc_chips[USBC_PORT_NEW(usbc_id)],     \
				     &ppc_chips_alt[USBC_PORT_NEW(usbc_id)], \
				     sizeof(struct ppc_config_t));),         \
			     ())),                                           \
		())

#define PPC_ENABLE_ALTERNATE                                            \
	do {                                                                \
		BUILD_ASSERT((0 DT_FOREACH_STATUS_OKAY(named_usbc_port,     \
						       ALT_PPC_CHIP_CHK)),  \
			     "No USB node specifies a PPC alternate chip"); \
		DT_FOREACH_STATUS_OKAY(named_usbc_port, ALT_PPC_CHIP_CPY)   \
	} while (0)

#endif /* ZEPHYR_CHROME_USBC_PPC_H */
