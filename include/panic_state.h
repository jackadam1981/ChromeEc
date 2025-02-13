/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PANIC_STATE_H
#define __CROS_EC_PANIC_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

enum panic_state_tag_t {
	PANIC_STATE_TAG_NULL = 0,
	PANIC_STATE_TAG_POWER,
	PANIC_STATE_TAG_LID,
	PANIC_STATE_TAG_PD0,
	PANIC_STATE_TAG_PD1,
	PANIC_STATE_TAG_C0,
	PANIC_STATE_TAG_C1,
	PANIC_STATE_TAG_BATTERY,
	PANIC_STATE_TAG_TICK,
	/* Count must be last tag */
	PANIC_STATE_TAG_COUNT,
};

void panic_state_init(void);

void panic_state_update(enum panic_state_tag_t tag, uint32_t value);

#define PANIC_STATE_UPDATE(tag, value)                          \
	if (IS_ENABLED(CONFIG_PANIC_STATE) &&                   \
	    IS_ENABLED(CONFIG_PANIC_STATE_TAG_##tag##_ENABLED)) \
	panic_state_update(PANIC_STATE_TAG_##tag, value)

#ifndef CONFIG_PANIC_STATE
/* Stub out API if disabled */
inline void panic_state_init(void)
{
}
inline void panic_state_update(enum panic_state_tag_t tag, uint32_t value)
{
}
#endif /* CONFIG_PANIC_STATE */

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_PANIC_STATE_H */