#include "x86_power.h"
#include "lpc.h"
#include "uart.h"
#include "chipset.h"


void x86_power_cpu_overheated(int too_hot)
{
	/* TODO: crosbug.com/p/8242 - real implementation */
	uart_printf("cpu_overheated: %d\n", too_hot);
}


void x86_power_force_shutdown(void)
{
	/* TODO: crosbug.com/p/8242 - real implementation */
	uart_puts("force_shutdown\n");
}


void lpc_set_host_events(uint32_t mask)
{
	uart_printf("lpc_set_host_events: %d\n", mask);
}

void chipset_throttle_cpu(int throttle)
{
	uart_printf("chipset_throttle_cpu: %d\n", throttle);
}
