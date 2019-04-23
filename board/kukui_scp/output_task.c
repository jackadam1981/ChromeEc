#include "console.h"

void output_task(void *arg)
{
	while (1)
		ccprintf("test\n");
}
