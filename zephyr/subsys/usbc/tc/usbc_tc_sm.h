/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "usbc_tc_dt.h"

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
struct usbc_tc_port_t {
	/* state machine context */
	struct sm_ctx ctx;
	/* USB-C State Machine Port thread ID */
	k_tid_t tid;

	/*********************************************************************
	 * Port information gathered from DeviceTree
	 */
	/* USB-Cx port number */
	int port;
	/* Indicates if DebugAcc is supported on this SNK port */
	bool supports_debug_accessory;
	/* Attached TCPC port struct */
	const struct device *tcpc_dev;

	/*********************************************************************
	 * Port State information
	 */
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


/*****************************************************************************
 * TypeC API
 */
typedef enum usb_tc_state (*tc_sm_initial_state_t)(const struct device *dev);

__subsystem struct tc_api_t {
};

struct tc_data_t {
	/* StateMachine Initial State callback function */
	tc_sm_initial_state_t sm_initial_state;
};

/**
 * @brief Set the TypeC StateMachine Initial State callback function
 *
 * @param dev  Runtime device structure
 */
static inline int tc_set_sm_initial_state(
	struct device *dev,
	tc_sm_initial_state_t func)
{
	struct tc_data_t *data = (struct tc_data_t *)dev->data;

	data->sm_initial_state = func;
}

#endif /* _USB_TC_SM_H_ */
