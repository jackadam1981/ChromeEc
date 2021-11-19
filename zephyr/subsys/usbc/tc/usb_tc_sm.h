/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * USB Type-C State Machine
 */
#ifndef _USB_TC_SM_H_
#define _USB_TC_SM_H_

/*
 * NOTE: This is just a place holder. CONFIG_USB_TYPEC_SM_LOG_LEVEL
 * should be in a Kconfig.defconfig as
 *
 * config CONFIG_USB_TYPEC_SM_LOG_LEVEL
 *	default LOG_LEVEL_INF
 *
 * I just don't know where in the zephyr project at this time.
 */
#ifndef CONFIG_USB_TYPEC_SM_LOG_LEVEL
#define CONFIG_USB_TYPEC_SM_LOG_LEVEL		LOG_LEVEL_INF
#endif

/*
 * optional CONFIG to control a set level for TC debug.  Leaving
 * this as not defined will allow the shell commands to adjust
 * the value and will initially set it to LEVEL_1. Defining this
 * will cause the debug level to be constant and will not be
 * changeable in the shell.
 * 
 *	CONFIG_USB_TYPEC_DEBUG_LEVEL
 */


/*****************************************************************************
 * DeviceTree helper macros
 */
/*
 * USBC_PORT_LIST_NODE
 * Return the parent node of the USBC Port List
 */
#define USBC_PORT_LIST_NODE						\
	DT_COMPAT_GET_ANY_STATUS_OKAY(					\
				usbc-subsystem-statemachine-port-list)

/*
 * SYSTEM_DT_USBC_CONFIG
 * Determine if the USBC State Machine is needed
 */
#define SYSTEM_DT_USBC_CONFIG						\
	DT_HAS_COMPAT_STATUS_OKAY(					\
				usbc-subsystem-statemachine-port-list)

/*
 * INCLUDE_USB_TC_SNK_STATE_MACHINE
 * Determine if the USBC SNK State Machine is needed
 *
 *   if (INCLUDE_USB_TC_SRC_STATE_MACHINE)
 *       zephyr_library_sources(usbc_snk_state_machine.c)
 *   endif ()
 */
#define USBC_SNK_ENUM_PRESENT_ENTRY(cid)				\
(									\
	DT_NODE_HAS_PROP(						\
		cid,							\
		usbc-type						\
	) &&								\
	(								\
		(							\
			DT_NODE_PROP(					\
				cid,					\
				usbc-type) == USBC_SNK			\
		) ||							\
		(							\
			DT_NODE_PROP(					\
				cid,					\
				usbc-type) == USBC_SNK_DGBACC		\
		)							\
	)								\
) ||

#define INCLUDE_USB_TC_SNK_STATE_MACHINE				\
(									\
	DT_FOREACH_CHILD(						\
		USBC_PORT_LIST_NODE,					\
		USBC_SNK_ENUM_PRESENT_ENTRY				\
	)								\
	(0)								\
)


/*
 * INCLUDE_USB_TC_DBGACC_OPTION
 * Determine if the USBC SNK State Machine needs the DebugAccessory
 */
#define USBC_SNK_DBGACC_ENUM_PRESENT_ENTRY(cid)				\
(									\
	DT_NODE_HAS_PROP(						\
		cid,							\
		usbc-type						\
	) &&								\
	(								\
		DT_NODE_PROP(						\
			cid,						\
			usbc-type) == USBC_SNK_DGBACC			\
	)								\
) ||

#define INCLUDE_USB_TC_DBGACC_OPTION					\
(									\
	DT_FOREACH_CHILD(						\
		USBC_PORT_LIST_NODE,					\
		USBC_SNK_DBGACC_ENUM_PRESENT_ENTRY			\
	)								\
	(0)								\
)


/*
 * INCLUDE_USB_TC_SRC_STATE_MACHINE
 * Determine if the USBC SRC State Machine is needed
 */
#define USBC_SRC_ENUM_PRESENT_ENTRY(cid)				\
(									\
	DT_NODE_HAS_PROP(						\
		cid,							\
		usbc-type						\
	) &&								\
	(								\
		DT_NODE_PROP(						\
			cid,						\
			usbc-type) == USBC_SRC				\
	)								\
) ||

#define INCLUDE_USB_TC_SRC_STATE_MACHINE				\
(									\
	DT_FOREACH_CHILD(						\
		USBC_PORT_LIST_NODE,					\
		USBC_SRC_ENUM_PRESENT_ENTRY				\
	)								\
	(0)								\
)


/*****************************************************************************
 * TypeC Halper Macros
 */
/* Determine if the TypeC State Machine is in an ATTACHED_SNK state */
#if (INCLUDE_USB_TC_DBGACC_OPTION)
#define IS_ATTACHED_SNK(port)						\
	((get_state_tc(port) == TC_ATTACHED_SNK) ||			\
	 (get_state_tc(port) == TC_DEBUG_ACCESSORY_SNK))
#else
#define IS_ATTACHED_SNK(port)						\
	(get_state_tc(port) == TC_ATTACHED_SNK)
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */


/*****************************************************************************
 * TypeC per port information
 */
struct usb_tc_port_info {
	/* state machine context */
	struct sm_ctx ctx;
	/* current port data role (UFP or Disconnected) */
	enum pd_data_role data_role;
	/* Port polarity */
	enum tcpc_cc_polarity polarity;
	/* port flags, see TC_FLAGS_* */
	uint32_t flags;
	/* The cc state */
	enum pd_cc_states cc_state;
	/* Tasks to notify after TCPC has been reset */
	int tasks_waiting_on_reset;
	/* Tasks preventing TCPC from entering low power mode */
	int tasks_preventing_lpm;
	/* Voltage on CC pin */
	enum tcpc_cc_voltage_status cc_voltage;
	/* Type-C current */
	typec_current_t typec_curr;
	/* Type-C current change */
	typec_current_t typec_curr_change;

	/* Selected TCPC CC/Rp values */
	enum tcpc_cc_pull select_cc_pull;
	enum tcpc_rp_value select_current_limit_rp;
	enum tcpc_rp_value select_collision_rp;
};


/*****************************************************************************
 * List of all TypeC-level states
 */
enum usb_tc_state {
	/* Super States */
	TC_CC_OPEN,
	TC_CC_RD,
	/* Normal States */
	TC_DISABLED,
	TC_ERROR_RECOVERY,
	TC_UNATTACHED_SNK,
	TC_ATTACH_WAIT_SNK,
	TC_ATTACHED_SNK,
#if (INCLUDE_USB_TC_DBGACC_OPTION)
	TC_DEBUG_ACCESSORY_SNK,
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */

	TC_STATE_COUNT
};

/*****************************************************************************
 * TypeC State Machine Flags
 */
/* Flag manipulation and checking */
#define TC_SET_FLAG(port, flag) atomic_or(&tc[port].flags, (flag))
#define TC_CLR_FLAG(port, flag) atomic_clear_bits(&tc[port].flags, (flag))
#define TC_CHK_FLAG(port, flag) (tc[port].flags & (flag))

/* Flag to note request to power off sink */
#define TC_FLAGS_POWER_OFF_SNK          BIT(0)
/* Flag to note request from pd_set_suspend to enter TC_DISABLED state */
#define TC_FLAGS_REQUEST_SUSPEND        BIT(1)
/* Flag to note we are in TC_DISABLED state */
#define TC_FLAGS_SUSPENDED              BIT(2)
/* Flag for asynchronous call to request Error Recovery */
#define TC_FLAGS_REQUEST_ERROR_RECOVERY	BIT(3)

/* For checking flag_bit_names[] array */
#define TC_FLAGS_COUNT			4

#endif /* _USB_TC_SM_H_ */
