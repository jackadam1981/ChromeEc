/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_POWER_POWER_H
#define ZEPHYR_CHROME_POWER_POWER_H

#include <devicetree.h>

#define POWER_SIGNAL_ENUM_PRESENT_ENTRY(cid)                                  \
(                                                                             \
	DT_NODE_HAS_PROP(                                                     \
		cid,                                                          \
		power_signal_enum_name                                        \
	)                                                                     \
) ||

#define POWER_SIGNAL_ENUM_PRESENT_PARENT(pid)                                 \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		POWER_SIGNAL_ENUM_PRESENT_ENTRY                               \
	)

#define SYSTEM_DT_POWER_SIGNAL_CONFIG                                         \
(                                                                             \
	DT_FOREACH_STATUS_OKAY(                                               \
		named_gpios,                                                  \
		POWER_SIGNAL_ENUM_PRESENT_PARENT                              \
	)                                                                     \
	(0)                                                                   \
)


#if (SYSTEM_DT_POWER_SIGNAL_CONFIG)

#define GEN_POWER_SIGNAL_STRUCT_ENTRY(cid)                                    \
{                                                                             \
	DT_STRING_UPPER_TOKEN(                                                \
		cid,                                                          \
		enum_name                                                     \
	),                                                                    \
	(                                                                     \
		DT_GPIO_FLAGS(                                                \
			DT_PROP(cid, gpios),                                  \
			flags                                                 \
		) & GPIO_ACTIVE_LOW                                           \
			? POWER_SIGNAL_ACTIVE_LOW                             \
			: POWER_SIGNAL_ACTIVE_HIGH                            \
	),                                                                    \
	DT_PROP(                                                              \
		cid,                                                          \
		power_signal_enum_name                                        \
	)                                                                     \
},

#define GEN_POWER_SIGNAL_STRUCT(cid)                                          \
	COND_CODE_1(                                                          \
		DT_NODE_HAS_PROP(                                             \
			cid,                                                  \
			power_signal_enum_name                                \
		),                                                            \
		(GEN_POWER_SIGNAL_STRUCT_ENTRY(cid)),                         \
		()                                                            \
	)

#define GEN_POWER_SIGNAL_STRUCT_PARENT(pid)                                   \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		GEN_POWER_SIGNAL_STRUCT                                       \
	)

#define INIT_POWER_SIGNAL_STRUCT                                              \
	DT_FOREACH_STATUS_OKAY(                                               \
		named_gpios,                                                  \
		GEN_POWER_SIGNAL_STRUCT_PARENT                                \
	)


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

#define INIT_POWER_SIGNAL_ENUM                                                \
	DT_FOREACH_STATUS_OKAY(                                               \
		named_gpios,                                                  \
		GEN_POWER_SIGNAL_ENUM_PARENT                                  \
	)

enum power_signal {
	INIT_POWER_SIGNAL_ENUM
	POWER_SIGNAL_COUNT
};

#endif /* SYSTEM_DT_POWER_SIGNAL_CONFIG */
#endif /* ZEPHYR_CHROME_POWER_POWER_H */
