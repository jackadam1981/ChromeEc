/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SCP_FD_H
#define __CROS_EC_SCP_FD_H

#include "chip/mt_scp/registers.h"
#include "queue.h"

enum fd_msg_type {
	FD_IPI_MSG,
	FD_MAX,
};

enum fd_cmd_type {
	FD_CMD_INIT,
	FD_CMD_ENQ,
	FD_CMD_EXIT,
};

typedef void (*fd_msg_handler)(void *msg);

struct fd_service {
	enum fd_msg_type type;
	unsigned char msg[64];
};

/* Functions provided by private overlay. */
void fd_ipi_msg_handler(void *data);

#endif /* __CROS_EC_SCP_FD_H */
