#include "console.h"
#include "hooks.h"
#include "ipi_chip.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "timer.h"

void test_event_task(void *u)
{
       ccprintf("test event\n");
       while (1) {
               task_wait_event(100000);
       }
}
