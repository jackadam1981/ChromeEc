#include "ec_commands.h"

uint32_t flash_get_rw_offset(enum ec_image copy)
{
	return 0x10000;
}

int crec_flash_read(int offset, int size, char *data)
{
	data = "zw-test-build";
	return 0;
}