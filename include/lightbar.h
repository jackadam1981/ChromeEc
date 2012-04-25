/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Ask the EC to set the lightbar state to reflect the CPU activity */

#ifndef __CROS_EC_LIGHTBAR_H
#define __CROS_EC_LIGHTBAR_H

/****************************************************************************/
/* Internal stuff */

/* Define the types of sequences */
#define LBMSG(state) LIGHTBAR_##state
#include "lightbar_msg_list.h"
enum lightbar_sequence {
	LIGHTBAR_MSG_LIST
	LIGHTBAR_NUM_SEQUENCES
};
#undef LBMSG

/* Request a preset sequence from the lightbar task. */
void lightbar_sequence(enum lightbar_sequence s);

/****************************************************************************/
/* External stuff */

/* These are the commands available to the EC console or via LPC. */
enum lightbar_command {
	LIGHTBAR_CMD_DUMP,
	LIGHTBAR_CMD_OFF,
	LIGHTBAR_CMD_ON,
	LIGHTBAR_CMD_INIT,
	LIGHTBAR_CMD_BRIGHTNESS,
	LIGHTBAR_CMD_SEQUENCE,
	LIGHTBAR_CMD_REGISTER,
	LIGHTBAR_CMD_RGB,
	LIGHTBAR_NUM_CMDS
};

/* The commands require different numbers of inputs and outputs. How many? */
struct lightbar_command_paramcount_s {
	uint8_t insize;
	uint8_t outsize;
};

extern const struct lightbar_command_paramcount_s
lightbar_command_paramcount[LIGHTBAR_NUM_CMDS];

#endif  /* __CROS_EC_LIGHTBAR_H */
