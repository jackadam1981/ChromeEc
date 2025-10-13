/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/cros_system.h"
#include "system.h"

#include <zephyr/device.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

// #include <hwinfo_em32.h>
#include "../../../../elan-zephyr/include/zephyr/drivers/hwinfo/hwinfo_em32.h"

#define LOG_LEVEL LOG_LEVEL_INFO
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cros_system);

#define DRV_DATA(dev) ((struct cros_system_elan_data *)(dev)->data)

/* Driver data */
struct cros_system_elan_data {
	int reset; /* reset cause */
};

static const struct device *const watchdog =
	DEVICE_DT_GET(DT_CHOSEN(cros_ec_watchdog));

/* Soc specific system local functions */
static int system_elan_watchdog_stop(void)
{
#ifdef CONFIG_WATCHDOG
	if (!device_is_ready(watchdog)) {
		LOG_ERR("device %s not ready", watchdog->name);
		return -ENODEV;
	}

	wdt_disable(watchdog);
#endif /* CONFIG_WATCHDOG */

	return 0;
}

static uint32_t system_elan_get_chip_id(void)
{
	struct em32_hwinfo dev_hwinfo = { 0 };
	ssize_t ret = 0;
	uint32_t chip_id = 0;

	// Get HW Info.
	ret = hwinfo_get_device_id((uint8_t *)&dev_hwinfo, sizeof(dev_hwinfo));
	if (ret < 0) {
		LOG_ERR("hwinfo_get_device_id fail, err=%d.", ret);
		goto SYSTEM_ELAN_GET_CHIP_ID_EXIT;
	}

	// Get Chip ID
	chip_id = dev_hwinfo.chip_id;
	LOG_INF("chip_id: 0x%x.", chip_id);

SYSTEM_ELAN_GET_CHIP_ID_EXIT:
	return chip_id;
}

static uint8_t system_elan_get_chip_version(void)
{
	struct em32_hwinfo dev_hwinfo = { 0 };
	ssize_t ret = 0;
	uint8_t second_last_byte = 0;
	uint8_t chip_version = 0;

	// Get HW Info.
	ret = hwinfo_get_device_id((uint8_t *)&dev_hwinfo, sizeof(dev_hwinfo));
	if (ret < 0) {
		LOG_ERR("hwinfo_get_device_id fail, err=%d.", ret);
		goto SYSTEM_ELAN_GET_CHIP_VER_EXIT;
	}

	// Get Chip Version (take the two's complement of second_last_byte)
	second_last_byte = (uint8_t)((dev_hwinfo.ic_version & 0x0000ff00) >> 8);
	chip_version = (uint8_t)(~second_last_byte + 1);
	LOG_INF("chip_version: 0x%02x.", chip_version);

SYSTEM_ELAN_GET_CHIP_VER_EXIT:
	return chip_version;
}

static const char *cros_system_elan_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "elan";
}

static const char *cros_system_elan_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[9] = { 'e', 'm', '3', '2', 'f' };
	uint32_t chip_id = system_elan_get_chip_id();

	snprintk(buf + 5, sizeof(buf) - 5, "%03x", chip_id);

	return buf;
}

static const char *cros_system_elan_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[3] = { 0 };
	uint8_t chip_version = system_elan_get_chip_version();

	snprintk(buf, sizeof(buf), "%02x", chip_version);

	return buf;
}

static int cros_system_elan_get_reset_cause(const struct device *dev)
{
	struct cros_system_elan_data *data = DRV_DATA(dev);

	LOG_INF("cros_system_elan_get_reset_cause reset 0x%x", data->reset);
	return data->reset;
}

static int cros_system_elan_soc_reset(const struct device *dev)
{
	ARG_UNUSED(dev);

	/*
	 * Set minimal watchdog timeout - 1 millisecond.
	 * Elan EM32 WDT can be set for lower value, but we are limited by
	 * Zephyr API.
	 */
	struct wdt_timeout_cfg minimal_timeout = { .window.max = 1 };

	LOG_INF("cros_system_elan_soc_reset");

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

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable_all();

	/* Stop watchdog */
	system_elan_watchdog_stop();

	/* Setup watchdog */
	wdt_install_timeout(watchdog, &minimal_timeout);

	/* Apply the changes (the driver will reload watchdog) */
	wdt_setup(watchdog, 0);

	/* Spin and wait for reboot */
	while (1)
		;

	/* Should never return */
	return 0;
}

static int cros_system_elan_init(const struct device *dev)
{
	struct cros_system_elan_data *data = DRV_DATA(dev);
	uint32_t reset_cause;

	data->reset = UNKNOWN_RST;
	hwinfo_get_reset_cause(&reset_cause);

	if (reset_cause & RESET_WATCHDOG) {
		data->reset = WATCHDOG_RST;
	} else if (reset_cause & RESET_SOFTWARE) {
		/* Use DEBUG_RST because it maps to EC_RESET_FLAG_SOFT. */
		data->reset = DEBUG_RST;
	} else if (reset_cause & RESET_BROWNOUT) {
		data->reset = POWERUP;
	} else if (reset_cause & RESET_PIN) {
		data->reset = VCC1_RST_PIN;
	} else if (reset_cause & RESET_LOW_POWER_WAKE) {
		/* TBC how to maps. */
		data->reset = DEBUG_RST;
	}

	return 0;
}

static DEVICE_API(cros_system, cros_system_driver_elan_api) = {
	.get_reset_cause = cros_system_elan_get_reset_cause,
	.soc_reset = cros_system_elan_soc_reset,
	.chip_vendor = cros_system_elan_get_chip_vendor,
	.chip_name = cros_system_elan_get_chip_name,
	.chip_revision = cros_system_elan_get_chip_revision,
};

#if CONFIG_CROS_SYSTEM_ELAN_INIT_PRIORITY >= \
	CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY
#error "CROS_SYSTEM must initialize before the SYSTEM_PRE initialization"
#endif

static struct cros_system_elan_data cros_system_elan_dev_data;

DEVICE_DEFINE(cros_system_elan_0, "CROS_SYSTEM", cros_system_elan_init, NULL,
	      &cros_system_elan_dev_data, NULL, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_ELAN_INIT_PRIORITY,
	      &cros_system_driver_elan_api);
