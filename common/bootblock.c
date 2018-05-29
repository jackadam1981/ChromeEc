/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#define STRINGIFY0(name)  #name
#define STRINGIFY(name)  STRINGIFY0(name)

extern const uint8_t *bootblock_start;
extern const uint8_t *bootblock_end;
asm(
	".section .rodata, \"a\", %progbits\n"
	".global bootblock_start\n"
	".type bootblock_start, %object\n"
	".balign 4\n"
	"bootblock_start:\n"
	".incbin \"" STRINGIFY(FINAL_OUTDIR) "/bootblock.bin\"\n\t"
	".global bootblock_end\n"
	".type bootblock_end, %object\n"
	".balign 1\n"
	"bootblock_end:\n"
	".byte 0\n"
);
