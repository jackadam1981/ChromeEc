/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* noinit_end_of_ram section should be linked at the very end of ram.
 * CONFIG_NOINIT_END_OF_RAM_SECTION_OFFSET can be used to offset the section
 * if it overlaps with RO ram.
 */

#ifdef CONFIG_NOINIT_END_OF_RAM_SECTION

ASSERT(!SECTION_IS_RO, "noinit_end_of_ram may not be used in RO")

.noinit_end_of_ram(NOLOAD) : {
	. = ALIGN(4);
	__noinit_end_of_ram_start = .;
	. += CONFIG_NOINIT_END_OF_RAM_SECTION_OFFSET;
	*(.noinit_end_of_ram.*)
	__noinit_end_of_ram_end = .;
	. = ALIGN(4);
} > IRAM

#endif /* CONFIG_NOINIT_END_OF_RAM_SECTION */
