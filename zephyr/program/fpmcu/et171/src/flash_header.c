/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/toolchain.h>
#include <stdint.h>

struct egis_gp_flash_header {
	uint32_t reserved;
	uint32_t tag;
	uint32_t fw_ver;
	uint32_t start_address;
	uint32_t code_size;
	uint32_t cmd_buff_address;
	uint32_t max_cmd_buff_size;
} __packed;

struct egis_gp_flash_header flash_header = {
	.reserved = 0x0240006f, // magic in MS binary
	.tag = 0x53494745,
	.fw_ver = 0x90601724, // magic from MS bin 0xffffffff,
	// TODO rework to binman node prop
	.start_address = 0x80000000, // start of mapped flash
	.code_size = 0x5e000 - 64, // 0x5e000 - 64 (signature)
	.cmd_buff_address = 0x30363039, // magic from MS bin
	.max_cmd_buff_size = 0x5a5,
	// .cmd_buff_address = 0x00010000,
	// .max_cmd_buff_size = 320 * 1024,
};
