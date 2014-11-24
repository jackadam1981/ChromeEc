/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "flash.h"
#include "registers.h"
#include "system.h"
#include "usb_pd.h"
#include "util.h"
#include "version.h"

#ifndef CONFIG_COMMON_RUNTIME
#include "debug.h"
/* RW firmware reset vector */
uint32_t * const rw_rst =
	(uint32_t *)(CONFIG_FLASH_BASE+CONFIG_FW_RW_OFF+4);
#endif

void pd_jump_to_rw(void)
{
#ifdef CONFIG_COMMON_RUNTIME
	system_run_image_copy(SYSTEM_IMAGE_RW);
#else
	void (*jump_rw_rst)(void) = (void *)*rw_rst;

	debug_printf("Jump to RW\n");
	/* Disable interrupts */
	asm volatile("cpsid i");
	/* Call RW firmware reset vector */
	jump_rw_rst();
#endif
}

int pd_is_ro_mode(void)
{
#ifdef CONFIG_COMMON_RUNTIME
	return system_get_image_copy() == SYSTEM_IMAGE_RO;
#else
	return (uint32_t)&pd_jump_to_rw < (uint32_t)rw_rst;
#endif
}

#ifdef CONFIG_COMMON_RUNTIME
static int flash_erase_rw(void)
{
	return flash_physical_erase(CONFIG_FW_RW_OFF, CONFIG_FW_RW_SIZE);
}

static int flash_write_rw(int offset, int size, const char *data)
{
	return flash_physical_write(offset, size, data);
}
#endif

int pd_custom_flash_vdm(int port, int cnt, uint32_t *payload)
{
	static int flash_offset;
	int rsize = 1; /* default is just VDM header returned */

	switch (PD_VDO_CMD(payload[0])) {
	case VDO_CMD_VERSION:
		memcpy(payload + 1, &version_data.version, 24);
		rsize = 7;
		break;
	case VDO_CMD_REBOOT:
		/* ensure the power supply is in a safe state */
		pd_power_supply_reset(0);
#ifndef CONFIG_COMMON_RUNTIME
		cpu_reset();
#else
		system_reset(0);
#endif
		break;
	case VDO_CMD_READ_INFO:
		/* copy info into response */
		pd_get_info(payload + 1);
		rsize = 7;
		break;
	case VDO_CMD_FLASH_ERASE:
		/* do not kill the code under our feet */
		if (!pd_is_ro_mode())
			break;
		flash_offset = 0;
		flash_erase_rw();
		break;
	case VDO_CMD_FLASH_WRITE:
		/* do not kill the code under our feet */
		if (!pd_is_ro_mode())
			break;
		flash_write_rw(flash_offset, 4*(cnt - 1),
			       (const char *)(payload+1));
		flash_offset += 4*(cnt - 1);
		break;
	case VDO_CMD_ERASE_SIG:
		/* this is not touching the code area */
		{
			uint32_t zero = 0;
			int offset;
			/* zeroes the area containing the RSA signature */
			for (offset = CONFIG_FW_RW_SIZE - 256;
			     offset < CONFIG_FW_RW_SIZE; offset += 4)
				flash_write_rw(offset, 4, (const char *)&zero);
		}
		break;
	default:
		/* Unknown : do not answer */
		return 0;
	}
	return rsize;
}
