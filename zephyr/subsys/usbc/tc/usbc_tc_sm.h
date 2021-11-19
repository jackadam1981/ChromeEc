/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * USBC Type-C State Machine
 */
#ifndef _USB_TC_SM_H_
#define _USB_TC_SM_H_

/*
 * NOTE: This is just a place holder. CONFIG_USB_TC_SM_LOG_LEVEL
 * should be in a Kconfig.defconfig as
 *
 * config CONFIG_USB_TC_SM_LOG_LEVEL
 *	default LOG_LEVEL_INF
 *
 * I just don't know where in the zephyr project at this time.
 */
#ifndef CONFIG_USB_TC_SM_LOG_LEVEL
#define CONFIG_USB_TC_SM_LOG_LEVEL		LOG_LEVEL_INF
#endif

/*
 * optional CONFIG to control a set level for TC debug.  Leaving
 * this as not defined will allow the shell commands to adjust
 * the value and will initially set it to LEVEL_1. Defining this
 * will cause the debug level to be constant and will not be
 * changeable in the shell.
 *
 *	CONFIG_USB_TC_DEBUG_LEVEL
 */


/*****************************************************************************
 * DeviceTree helper macros
 *
 * The following DeviceTree example describes a three USB-C port system.
 *	USB-C0 will be Sink only and uses TCPC as it will be defined in
 *	tcpc_port_0.
 *
 *	USB-C1 will be Sink only with Debug Accessory and uses TCPC as it
 *	will be defined in tcpc_port_1.
 *
 *	USB-C2 will be Sink only and uses TCPC as it will be defined in
 *	tcpc_port_2.
 *
 *	usbc_port_list {
 *		compatible = "usbc-subsystem-statemachine-port-list";
 *		status = “okay”;
 *
 *		usbc_port_0: usbc_port_0 {
 *			usbc-port = 0;
 *			usbc-tcpc = <&tcpc_port_0>;
 *			usbc-type = USBC_SNK;
 *		};
 *		usbc_port_1: usbc_port_1 {
 *			usbc-port = 1;
 *			usbc-tcpc = <&tcpc_port_1>;
 *			usbc-type = USBC_SNK_DBGACC;
 *		};
 *		usbc_port_2: usbc_port_2 {
 *			usbc-port = 2;
 *			usbc-tcpc = <&tcpc_port_2>;
 *			usbc-type = USBC_SNK;
 *		};
 *	};
 */
/*
 * USBC_PORT_LIST_NODE
 * Return the parent node of the USBC Port List
 */
#define USBC_PORT_LIST_NODE						\
	DT_COMPAT_GET_ANY_STATUS_OKAY(					\
				usbc_subsystem_statemachine_port_list)

/*
 * SYSTEM_DT_USBC_CONFIG
 * Determine if the USBC State Machine is needed
 */
#define SYSTEM_DT_USBC_CONFIG						\
	DT_HAS_COMPAT_STATUS_OKAY(					\
				usbc_subsystem_statemachine_port_list)


/*
 * USBC_SM_IS_TYPE
 * Helper macro to determine if the USBC node is of a given type
 */
#define USBC_STATE_MACHINE_IS_TYPE(node, type)				\
	UTIL_AND(DT_NODE_HAS_PROP(node, usbc_type),			\
		 DT_NODE_PROP(node, usbc_type) == type)


/*
 * USB_TC_INCLUDES_SNK_STATE_MACHINE
 * Determine if the USBC SNK State Machine is needed
 */
#define USBC_SNK_ENUM_PRESENT_ENTRY(cid)				\
(									\
	UTIL_OR(USBC_STATE_MACHINE_IS_TYPE(cid, USBC_SNK),		\
		USBC_STATE_MACHINE_IS_TYPE(cid, USBC_SNK_DGBACC))	\
) ||

#define USB_TC_INCLUDES_SNK_STATE_MACHINE				\
(									\
	DT_FOREACH_CHILD(						\
		USBC_PORT_LIST_NODE,					\
		USBC_SNK_ENUM_PRESENT_ENTRY				\
	)								\
	(0)								\
)


/*
 * USB_TC_INCLUDES_DBGACC_OPTION
 * Determine if the USBC SNK State Machine needs the DebugAccessory
 */
#define USBC_SNK_DBGACC_ENUM_PRESENT_ENTRY(cid)				\
(									\
	USBC_STATE_MACHINE_IS_TYPE(cid, USBC_SNK_DGBACC)		\
) ||

#define USB_TC_INCLUDES_DBGACC_OPTION					\
(									\
	DT_FOREACH_CHILD(						\
		USBC_PORT_LIST_NODE,					\
		USBC_SNK_DBGACC_ENUM_PRESENT_ENTRY			\
	)								\
	(0)								\
)


/*
 * USB_TC_INCLUDES_SRC_STATE_MACHINE
 * Determine if the USBC SRC State Machine is needed
 */
#define USBC_SRC_ENUM_PRESENT_ENTRY(cid)				\
(									\
	USBC_STATE_MACHINE_IS_TYPE(cid, USBC_SRC)			\
) ||

#define USB_TC_INCLUDES_SRC_STATE_MACHINE				\
(									\
	DT_FOREACH_CHILD(						\
		USBC_PORT_LIST_NODE,					\
		USBC_SRC_ENUM_PRESENT_ENTRY				\
	)								\
	(0)								\
)


/*
 * USB_TC_INCLUDES_DRP_STATE_MACHINE
 * Determine if the USBC DRP State Machine is needed
 */
#define USBC_DRP_ENUM_PRESENT_ENTRY(cid)				\
(									\
	USBC_STATE_MACHINE_IS_TYPE(cid, USBC_DRP)			\
) ||

#define USB_TC_INCLUDES_DRP_STATE_MACHINE				\
(									\
	DT_FOREACH_CHILD(						\
		USBC_PORT_LIST_NODE,					\
		USBC_DRP_ENUM_PRESENT_ENTRY				\
	)								\
	(0)								\
)


/*****************************************************************************
 * TypeC Halper Macros
 */
/* Determine if the TypeC State Machine is in an ATTACHED_SNK state */
#if (INCLUDE_USB_TC_DBGACC_OPTION)
#define IS_ATTACHED_SNK(tc_port)					\
	((get_state_tc(tc_port) == TC_ATTACHED_SNK) ||			\
	 (get_state_tc(tc_port) == TC_DEBUG_ACCESSORY_SNK))
#else
#define IS_ATTACHED_SNK(tc_port)					\
	(get_state_tc(tc_port) == TC_ATTACHED_SNK)
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */


/*****************************************************************************
 * TypeC per port information
 */
struct usb_tc_port_info {
	/*********************************************************************
	 * Port State information
	 */
	/* state machine context */
	struct sm_ctx ctx;

	/* port flags, see TC_FLAGS_* */
	uint32_t flags;
	/* Port polarity */
	enum tc_cc_polarity polarity;
	/* The cc state */
	enum tc_cc_states cc_state;
	/* Voltage on CC pin */
	enum tc_cc_voltage_state cc_voltage;
	/* Type-C current */
	typec_current_t typec_curr;

	/* Selected TCPC CC/Rp values */
	enum tc_cc_pull select_cc_pull;
	enum tc_rp_value select_current_limit_rp;

	/*********************************************************************
	 * Port TC protocol Timers
	 */
	struct k_timer timer_cc_debounce;
	struct k_timer timer_pd_debounce;
	struct k_timer timer_timeout;

	/*********************************************************************
	 * Port information gathered from DeviceTree
	 */
	/* USB-Cx port number */
	int port;
	/* USB-C State Machine Port thread ID */
	k_tid_t tid;
	/* Indicates if DebugAcc is supported on this SNK port */
	bool supports_debug_accessory;

	/* Attached TCPC port struct */
	struct device *tcpc;
};


/*****************************************************************************
 * List of all TypeC-level states
 */
enum usb_tc_state {
	/* Super States */
	TC_CC_OPEN,
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
#define TC_SET_FLAG(tc_port, f) atomic_or(&((tc_port)->flags), (f))
#define TC_CLR_FLAG(tc_port, f) atomic_clear_bits(&((tc_port)->flags), (f))
#define TC_CHK_FLAG(tc_port, f) ((tc_port)->flags & (f))

/* Flag to note request from pd_set_suspend to enter TC_DISABLED state */
#define TC_FLAGS_REQUEST_SUSPEND        BIT(0)
/* Flag to note we are in TC_DISABLED state */
#define TC_FLAGS_SUSPENDED              BIT(1)
/* Flag for asynchronous call to request Error Recovery */
#define TC_FLAGS_REQUEST_ERROR_RECOVERY	BIT(2)

/* For checking flag_bit_names[] array */
#define TC_FLAGS_COUNT			3

#endif /* _USB_TC_SM_H_ */


/*****************************************************************************
 * Override Prototypes
 */
__overridable_proto enum usb_tc_state usb_tc_sm_initial_state(int port);
