/*
 * Copyright 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AP_PWRSEQ_SM_DEFS_H_
#define _AP_PWRSEQ_SM_DEFS_H_
#include <smf.h>
#include <ap_power/ap_pwrseq.h>

struct ap_pwrseq_smf {
	struct smf_state actions;
	enum ap_pwrseq_state state;
};

#define AP_POWER_ARCH_STATE_DECL(state)                                       \
	extern struct smf_state arch_##state;

#define AP_POWER_ARCH_STATE_DECL_PRE(state)                                   \
	AP_POWER_ARCH_STATE_DECL(state)

#define AP_PWRSEQ_EACH_ARCH_STATE_NODE_DECL(node_id)                          \
		AP_POWER_ARCH_STATE_DECL_PRE(                                \
			DT_STRING_TOKEN(node_id, state))

#define AP_PWRSEQ_EACH_ARCH_STATE_NODE_CHILD_DECL(node_id)                    \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_ARCH_STATE_NODE_DECL)

DT_FOREACH_STATUS_OKAY(ap_pwrseq_state,
		AP_PWRSEQ_EACH_ARCH_STATE_NODE_CHILD_DECL)

#define AP_POWER_CHIPSET_STATE_DECL(state)                                    \
	extern struct smf_state chipset_##state;

#define AP_POWER_CHIPSET_STATE_DECL_PRE(state)                                \
	AP_POWER_CHIPSET_STATE_DECL(state)

#define AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_DECL(node_id)                       \
		AP_POWER_CHIPSET_STATE_DECL_PRE(                             \
			DT_STRING_TOKEN(node_id, state))

#define AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_CHILD_DECL(node_id)                 \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_DECL)

DT_FOREACH_STATUS_OKAY(ap_pwrseq_state,
			AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_CHILD_DECL)

#define AP_POWER_STATE_DECL(state)                                    \
	extern struct ap_pwrseq_smf state_##state;

#define AP_POWER_STATE_DECL_PRE(state)                                \
	AP_POWER_STATE_DECL(state)

#define AP_PWRSEQ_EACH_STATE_NODE_DECL(node_id)                       \
	COND_CODE_1(DT_PROP(node_id, action_enabled),               \
		(AP_POWER_STATE_DECL_PRE(                             \
			DT_STRING_TOKEN(node_id, state))),())

#define AP_PWRSEQ_EACH_STATE_NODE_CHILD_DECL(node_id)                 \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_STATE_NODE_DECL)

DT_FOREACH_STATUS_OKAY(ap_pwrseq_state,
			AP_PWRSEQ_EACH_STATE_NODE_CHILD_DECL)

#define AP_POWER_SUB_STATE_DECL(state)                                    \
	extern struct ap_pwrseq_smf sub_state_##state;

#define AP_POWER_SUB_STATE_DECL_PRE(state)                                \
	AP_POWER_SUB_STATE_DECL(state)

#define AP_PWRSEQ_EACH_SUB_STATE_NODE_DECL(node_id)                       \
		AP_POWER_SUB_STATE_DECL_PRE(                             \
			DT_STRING_TOKEN(node_id, state))

#define AP_PWRSEQ_EACH_SUB_STATE_NODE_CHILD_DECL(node_id)                 \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_SUB_STATE_NODE_DECL)

DT_FOREACH_STATUS_OKAY(ap_pwrseq_sub_state,
			AP_PWRSEQ_EACH_SUB_STATE_NODE_CHILD_DECL)

#endif //_AP_PWRSEQ_SM_DEFS_H_
