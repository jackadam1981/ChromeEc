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
 * USBC_PORT_LIST_NODE
 * Return the parent node of the USBC Port List
 */
#define CONFIG_USB_TYPEC_SM_LOG_LEVEL		LOG_LEVEL_INF


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

#endif /* _USB_TC_SM_H_ */
