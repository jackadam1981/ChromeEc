/*
 * Copyright 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AP_PWRSEQ_H_
#define _AP_PWRSEQ_H_
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AP_POWER_STATE_ACTION_ENTRY	BIT(0)
#define AP_POWER_STATE_ACTION_RUN	BIT(1)
#define AP_POWER_STATE_ACTION_EXIT	BIT(2)

#define AP_POWER_SUB_STATE_ENUM_DEF_WITH_COMMA(node_id)                           \
	DT_STRING_TOKEN(node_id, state),

#define AP_POWER_SUB_STATE_ENUM_DEF(node_id)                                      \
	DT_FOREACH_CHILD(node_id, AP_POWER_SUB_STATE_ENUM_DEF_WITH_COMMA)

enum ap_pwrseq_state {
	AP_POWER_STATE_G3,
	AP_POWER_STATE_S5,
	AP_POWER_STATE_S4,
	AP_POWER_STATE_S3,
	AP_POWER_STATE_S2,
	AP_POWER_STATE_S1,
	AP_POWER_STATE_S0,
	DT_FOREACH_STATUS_OKAY(ap_pwrseq_sub_state, AP_POWER_SUB_STATE_ENUM_DEF)
	AP_POWER_STATE_COUNT,
	AP_POWER_STATE_UNDEF = 0xFFFE,
	AP_POWER_STATE_ERROR = 0xFFFF,
};

enum ap_pwrseq_event {
	AP_PWRSEQ_EVENT_POWER_BUTTON,
	AP_PWRSEQ_EVENT_POWER_STARTUP,
	AP_PWRSEQ_EVENT_POWER_SIGNAL,
	AP_PWRSEQ_EVENT_POWER_TIMEOUT,
	AP_PWRSEQ_EVENT_HOST,
};

typedef void (*ap_pwrseq_callback)(const struct device *dev,
					 enum ap_pwrseq_state state,
					 uint8_t action);

struct ap_pwrseq_state_callback {
	sys_snode_t node;
	ap_pwrseq_callback cb;
	enum ap_pwrseq_state state;
	uint8_t action;
};

void ap_pwrseq_start(const struct device *dev);

void ap_pwrseq_post_event(const struct device *dev,
			 enum ap_pwrseq_event event);

int ap_pwrseq_get_current_state(const struct device *dev,
				enum ap_pwrseq_state *state);

int ap_pwrseq_register_state_callback(const struct device *dev,
				      struct ap_pwrseq_state_callback
				      *state_cb);

#ifdef __cplusplus
}
#endif
#endif //_AP_PWRSEQ_H_
