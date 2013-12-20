/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash module for Chrome EC */

#include "console.h"
#include "flash.h"
#include "registers.h"
#include "timer.h"


/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_write(int offset, int size, const char *data)
{
	return EC_SUCCESS;
}

int flash_physical_erase(int offset, int size)
{
	return EC_SUCCESS;
}

extern void display_7seg(uint8_t val);
int flash_physical_read(int offset, int size)
{
	int i;
	uint8_t tmp;

	// Set HOSTWA
	//IT83XX_SMFI_SMECCS |= 0x20;

	IT83XX_SMFI_HCTRL2R |= 0x20;

#if 1
	IT83XX_SMFI_ECINDAR3 = 0x00;
	IT83XX_SMFI_ECINDAR0 = 0x00;
	IT83XX_SMFI_ECINDAR1 = 0x00;
	IT83XX_SMFI_ECINDAR2 = 0x00;

	for (i=0; i<128; i++) {
		udelay(5000);
		if (i%16 == 0)
			ccprintf("%d %d %d %d\n", IT83XX_SMFI_ECINDAR3, IT83XX_SMFI_ECINDAR2, IT83XX_SMFI_ECINDAR1, IT83XX_SMFI_ECINDAR0);
		tmp = IT83XX_SMFI_ECINDDR;
		IT83XX_SMFI_ECINDAR0 += 8;
		ccprintf("%02x ", tmp);
	}
	ccprintf("\n");
#else
display_7seg(0);
	/* Enable EC-Indirect Follow Mode. */
	IT83XX_SMFI_ECINDAR3 = 0x0f;
	udelay(5000);
display_7seg(1);
	IT83XX_SMFI_ECINDAR0 = 0x00;
	IT83XX_SMFI_ECINDAR1 = 0xfe;
	IT83XX_SMFI_ECINDAR2 = 0xff;
	IT83XX_SMFI_ECINDAR3 = 0x0f;
	udelay(5000);
display_7seg(2);
	IT83XX_SMFI_ECINDDR = 0x00;
display_7seg(3);
	udelay(5000);
	IT83XX_SMFI_ECINDAR1 = 0xfd;
display_7seg(4);
	//IT83XX_SMFI_ECINDDR = 0x0b;
display_7seg(5);
	//IT83XX_SMFI_ECINDDR = 0x00;
display_7seg(6);
	//IT83XX_SMFI_ECINDDR = 0x00;

	//IT83XX_SMFI_ECINDDR = 0x00;
display_7seg(7);
	for (i=0; i<16; i++) {
		tmp = IT83XX_SMFI_ECINDDR;
		ccprintf("%02x ", tmp);
		if (i%16 == 0)
			ccprintf("\n");
	}
display_7seg(8);
	ccprintf("\n");

	/* Disable EC-Indirect Follow Mode. */
	IT83XX_SMFI_ECINDAR3 = 0x00;
#endif

	return EC_SUCCESS;
}

int flash_physical_get_protect(int bank)
{
	return 0;
}

uint32_t flash_physical_get_protect_flags(void)
{
	return EC_SUCCESS;
}

int flash_physical_protect_now(int all)
{
	return EC_SUCCESS;
}

/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
	return EC_SUCCESS;
}

/*****************************************************************************/
/* Console commands */

static int command_read_flash(int argc, char **argv)
{
	flash_physical_read(0, 0);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flashread, command_read_flash,
			"",
			"",
			NULL);
