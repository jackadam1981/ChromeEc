#include "console.h"
#include "link_defs.h"
#include "util.h"

__SECTION(l1tcm.bss) static int bss_data;

__SECTION(l1tcm.data) static int data = 0xdeafbeef;

__SECTION(l1tcm.rodata) static const int rodata = 0x5566;

__SECTION(l1tcm.bss) static int counter;

__SECTION(l1tcm.text)
static int command_l1tcm_test(int argc, char **argv)
{
	ccprintf("self %x bss %x data %x const %x counter %x\n",
			(unsigned int)&command_l1tcm_test,
			(unsigned int)&bss_data,
			(unsigned int)&data,
			(unsigned int)&rodata,
			(unsigned int)&counter);

	ccprintf("original:\n");
	ccprintf("  bss_data: %x\n", bss_data);
	ccprintf("  data: %x\n", data);
	ccprintf("  rodata: %x\n", rodata);

	ccprintf("copying data to bss:\n");
	bss_data = data;
	ccprintf("  bss_data: %x\n", bss_data);

	ccprintf("copying rodata to data:\n");
	data = rodata;
	ccprintf("  data: %x\n", data);

	ccprintf("counter: %x\n", ++counter);

	cflush();
	return EC_SUCCESS;
}

DECLARE_SAFE_CONSOLE_COMMAND(l1tcm_test, command_l1tcm_test,
			     NULL, "l1tcm test");
