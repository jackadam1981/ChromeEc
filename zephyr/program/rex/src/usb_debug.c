
#include "hooks.h"

extern int command_i2ctrace_enable(int port, int addr_lo, int addr_hi);

static void trace_usb_c0(void)
{
	command_i2ctrace_enable(1, 0x70, 0x70);
}

DECLARE_HOOK(HOOK_INIT, trace_usb_c0, HOOK_PRIO_POST_I2C);
