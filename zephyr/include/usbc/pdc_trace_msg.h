/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_PDC_TRACE_MSG_H_
#define ZEPHYR_INCLUDE_DRIVERS_PDC_TRACE_MSG_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset the message FIFO
 */
__test_only void pdc_trace_msg_fifo_reset(void);

/**
 * @brief Control PDC message tracing
 *
 * @param port Type-C port number or
 *             EC_PDC_TRACE_MSG_PORT_NONE to disable or
 *             EC_PDC_TRACE_MSG_PORT_ALL to enable on all ports
 *
 * @retval previous port number
 */
test_export_static int pdc_trace_msg_enable(int port);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_PDC_TRACE_MSG_H_ */
