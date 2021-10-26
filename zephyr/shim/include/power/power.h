/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_POWER_POWER_H
#define ZEPHYR_CHROME_POWER_POWER_H

#include <devicetree.h>

#define SYSTEM_DT_POWER_SIGNAL_CONFIG                                         \
	DT_NODE_EXISTS(DT_PATH(power_signal_list))

#if (SYSTEM_DT_POWER_SIGNAL_CONFIG)

#define GEN_POWER_SIGNAL_STRUCT_ENTRY(cid, gid)                               \
{                                                                             \
	.gpio = DT_STRING_UPPER_TOKEN(                                        \
		gid,                                                          \
		enum_name                                                     \
	),                                                                    \
	.flags = (                                                            \
		DT_GPIO_FLAGS(                                                \
			DT_PROP(gid, gpios),                                  \
			flags                                                 \
		) & GPIO_ACTIVE_LOW                                           \
			? POWER_SIGNAL_ACTIVE_LOW                             \
			: POWER_SIGNAL_ACTIVE_HIGH                            \
	),                                                                    \
	.name = DT_PROP(                                                      \
		cid,                                                          \
		power_enum_name                                               \
	)                                                                     \
},
#define GEN_POWER_SIGNAL_STRUCT(cid)                                          \
	GEN_POWER_SIGNAL_STRUCT_ENTRY(                                        \
		cid,                                                          \
		DT_PROP(cid, gpios)                                           \
	)
#define GEN_POWER_SIGNAL_STRUCT_PARENT(pid)                                   \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		GEN_POWER_SIGNAL_STRUCT                                       \
	)
#define INIT_POWER_SIGNAL_STRUCT                                              \
	GEN_POWER_SIGNAL_STRUCT_PARENT(                                       \
		DT_PATH(power_signal_list)                                    \
	)


#define GEN_POWER_SIGNAL_ENUM_ENTRY(cid)                                      \
	DT_STRING_UPPER_TOKEN(                                                \
		cid,                                                          \
		power_signal_enum_name                                        \
	),
#define GEN_POWER_SIGNAL_ENUM_PARENT(pid)                                     \
	DT_FOREACH_CHILD(                                                     \
		pid,                                                          \
		GEN_POWER_SIGNAL_ENUM_ENTRY                                   \
	)
#define INIT_POWER_SIGNAL_ENUM                                                \
	GEN_POWER_SIGNAL_ENUM_PARENT(                                         \
		DT_PATH(power_signal_list)                                    \
	)

enum power_signal {
	INIT_POWER_SIGNAL_ENUM
	POWER_SIGNAL_COUNT
};

#endif /* SYSTEM_DT_POWER_SIGNAL_CONFIG */
#endif /* ZEPHYR_CHROME_POWER_POWER_H */
