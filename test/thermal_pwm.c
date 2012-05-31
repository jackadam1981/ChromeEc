#include "pwm.h"
#include "uart.h"

int pwm_set_fan_target_rpm(int rpm)
{
	uart_printf("set fan: %d\n", rpm);
	return EC_SUCCESS;
}
