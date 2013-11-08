/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __UTIL_ITECOMMON_H
#define __UTIL_ITECOMMON_H

/* common command-line arguments */
#define COMMON_CMD_FLAGS "dv:p:i:s:h?"
#define COMMON_CMD_LONGOPTS      \
	{"debug", 0, 0, 'd'},    \
	{"verbose", 0, 0, 'V'},  \
	{"product", 1, 0, 'p'},  \
	{"vendor", 1, 0, 'v'},   \
	{"interface", 1, 0, 'i'},\
	{"serial", 1, 0, 's'},   \
	{"help", 0, 0, 'h'}
/* Common parameter parsing */
int parse_common_arg(int opt, char *program);
void display_common_usage(void);
/* Tool specific parameter parsing */
int parse_parameters(int argc, char **argv);
void display_usage(char *program);

/* DBGR I2C addresses */
#define I2C_CMD_ADDR   0x5A
#define I2C_DATA_ADDR  0x35
#define I2C_BLOCK_ADDR 0x79

/* I2C communication primitives */
int i2c_add_send_byte(void *ctxt, uint8_t *buf, uint8_t *ptr,
		      uint8_t *tbuf, int tcnt);
int i2c_add_recv_bytes(void *ctxt, uint8_t *buf, uint8_t *ptr,
		       uint8_t *rbuf, int rcnt);
int i2c_byte_transfer(void *ctxt, uint8_t addr, uint8_t *data,
		      int write, int numbytes);
int i2c_write_byte(void *ctxt, uint8_t cmd, uint8_t data);
int i2c_read_byte(void *ctxt, uint8_t cmd, uint8_t *data);

/* read and validate CHIP ID */
int check_chipid(void *ctxt);

/* tool specific implementation */
int tool_main(void *ctxt, int flags);

#endif /* __UTIL_ITECOMMON_H */
