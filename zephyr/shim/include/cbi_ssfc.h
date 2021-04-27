/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SHIM_INCLUDE_CBI_SSFC_H_
#define ZEPHYR_SHIM_INCLUDE_CBI_SSFC_H_

#include <devicetree.h>
#include <device.h>

#define CBI_SSFC_NODE			DT_PATH(cbi_ssfc)

#define CBI_SSFC_UNION_ENTRY_NAME(id)	DT_CAT(cbi_ssfc_, id)
#define CBI_SSFC_UNION_ENTRY(id)               \
	uint32_t CBI_SSFC_UNION_ENTRY_NAME(id) \
		: DT_PROP(id, size);

#define CBI_SSFC_PLUS_FIELD_SIZE(id)	+ DT_PROP(id, size)
#define CBI_SSFC_FIELDS_SIZE                                          \
	(0 DT_FOREACH_CHILD(CBI_SSFC_NODE, CBI_SSFC_PLUS_FIELD_SIZE))

union cbi_ssfc {
	struct {
#if DT_NODE_EXISTS(CBI_SSFC_NODE)
		DT_FOREACH_CHILD(CBI_SSFC_NODE, CBI_SSFC_UNION_ENTRY)
		uint32_t reserved : (32 - CBI_SSFC_FIELDS_SIZE);
#endif
	};
	uint32_t raw_value;
};

#endif /* ZEPHYR_SHIM_INCLUDE_CBI_SSFC_H_ */
