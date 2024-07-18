

#include "common.h"
#include "hooks.h"
#include "chip_chipregs.h"
#include "hooks.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <stdlib.h>
void board_hook(void)
{
	const struct device *const it8801_mfd = DEVICE_DT_GET(DT_NODELABEL(it8801_mfd));
	printk(" it8801_mfd=%p\n",it8801_mfd);

	const struct device *const gpiocr2 = DEVICE_DT_GET(DT_NODELABEL(gpiocr2));
	printk(" gpiocr2=%p\n",gpiocr2);


	/* PWM test */
	const struct device *const pwm_dev = DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_pwm));

	printk(" board_hook=%p\n",pwm_dev);
	pwm_set_cycles(pwm_dev, 7, 100, 50, BIT(8));
}
DECLARE_HOOK(HOOK_INIT, board_hook, HOOK_PRIO_LAST);
