/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT st_stm32_rcc

#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "drivers/cros_system.h"
#include "system.h"
#include "util.h"

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

static const struct device *const watchdog =
	DEVICE_DT_GET(DT_CHOSEN(cros_ec_watchdog));

static const char *cros_system_stm32_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "st";
}

static const char *cros_system_stm32_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	return CONFIG_SOC;
}

static const char *
cros_system_stm32_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "";
}

static int cros_system_stm32_get_reset_cause(const struct device *dev)
{
	ARG_UNUSED(dev);

        uint32_t rcc_csr = RCC->CSR;
#if 0
#ifdef STM32_PWR_RESET_CAUSE
        uint32_t pwr_status = STM32_PWR_RESET_CAUSE;
#endif
#endif

        /* Clear the hardware reset cause by setting the RMVF bit */
	SET_BIT(RCC->CSR, RCC_CSR_RMVF);
	//LL_RCC_ClearResetFlags()

#if 0
#ifdef STM32_PWR_RESET_CAUSE
        /* Clear SBF in PWR_CSR */
        STM32_PWR_RESET_CAUSE_CLR |= RESET_CAUSE_SBF_CLR;

        if (pwr_status & RESET_CAUSE_SBF)
                /* Hibernated and subsequently awakened */
                flags |= EC_RESET_FLAG_HIBERNATE;
#endif
#endif

        if (rcc_csr & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF))
		return WATCHDOG_RST; 

        if (rcc_csr & RCC_CSR_SFTRSTF)
                return DEBUG_RST;

        if (rcc_csr & RCC_CSR_PORRSTF)
                return POWERUP;

        if (rcc_csr & RCC_CSR_PINRSTF)
                return VCC1_RST_PIN;

/* We probably need to move that workaround to shim/src/system.c */
#if 0
        /*
         * WORKAROUND: as we cannot de-activate the watchdog during
         * long hibernation, we are woken-up once by the watchdog and
         * go back to hibernate if we detect that condition, without
         * watchdog initialized this time.
         * The RTC deadline (if any) is already set.
         */
        if ((flags & EC_RESET_FLAG_HIBERNATE) &&
            (flags & EC_RESET_FLAG_WATCHDOG)) {
                __enter_hibernate(0, 0);
        }
#endif
	return UNKNOWN_RST;
}

static int cros_system_stm32_soc_reset(const struct device *dev)
{
	ARG_UNUSED(dev);

	uint32_t chip_reset_flags = chip_read_reset_flags();

	/*
	 * We are going to reboot MCU here, so we need to disable caches here.
	 * SCB_DisableDCache also flushes data cache lines.
	 */
#ifdef CONFIG_DCACHE
		SCB_DisableDCache();
#endif

#ifdef CONFIG_ICACHE
		SCB_DisableICache();
#endif


	if (chip_reset_flags & EC_RESET_FLAG_HARD) {
		/*
		 * Set minimal watchdog timeout - 1 millisecond.
		 * STM32 IWDG can be set for lower value, but we are limited by
		 * Zephyr API.
		 */
		struct wdt_timeout_cfg minimal_timeout = {
			.window.max = 1
		};

		/* Setup watchdog */
		wdt_install_timeout(watchdog, &minimal_timeout);

		/* Spin and wait for reboot */
		while (1)
			;
	} else {
		/* Reset implementation for ARM ignores the reset type */
		sys_reboot(0);
	}

	/* Should never return */
	return 0;
}

static uint64_t cros_system_stm32_deep_sleep_ticks(const struct device *dev)
{

	return 0;
}

static int cros_system_stm32_init(const struct device *dev) {
	return 0;
}

static const struct cros_system_driver_api cros_system_driver_stm32_api = {
	.get_reset_cause = cros_system_stm32_get_reset_cause,
	.soc_reset = cros_system_stm32_soc_reset,
	.chip_vendor = cros_system_stm32_get_chip_vendor,
	.chip_name = cros_system_stm32_get_chip_name,
	.chip_revision = cros_system_stm32_get_chip_revision,
	.deep_sleep_ticks = cros_system_stm32_deep_sleep_ticks,
};

#if CONFIG_CROS_SYSTEM_STM32_INIT_PRIORITY >= \
	CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY
#error "CROS_SYSTEM must initialize before the SYSTEM_PRE initialization"
#endif
DEVICE_DEFINE(cros_system_stm32_0, "CROS_SYSTEM", cros_system_stm32_init,
	      NULL, NULL, NULL, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_STM32_INIT_PRIORITY,
	      &cros_system_driver_stm32_api);
