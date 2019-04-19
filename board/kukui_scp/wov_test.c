#include "util.h"
#include "console.h"
#include "registers.h"
#include "system.h"
#include "task.h"

#define DBG(format, args...) \
do { \
	ccprintf("[DBG] " format " (%s:%d)\n", ##args, __FILE__, __LINE__); \
	cflush(); \
} while (0)

static int wov_disable(void)
{
	SCP_VIF_FIFO_EN = 0;

	return EC_SUCCESS;
}

static int wov_enable(void)
{
	uint32_t serial_if_cfg0 = RXIF_CFG0_RESET_VAL;

	wov_disable();

	/* WOV_MICTYPE_DMIC */
	serial_if_cfg0 |= RXIF_RGDL2_DMIC_16K;

	SCP_RXIF_CFG0 = serial_if_cfg0;
	SCP_RXIF_CFG1 = RXIF_CFG1_RESET_VAL;

	SCP_VIF_FIFO_EN |= VIF_FIFO_RSTN;

	return EC_SUCCESS;
}

static size_t wov_fifo_level(void)
{
	uint32_t fifo_status = SCP_VIF_FIFO_STATUS;

	if (!(fifo_status & VIF_FIFO_VALID))
		return 0;

	if (fifo_status & VIF_FIFO_FULL)
		return VIF_FIFO_MAX;

	return VIF_FIFO_LEVEL(fifo_status);
}

void wov_test(void *arg)
{
	size_t i;

	wov_enable();

	for (i = 0; ; ++i) {
		if (wov_fifo_level()) {
			DBG("i=%d: %04x", i, SCP_VIF_FIFO_DATA & 0xffff);
			while (wov_fifo_level())
				SCP_VIF_FIFO_DATA;
		} else {
			DBG("i=%d: nothing", i);
		}

		/* yield the CPU */
		usleep(1000000);
	}
}

static int enable(int argc, char **argv)
{
	wov_enable();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(a, enable, "None", "Enable WoV");

static int disable(int argc, char **argv)
{
	wov_disable();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(b, disable, "None", "Disable WoV");

static int write_reg(int argc, char **argv)
{
	int addr, value;

	if (argc != 3)
		return EC_ERROR_INVAL;

	addr = strtoi(argv[1], NULL, 16);
	value = strtoi(argv[2], NULL, 16);

	DBG("write 0x%x = 0x%x", addr, value);
	*(int *)addr = value;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(w, write_reg, "[REG] [VALUE]", "Uh");

static int read_reg(int argc, char **argv)
{
	int addr;

	if (argc != 2)
		return EC_ERROR_INVAL;

	addr = strtoi(argv[1], NULL, 16);

	DBG("read 0x%x = 0x%x", addr, *(int *)addr);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(r, read_reg, "[REG]", "Uh");
