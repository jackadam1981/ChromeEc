# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(include)
zephyr_sources_ifdef(CONFIG_X86_NON_DSX_POWER_SEQ
	x86_non_dsx_common_pwrseq_sm_handler.c)
zephyr_sources_ifdef(CONFIG_X86_NON_DSX_POWER_SEQ_ADL   x86_non_dsx_adlp_pwrseq_sm.c)
zephyr_sources_ifdef(CONFIG_X86_NON_DSX_POWER_SEQ   x86_non_dsx_espi.c)
