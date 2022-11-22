#include "gpio_signal.h"

void touchpad_interrupt(enum gpio_signal signal)
{
}

void keyboard_clear_buffer(void)
{
	/* release all keys? */
}

void clear_typematic_key(void)
{
}

int lid_is_open(void)
{
	return 1;
}
