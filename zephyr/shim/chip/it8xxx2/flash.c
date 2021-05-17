/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <flash.h>
#include <kernel.h>

int flash_physical_write(int offset, int size, const char *data)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

int flash_physical_erase(int offset, int size)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

int flash_physical_get_protect(int bank)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

uint32_t flash_physical_get_protect_flags(void)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

int flash_physical_protect_at_boot(uint32_t new_flags)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

int flash_physical_protect_now(int all)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

int flash_physical_read(int offset, int size, char *data)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

static int flash_dev_init(const struct device *unused)
{
	/* TODO(b/187192628): implement me... */
	ARG_UNUSED(unused);

	return -ENOSYS;
}

uint32_t flash_physical_get_valid_flags(void)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	/* TODO(b/187192628): implement me... */
	return -ENOSYS;
}

/*
 * The priority flash_dev_init should be lower than GPIO initialization because
 * it calls gpio_get_level function.
 */
#if CONFIG_PLATFORM_EC_FLASH_INIT_PRIORITY <= \
	CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY
#error "Flash must be initialized after GPIOs"
#endif
SYS_INIT(flash_dev_init, POST_KERNEL, CONFIG_PLATFORM_EC_FLASH_INIT_PRIORITY);
