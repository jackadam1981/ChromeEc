#include "console.h"
#include "hooks.h"
#include "ipi_chip.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "timer.h"
#include "power.h"
#include "ec_commands.h"

void test_event_task(void *u)
{
	static int change_clk = 0;

	while (1) {
		task_wait_event(100000);
		if (change_clk == 0) {
			power_chipset_handle_host_sleep_event(HOST_SLEEP_EVENT_S3_SUSPEND, NULL);
			change_clk = 1;
		} else {
			power_chipset_handle_host_sleep_event(HOST_SLEEP_EVENT_S3_RESUME, NULL);
			change_clk = 0;
		}
	}
}
