#include "util.h"
#include "console.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "link_defs.h"

#define DBG(format, args...) \
do { \
	ccprintf("[DBG] " format " (%s:%d)\n", ##args, __FILE__, __LINE__); \
	cflush(); \
} while (0)

/* VIF FIFO irq is triggered above this level */
#define WOV_TRIGGER_LEVEL 160

static int wov_disable(void)
{
	/* Reset FIFO and stop IRQ */
	SCP_VIF_FIFO_EN &= ~(VIF_FIFO_RSTN | VIF_FIFO_IRQ_EN);

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

	SCP_VIF_FIFO_EN |= VIF_FIFO_RSTN | VIF_FIFO_IRQ_EN;
	SCP_VIF_FIFO_DATA_THRE = WOV_TRIGGER_LEVEL + 1;

	task_enable_irq(SCP_IRQ_MAD_FIFO);

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

void wov_fifo_interrupt_handler(void)
{
	static size_t count = 0;
	uint32_t fifo_status = SCP_VIF_FIFO_STATUS;

	if (++count % 100000 == 0)
		DBG("%s", __func__);

	/* Read to clear */
	SCP_VIF_FIFO_IRQ_STATUS;

	if (fifo_status & VIF_FIFO_VALID)
		SCP_VIF_FIFO_DATA;
	task_clear_pending_irq(SCP_IRQ_MAD_FIFO);
}
DECLARE_IRQ(SCP_IRQ_MAD_FIFO, wov_fifo_interrupt_handler, 2);

void wov_test(void *arg)
{
	wov_enable();

	while (1) {
		DBG("level=%d", wov_fifo_level());
		DBG("data=%02x", SCP_VIF_FIFO_DATA);
		usleep(1000000);
	}
}
