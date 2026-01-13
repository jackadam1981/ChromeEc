# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

/* .pigweed_lib section, but do not map to any code, bcs we don't know/can't assign pigweed section name in thirdparty/pigweed */
SECTION_PROLOGUE(.pigweed_lib,,)
{
#ifdef CONFIG_SOC_IT8XXX2_LIBRARY_TO_RAM
		*(EXCLUDE_FILE (
#endif
#ifdef CONFIG_SOC_IT8XXX2_PIGWEED_IN_RAM
			*libpw_tokenizer.a:*
			*libpw_varint.a:*
			*libpw_base64.a:*
			*libpw_log_tokenized.a:*
#endif
#ifdef CONFIG_SOC_IT8XXX2_LIBRARY_TO_RAM
		)
			.text
		)
#endif

#ifdef CONFIG_SOC_IT8XXX2_LIBRARY_TO_RAM
		*(EXCLUDE_FILE (
#endif
#ifdef CONFIG_SOC_IT8XXX2_PIGWEED_IN_RAM
			*libpw_tokenizer.a:*
			*libpw_varint.a:*
			*libpw_base64.a:*
			*libpw_log_tokenized.a:*
#endif
#ifdef CONFIG_SOC_IT8XXX2_LIBRARY_TO_RAM
		)
			.text.*
		)
#endif

	. = ALIGN(0x1000);
	KEEP(*(.pigweed_lib*)) /* useless, there is no section name pigweed_lib, no code will be kept */
	__ilm_flash_pigweed_start = .;

#ifdef CONFIG_SOC_IT8XXX2_PIGWEED_IN_RAM
	*libpw_tokenizer.a:*(.text .text.*)
	*libpw_tokenizer.a:*(.rodata .rodata.*)
	*libpw_tokenizer.a:*(.srodata .srodata.*)
	*libpw_varint.a:*(.text .text.*)
	*libpw_varint.a:*(.rodata .rodata.*)
	*libpw_varint.a:*(.srodata .srodata.*)
	*libpw_base64.a:*(.text .text.*)
	*libpw_base64.a:*(.rodata .rodata.*)
	*libpw_base64.a:*(.srodata .srodata.*)
	*libpw_log_tokenized.a:*(.text .text.*)
	*libpw_log_tokenized.a:*(.rodata .rodata.*)
	*libpw_log_tokenized.a:*(.srodata .srodata.*)
#endif
	__ilm_flash_pigweed_end = .;
	. = ALIGN(0x1000);
} GROUP_LINK_IN(ROMABLE_REGION)
