/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Force header files to define grantpt(), posix_openpt(), cfmakeraw() */
#define _BSD_SOURCE
#define _XOPEN_SOURCE 600
/* Force header file to declare ptsname_r(), etc. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <ftdi.h>
#pragma GCC diagnostic pop
#include <unistd.h>
#include <string.h>
#include <libusb.h>
#include <sys/stat.h>
#include <termios.h>

void eeprom_enable_cbus(struct ftdi_context *ftdi, int i)
{
	printf("ftdi_eeprom_initdefaults %d\n", ftdi_eeprom_initdefaults(ftdi,
	       NULL, NULL, NULL));
	printf("ftdi_read_eeprom %d\n", ftdi_read_eeprom(ftdi));
	printf("ftdi_eeprom_decode %d\n", ftdi_eeprom_decode(ftdi, 0));

	printf("ret: %d\n", ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_0, i));
	printf("ret: %d\n", ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_1, i));
	printf("ret: %d\n", ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_2, 0));
	printf("ret: %d\n", ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_3, 0));
	printf("ret: %d\n", ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_4, 0));
	printf("ftdi_eeprom_build %d\n", ftdi_eeprom_build(ftdi));
	printf("ftdi_write_eeprom %d\n", ftdi_write_eeprom(ftdi));
	libusb_reset_device(ftdi->usb_dev);
}

void set_cbus(struct ftdi_context *ftdi, int i)
{
	unsigned char magic = i | 0xf0;
	printf("Setting cbus: %02x\n", magic);
	ftdi_set_bitmode(ftdi, magic, 0x20);
}

int usage(char *program)
{
	printf("usage: %s f|w|r\n", program);
	return -1;
}

int main(int argc, char **argv)
{
	int ret;
	struct ftdi_context *ftdi;

	ftdi = ftdi_new();
	if (ftdi == 0) {
		fprintf(stderr, "ftdi_new failed\n");
		return EXIT_FAILURE;
	}

	ret = ftdi_usb_open(ftdi, 0x0403, 0x6001);
	if (ret < 0) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n", ret,
			ftdi_get_error_string(ftdi));
		ftdi_free(ftdi);
		return EXIT_FAILURE;
	}

	if (ftdi->type == TYPE_R) {
		unsigned int chipid;
		printf("ftdi_read_chipid: %d\n",
		       ftdi_read_chipid(ftdi, &chipid));
		printf("FTDI chipid: %X\n", chipid);
	}

	if (argc == 1)
		return usage(argv[0]);

	if (argv[1][0] == 'w') {
		if (argc < 3)
			return usage(argv[0]);
		eeprom_enable_cbus(ftdi, atoi(argv[2]));
	}

	if (argv[1][0] == 'r') {
		printf("ftdi_read_eeprom %d\n", ftdi_read_eeprom(ftdi));
		printf("ftdi_eeprom_decode %d\n", ftdi_eeprom_decode(ftdi, 1));
	}

	if (argv[1][0] == 'c') {
		if (argc < 3)
			return usage(argv[0]);
		set_cbus(ftdi, atoi(argv[2]));
	}

	if (argv[1][0] == 'n')
		ftdi_disable_bitbang(ftdi);

	ret = ftdi_usb_close(ftdi);
	if (ret < 0) {
		fprintf(stderr, "unable to close ftdi device: %d (%s)\n",
			ret, ftdi_get_error_string(ftdi));
		ftdi_free(ftdi);
		return EXIT_FAILURE;
	}

	ftdi_free(ftdi);

	return EXIT_SUCCESS;
}

