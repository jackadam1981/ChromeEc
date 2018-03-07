/*
 * Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _CMD_H_
#define _CMD_H_

#include "uut_common.h"

/*---------------------------------------------------------------------------
 * Constant definitions
 *---------------------------------------------------------------------------
 */

#define MAX_CMD_BUF_SIZE 10
#define MAX_RESP_BUF_SIZE 512

enum uart_protocol_cmd {
	UFPP_H2D_SYNC_CMD = 0x55,   /* Single-Byte Host to Device	 */
				    /* synchronization command	 */
	UFPP_D2H_SYNC_CMD = 0x5A,   /* Single-Byte Device to Host	 */
				    /* synchronization response	 */
	UFPP_WRITE_CMD = 0x07,      /* Write command and response	 */
	UFPP_READ_CMD = 0x1C,       /* Read command and response	 */
	UFPP_READ_CRC_CMD = 0x89,   /* Read CRC command and response */
	UFPP_FCALL_CMD = 0x70,      /* Call function command	 */
	UFPP_FCALL_RSLT_CMD = 0x73, /* Call function response	 */
	UFPP_SPI_CMD = 0x92,        /* SPI specific command		 */
	UFPP_ERROR_CMD = 0xFF       /* Error response		 */
};

struct CommandNode {
	uint8_t cmd[512];
	uint32_t cmdSize;
	uint32_t respSize;
};

/*---------------------------------------------------------------------------
 * Functions prototypes
 *---------------------------------------------------------------------------
 */

void CMD_CreateSync(uint8_t *cmdInfo, uint32_t *cmdLen);
void CMD_CreateWrite(uint32_t addr, uint32_t size, uint8_t *dataBuf,
					uint8_t *cmdInfo, uint32_t *cmdLen);
void CMD_CreateRead(uint32_t addr, uint8_t size, uint8_t *cmdInfo,
					uint32_t *cmdLen);
void CMD_CreateExec(uint32_t addr, uint8_t *cmdInfo, uint32_t *cmdLen);

void CMD_BuildSync(struct CommandNode *cmdBuf, uint32_t *cmdNum);
void CMD_BuildExecExit(uint32_t addr, struct CommandNode *cmdBuf,
						uint32_t *cmdNum);
void CMD_BuildExecRet(uint32_t addr, struct CommandNode *cmdBuf,
						uint32_t *cmdNum);
void CMD_BuildRomCfg(struct CommandNode *cmdBuf, uint32_t *cmdNum);

BOOLEAN CMD_DispSync(uint8_t *respBuf);
BOOLEAN CMD_DispWrite(uint8_t *respBuf, uint32_t respSize, uint32_t respNum,
		      uint32_t totalSize);
BOOLEAN CMD_DispRead(uint8_t *respBuf, uint32_t respSize, uint32_t respNum,
		     uint32_t totalSize);
void CMD_DispData(uint8_t *respBuf, uint32_t respSize);
void CMD_DispFlashEraseDev(uint8_t *respBuf, uint32_t devNum);
void CMD_DispFlashEraseSect(uint8_t *respBuf, uint32_t devNum);
void CMD_DispFlashId(uint8_t *respBuf, uint32_t devNum);
void CMD_DispFlashSts(uint8_t *respBuf, uint32_t devNum);
void CMD_DispExecExit(uint8_t *respBuf);
void CMD_DispExecRet(uint8_t *respBuf);
void CMD_DispRomCfg(uint8_t *respBuf, uint32_t respType);

#endif /* _CMD_H_ */
