/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

. = ALIGN(4);
__flash_lplfw_start = .;
/* auto mode load and jump to EC image */
KEEP(*(.code_in_sram2))
. = ALIGN(4);
__flash_lplfw_end = .;
