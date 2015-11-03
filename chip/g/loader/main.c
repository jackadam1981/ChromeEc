#include "system.h"
#include "printf.h"
#include "common.h"

uint32_t sleep_mask;

timestamp_t get_time(void)
{
	timestamp_t ret;

	ret.val = 0;

	return ret;
}

static int panic_txchar(void *context, int c)
{
	if (c == '\n')
		panic_txchar(context, '\r');
#if 0
	/* Wait for space in transmit FIFO */
	while (!uart_tx_ready())
		;

	/* Write the character directly to the transmit FIFO */
	uart_write_char(c);
#endif
	return 0;
}

void panic_puts(const char *outstr)
{
	/* Put all characters in the output buffer */
	while (*outstr)
		panic_txchar(NULL, *outstr++);
}

void panic_printf(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfnprintf(panic_txchar, NULL, format, args);
	va_end(args);
}

int main(void)
{
	while(1)
		;
}

void panic_reboot(void)
{
        panic_puts("\n\nRebooting...\n");
        system_reset(0);
}

void interrupt_disable(void)
{
	asm("cpsid i");
}
