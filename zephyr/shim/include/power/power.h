/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_POWER_POWER_H
#define ZEPHYR_CHROME_POWER_POWER_H

#include <devicetree.h>

#define POWER_SIGNAL_ENUM_PRESENT_ENTRY(cid)                                  \
	(                                                                     \
		DT_NODE_HAS_PROP(                                             \
			cid,                                                  \
			power_signal_enum_name                                \
		)                                                             \
	) ||

#define POWER_SIGNAL_ENUM_PRESENT_PARENT(pid)                                 \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		POWER_SIGNAL_ENUM_PRESENT_ENTRY                               \
	)

#define SYSTEM_DT_NODE_POWER_SIGNAL_CONFIG                                    \
	(                                                                     \
		DT_FOREACH_STATUS_OKAY(                                       \
			named_gpios,                                          \
			POWER_SIGNAL_ENUM_PRESENT_PARENT                      \
		)                                                             \
		(0)                                                           \
	)


#define GEN_POWER_SIGNAL_STRUCT_ENTRY(cid)                                    \
	{                                                                     \
		DT_STRING_UPPER_TOKEN(                                        \
			cid,                                                  \
			enum_name                                             \
		),                                                            \
		(                                                             \
			(DT_PROP_BY_IDX(cid, gpios, 2) & GPIO_ACTIVE_LOW)     \
				? POWER_SIGNAL_ACTIVE_LOW                     \
				: POWER_SIGNAL_ACTIVE_HIGH                    \
		),                                                            \
		DT_PROP(                                                      \
			cid,                                                  \
			power_signal_enum_name                                \
		)                                                             \
	},

#define GEN_POWER_SIGNAL_STRUCT(cid)                                          \
	COND_CODE_1(                                                          \
		DT_NODE_HAS_PROP(                                             \
			cid,                                                  \
			power_signal_enum_name                                \
		),                                                            \
		(GEN_POWER_SIGNAL_STRUCT_ENTRY(id)),                          \
		()                                                            \
	)

#define GEN_POWER_SIGNAL_STRUCT_PARENT(pid)                                   \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		GEN_POWER_SIGNAL_STRUCT                                       \
	)



#if (SYSTEM_DT_NODE_POWER_SIGNAL_CONFIG)

#define GEN_POWER_SIGNAL_ENUM_ENTRY(cid)                                      \
	DT_STRING_UPPER_TOKEN(                                                \
		cid,                                                          \
		power_signal_enum_name                                        \
	),

#define GEN_POWER_SIGNAL_ENUM(cid)                                            \
	COND_CODE_1(                                                          \
		DT_NODE_HAS_PROP(                                             \
			cid,                                                  \
			power_signal_enum_name                                \
		),                                                            \
		(GEN_POWER_SIGNAL_ENUM_ENTRY(cid)),                           \
		()                                                            \
	)

#define GEN_POWER_SIGNAL_ENUM_PARENT(pid)                                     \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		GEN_POWER_SIGNAL_ENUM                                         \
	)


enum power_signal {
	DT_FOREACH_STATUS_OKAY(
		named_gpios,
		GEN_POWER_SIGNAL_ENUM_PARENT)
	POWER_SIGNAL_COUNT
};

#elif defined(CONFIG_AP_ARM_MTK_MT8192)

#warning "enum power_signal should use DT"
enum power_signal {
	PMIC_PWR_GOOD,
	AP_IN_S3_L,
	AP_WDT_ASSERTED,
	POWER_SIGNAL_COUNT,
};

#endif

#endif /* ZEPHYR_CHROME_POWER_POWER_H */
