/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_usba_port_enable_pins

#include <devicetree.h>
#include "hooks.h"

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0,
		"No compatible USBA Port Enable instance found");

#define SIGNAL_PHANDLE(id, prop, idx) DT_PHANDLE_BY_IDX(id, prop, idx)

#define SIGNAL_NAME(id, prop, idx) \
	T_STRING_UPPER_TOKEN(SIGNAL_PHANDLE(id, prop, idx), enum_name)
#define SIGNAL_NAME_WITH_COMMA(id, prop, idx) \
	SIGNAL_NAME(id, prop, idx),

const int usb_port_enable[] = {
	DT_FOREACH_PROP_ELEM(DT_DRV_INST(0),
			     enable_pins,
			     SIGNAL_NAME_WITH_COMMA)
};

#undef GPIO_SIGNAL_WITH_COMMA

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
