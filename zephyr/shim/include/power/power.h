/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_POWER_POWER_H
#define ZEPHYR_CHROME_POWER_POWER_H

#include <devicetree.h>

#define POWER_SIGNAL_LIST_NODE                                                \
	DT_NODELABEL(power_signal_list)

#define SYSTEM_DT_POWER_SIGNAL_CONFIG                                         \
	DT_NODE_EXISTS(POWER_SIGNAL_LIST_NODE)

#if (SYSTEM_DT_POWER_SIGNAL_CONFIG)

/*
 * I want to use the flags to convert to POWER_SIGNAL_ACTIVE_* but it is
 * not working for me... I added a hack to verify I could boot and the
 * fields are the same in the old static method and the from DT method.
 * The ,flags section is the last thing I need to figure out.
 */
#if 0
#define GEN_POWER_SIGNAL_STRUCT_ENTRY(cid)                                    \
{                                                                             \
	.gpio = DT_STRING_UPPER_TOKEN(                                        \
		DT_PROP(                                                      \
			cid,                                                  \
			gpio                                                  \
		),                                                            \
		enum_name                                                     \
	),                                                                    \
	.flags = (                                                            \
		DT_GPIO_FLAGS(                                                \
			DT_PROP(                                              \
				DT_PROP(                                      \
					cid,                                  \
					gpio                                  \
				),                                            \
				gpios                                         \
			),                                                    \
			flags                                                 \
		) & GPIO_ACTIVE_LOW                                           \
			? POWER_SIGNAL_ACTIVE_LOW                             \
			: POWER_SIGNAL_ACTIVE_HIGH                            \
	),                                                                    \
	.name = DT_PROP(                                                      \
		cid,                                                          \
		power_enum_name                                               \
	)                                                                     \
}
#else
#define GEN_POWER_SIGNAL_STRUCT_ENTRY(cid)                                    \
{                                                                             \
	.gpio = DT_STRING_UPPER_TOKEN(                                        \
		DT_PROP(                                                      \
			cid,                                                  \
			gpio                                                  \
		),                                                            \
		enum_name                                                     \
	),                                                                    \
	.flags = (                                                            \
		DT_STRING_UPPER_TOKEN(                                        \
			cid,                                                  \
			power_enum_name                                       \
		) != PMIC_PWR_GOOD                                            \
			? POWER_SIGNAL_ACTIVE_LOW                             \
			: POWER_SIGNAL_ACTIVE_HIGH                            \
	),                                                                    \
	.name = DT_PROP(                                                      \
		cid,                                                          \
		power_enum_name                                               \
	)                                                                     \
}
#endif

#define GEN_POWER_SIGNAL_STRUCT(cid)                                          \
	[GEN_POWER_SIGNAL_ENUM_ENTRY(cid)] =                                  \
		GEN_POWER_SIGNAL_STRUCT_ENTRY(cid),
#define GEN_POWER_SIGNAL_STRUCT_PARENT(pid)                                   \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		GEN_POWER_SIGNAL_STRUCT                                       \
	)
#define INIT_POWER_SIGNAL_STRUCT                                              \
	GEN_POWER_SIGNAL_STRUCT_PARENT(                                       \
		POWER_SIGNAL_LIST_NODE                                        \
	)


#define GEN_POWER_SIGNAL_ENUM_ENTRY(cid)                                      \
	DT_STRING_UPPER_TOKEN(                                                \
		cid,                                                          \
		power_enum_name                                               \
	)
#define GEN_POWER_SIGNAL_ENUM(cid)                                            \
	GEN_POWER_SIGNAL_ENUM_ENTRY(cid),
#define GEN_POWER_SIGNAL_ENUM_PARENT(pid)                                     \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		GEN_POWER_SIGNAL_ENUM                                         \
	)
#define INIT_POWER_SIGNAL_ENUM                                                \
	GEN_POWER_SIGNAL_ENUM_PARENT(                                         \
		POWER_SIGNAL_LIST_NODE                                        \
	)

enum power_signal {
	INIT_POWER_SIGNAL_ENUM
	POWER_SIGNAL_COUNT
};

#endif /* SYSTEM_DT_POWER_SIGNAL_CONFIG */
#endif /* ZEPHYR_CHROME_POWER_POWER_H */
