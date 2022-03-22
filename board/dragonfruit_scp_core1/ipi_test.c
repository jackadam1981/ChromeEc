#include "console.h"
#include "ipi_chip.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "compile_time_macros.h"

struct echo_test {
	uint32_t v;
	uint32_t p;
};

struct echo_test rx;

static void echo_ipi_handler(int id, void *data, unsigned int len)
{
	if (!len) {
		ccprintf("len is zero.");
		return;
	}

	memcpy(&rx, data, MIN(len, sizeof(rx)));

	ccprintf("id %d rx v %u p %x\n", id, rx.v, (uint32_t)rx.p);

	task_wake(TASK_ID_IPI_TEST);
}
DECLARE_IPI(SCP_IPI_DEBUG, echo_ipi_handler, 0);

void ipi_test_task(void *u)
{
	int ret;

	while (1) {
		task_wait_event(-1);
		ret = ipi_send(SCP_IPI_DEBUG, &rx, sizeof(rx), 1);
		ccprintf("task ack ret %d\n", ret);
	}
}
