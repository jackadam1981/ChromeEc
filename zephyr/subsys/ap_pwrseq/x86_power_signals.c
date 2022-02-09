/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Compose power signals list from device tree */

#include <x86_power_signals.h>

#define GEN_ENABLE_ON_BOOT_DATA(id)                            \
	COND_CODE_1(DT_PROP(id, enable_int_on_boot), (1), (0))

#define GEN_SIGNAL_SOURCE(id)                                  \
	COND_CODE_1(DT_NODE_HAS_PROP(id, pwrseq_int_gpios),    \
	(SOURCE_GPIO),                                         \
	(COND_CODE_1(DT_NODE_HAS_PROP(id, pwrseq_vw_enum),     \
	(SOURCE_VW), (SOURCE_OTHER))))

#define GEN_GPIO_CONFIG(id)                                    \
{                                                              \
	.spec = GPIO_DT_SPEC_GET(id, pwrseq_int_gpios),        \
	.intr_flags = DT_PROP(id, pwrseq_int_flags),           \
	.enable_on_boot = GEN_ENABLE_ON_BOOT_DATA(id),         \
}

#define GEN_COMMON_ENTRY(id)                                   \
	.power_sig = GEN_POWER_SIGNAL_ENUM(id),                \
	.source = GEN_SIGNAL_SOURCE(id),                       \
	.flags = DT_PROP(id, flags),                           \
	.name = DT_PROP(id, dbg_label),

#define GEN_OTHER_POWER_SIGNAL_ENTRY_COMMA(id)                 \
[GEN_POWER_SIGNAL_ENUM(id)] =                                  \
{                                                              \
	GEN_COMMON_ENTRY(id)                                   \
},

#define GEN_VW_POWER_SIGNAL_ENUM(id)                           \
	DT_STRING_UPPER_TOKEN(id, pwrseq_vw_enum)

#define GEN_VW_POWER_SIGNAL_ENTRY_COMMA(id)                    \
[GEN_POWER_SIGNAL_ENUM(id)] =                                  \
{                                                              \
	.vw_signal = GEN_VW_POWER_SIGNAL_ENUM(id),             \
	GEN_COMMON_ENTRY(id)                                   \
},

#define GEN_VW_POWER_SIGNAL_ENTRY(id)                          \
	COND_CODE_1(DT_NODE_HAS_PROP(id, pwrseq_vw_enum),      \
		(GEN_VW_POWER_SIGNAL_ENTRY_COMMA(id)),         \
		(GEN_OTHER_POWER_SIGNAL_ENTRY_COMMA(id)))

#define GEN_GPIO_POWER_SIGNAL_ENTRY_COMMA(id)                  \
[GEN_POWER_SIGNAL_ENUM(id)] = \
{                                                              \
	GEN_COMMON_ENTRY(id)                                   \
	.gpio_config = GEN_GPIO_CONFIG(id),                    \
},

#define GEN_POWER_SIGNAL_ENTRY(id)                             \
	COND_CODE_1(DT_NODE_HAS_PROP(id, pwrseq_int_gpios),    \
		(GEN_GPIO_POWER_SIGNAL_ENTRY_COMMA(id)),       \
		(GEN_VW_POWER_SIGNAL_ENTRY(id)))

const struct power_signal_info power_signal_list[] = {
#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_signal_list)
	DT_FOREACH_CHILD(
		POWER_SIGNALS_LIST_NODE,
		GEN_POWER_SIGNAL_ENTRY)
#endif
};
