#include "cache.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "link_defs.h"
#include "timer.h"
#include "util.h"

__SECTION(dram.bss) static int8_t bss_array[4];
__SECTION(dram.data) static int8_t data_array[4] = { 0xde, 0xad, 0xbe, 0xef };

__SECTION(dram.rodata) static const int8_t const_data_array[4] = { 5, 5, 6, 6 };

__SECTION(dram.bss) static int counter;

__SECTION(dram.text)
static void print_array(const char *name, int8_t *array, size_t size)
{
	int i;

	ccprintf("%s: ", name);
	for (i = 0; i < size; ++i)
		ccprintf("%02x ", array[i] & 0xff);
	ccprintf("\n");
}

DECLARE_SAFE_CONSOLE_COMMAND(dramtest, command_dram_test,
			     NULL, "DRAM placing test");

#undef DRAM_EMI_MPU_TEST
#ifdef DRAM_EMI_MPU_TEST
__SECTION(dram.data)
static uint32_t dram[] = {
	0x10000000, 0x1140FFFC, 0x11410000, 0x11500000, 0x12000000,
	0x13000000, 0x14000000, 0x15000000, 0x16000000, 0x17000000,
	0x18000000, 0x19000000, 0x1A000000, 0x1b000000, 0x1c000000,
	0x1d000000, 0x1e000000, 0x1f000000, 0x1ff00000, 0x1fff0000,
	0x1ffff000, 0x1fffff00, 0x1ffffff0, 0x1ffffff0, 0x1ffffffc,
};

__SECTION(dram.data)
static uint32_t w_dram[] = {
	0, 1, 1, 1, 1,
	1, 1, 1, 1, 1,
	1, 1, 1, 1, 1,
	1, 1, 1, 1, 1,
	1, 1, 1, 1, 1,
};

static void dram_emi_test(void)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(dram); i++) {
		ccprintf("%x R:%x\n", dram[i], *(uint32_t *)dram[i]);
		if (w_dram[i] != 0) {
			*(uint32_t *)dram[i] += 0x11111111;
			ccprintf("%x WR:%x\n", dram[i], *(uint32_t *)dram[i]);
		}
		ccprintf("---\n");
		cflush();
	}
}
#endif

__SECTION(dram.text) int command_dram_test(int argc, char **argv)
{
	int op = 0;

	if (argc == 2) {
		ccprintf("argc %d argv %c\n", argc, argv[0][0]);
		if (argv[1][0] >= '1' && argv[1][0] <= '3') {
			op = argv[1][0] - '0';
		}
	}

#ifdef DRAM_EMI_MPU_TEST
	dram_emi_test();
#endif

	ccprintf("self %x bss %x data %x const %x counter %x\n",
			(unsigned int)&command_dram_test,
			(unsigned int)bss_array,
			(unsigned int)data_array,
			(unsigned int)const_data_array,
			(unsigned int)&counter);
	cflush();
	msleep(100);

	ccprintf("original:\n");
	print_array("  bss_array", bss_array, ARRAY_SIZE(bss_array));
	print_array("  data_array", data_array, ARRAY_SIZE(data_array));

	ccprintf("copying data_array to bss_array:\n");
	memcpy(bss_array, data_array, sizeof(bss_array));
	print_array("  bss_array", bss_array, ARRAY_SIZE(bss_array));

	ccprintf("copying const_data_array to data_array:\n");
	memcpy(data_array, const_data_array, sizeof(data_array));
	print_array("  data_array", data_array, ARRAY_SIZE(data_array));

	ccprintf("counter: %d\n", counter++);

	cflush();

	switch (op) {
	case 1:
		cache_writeback_dcache();
		cache_invalidate_dcache();
		break;
	case 2:
		cache_flush_dcache();
		break;
	case 3:
		cache_invalidate_dcache();
		break;
	default:
		break;
	}

	return EC_SUCCESS;
}


