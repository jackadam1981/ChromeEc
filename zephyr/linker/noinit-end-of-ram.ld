/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Only allowed in RW */
#if defined(CONFIG_CROS_EC_RW)

/*
 * .noinit_end_of_ram is a section of uninitialized ram placed
 * at the very end of used ram. This section of is least likely
 * to overlap with ram used by RO.
 */

. = ALIGN(4);
__noinit_end_of_ram_start = .;
*(.noinit_end_of_ram)
*(".noinit_end_of_ram.*")
__noinit_end_of_ram_end = .;
. = ALIGN(4);

#endif /* defined(CROS_EC_RW) */