/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include "usbc/bc12_pi3usb9201.h"
#include "usbc/bc12_rt1718s.h"
#include "usbc/bc12_rt1739.h"
#include "usbc/bc12_rt9490.h"
#include "usbc/tcpc_rt1718s.h"
#include "usbc/utils.h"
#include "usb_charge.h"

#if DT_HAS_COMPAT_STATUS_OKAY(RT1718S_BC12_COMPAT) ||    \
	DT_HAS_COMPAT_STATUS_OKAY(RT1739_BC12_COMPAT) || \
	DT_HAS_COMPAT_STATUS_OKAY(RT9490_BC12_COMPAT) || \
	DT_HAS_COMPAT_STATUS_OKAY(PI3USB9201_COMPAT)

/* Check RT1718S dependency. BC12 node must be dependent on TCPC node. */
#if DT_HAS_COMPAT_STATUS_OKAY(RT1718S_BC12_COMPAT)
BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(RT1718S_TCPC_COMPAT));
#endif

#define BC12_CHIP_FIND(id)                                       \
	COND_CODE_1(DT_NODE_HAS_COMPAT(id, RT1718S_BC12_COMPAT), \
		    (BC12_CHIP_RT1718S(id)), ())                 \
	COND_CODE_1(DT_NODE_HAS_COMPAT(id, RT1739_BC12_COMPAT),  \
		    (BC12_CHIP_RT1739(id)), ())                  \
	COND_CODE_1(DT_NODE_HAS_COMPAT(id, RT9490_BC12_COMPAT),  \
		    (BC12_CHIP_RT9490(id)), ())                  \
	COND_CODE_1(DT_NODE_HAS_COMPAT(id, PI3USB9201_COMPAT),   \
		    (BC12_CHIP_PI3USB9201(id)), ())

#define BC12_CHIP_ENTRY(usbc_id, bc12_id) \
	[DT_REG_ADDR(usbc_id)] = BC12_CHIP_FIND(id)

#define BC12_CHIP(usbc_id)                           \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, bc12), \
		    (BC12_CHIP_ENTRY(usbc_id, DT_PHANDLE(usbc_id, bc12))), ())

/* Power Path Controller */
struct bc12_config bc12_ports[CHARGE_PORT_COUNT] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, BC12_CHIP) };

#endif /* #if DT_HAS_COMPAT_STATUS_OKAY */
