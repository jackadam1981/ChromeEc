# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(include)
zephyr_library_sources(
	ap_pwrseq_chipset_api.c
	power_signals.c
	signal_gpio.c
	signal_vw.c
	)

if(CONFIG_X86_NON_DSX_PWRSEQ)
zephyr_sources_ifdef(CONFIG_PLATFORM_EC
	ec_task.c)
zephyr_sources_ifndef(CONFIG_PLATFORM_EC
	zephyr_task.c)
zephyr_sources(x86_non_dsx_common_pwrseq_sm_handler.c)
endif()
zephyr_sources_ifdef(CONFIG_X86_NON_DSX_PWRSEQ_CONSOLE
	x86_non_dsx_common_pwrseq_console.c)
zephyr_sources_ifdef(CONFIG_X86_NON_DSX_PWRSEQ_ADL   x86_non_dsx_adlp_pwrseq_sm.c)
