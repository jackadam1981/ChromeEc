

#include "common.h"
#include "hooks.h"
#include "chip_chipregs.h"
#include "hooks.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <stdlib.h>

#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>

#if 0
const struct device *const kbd_dev =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_kbd));
static int keyboard_pm_init(void)
{
	/* Initialize as active */
	pm_device_runtime_get(kbd_dev);

	return 0;
}

SYS_INIT(keyboard_pm_init, APPLICATION, 0);
#endif

#if 1
struct drv_data {
	struct gpio_callback gpio_cb;
	struct gpio_callback gpio_cb2;
	struct gpio_callback gpio_cb3;
	struct gpio_callback gpio_cb4;
	gpio_flags_t mode;
	int index;
	int aux;
};
static struct drv_data data;
static int cb_cnt, cb_cnt2, cb_cnt3, cb_cnt4;

static void callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/*= checkpoint: pins should be marked with correct pin number bit =*/
	//zassert_equal(pins, BIT(1),
	//	      "unexpected pins %x", pins);
	//printk("callback: pins=%x\n",pins);
	++cb_cnt;
	printk("callback: pins=%d, cb_cnt=%d\n",pins, cb_cnt);
}

#if 1
static void callback2(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/*= checkpoint: pins should be marked with correct pin number bit =*/
	//zassert_equal(pins, BIT(1),
	//	      "unexpected pins %x", pins);
	printk("callback2: pins=%x\n",pins);
	++cb_cnt2;
	printk("callback2: cb_cnt=%d\n",cb_cnt2);
}

static void callback3(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/*= checkpoint: pins should be marked with correct pin number bit =*/
	//zassert_equal(pins, BIT(1),
	//	      "unexpected pins %x", pins);
	printk("callback3: pins=%x\n",pins);
	++cb_cnt3;
	printk("callback3: cb_cnt=%d\n",cb_cnt3);
}
static void callback4(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/*= checkpoint: pins should be marked with correct pin number bit =*/
	//zassert_equal(pins, BIT(1),
	//	      "unexpected pins %x", pins);
	printk("callback4: pins=%x\n",pins);
	++cb_cnt4;
	printk("callback4: cb_cnt=%d\n",cb_cnt4);
}
#endif
#endif
void board_hook(void)
{
	//const struct device *const it8801_mfd = DEVICE_DT_GET(DT_NODELABEL(it8801_mfd));
	//printk(" it8801_mfd=%p\n",it8801_mfd);

	printk("[uart]ioex_it8801_kbd=%p\n",DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_kbd)));
	printk("[uart]ioex_it8801_port0=%p\n",DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port0)));
	printk("[uart]ioex_it8801_port1=%p\n",DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port1)));
	printk("[uart]ioex_it8801_port2=%p\n",DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port2)));
#if 1
	const struct device *const dev =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port1));
	struct drv_data *drv_data = &data;
	int ret;

	cb_cnt = 0;

	ret = gpio_pin_configure(dev, 0, GPIO_DISCONNECTED);
	ret = gpio_pin_configure(dev, 1, GPIO_DISCONNECTED);

	ret = gpio_pin_configure(dev, 0, GPIO_OUTPUT_LOW);
	//ret = gpio_pin_set(dev, 0, 0);

	/* 1. Configure PIN_IN callback */
	ret = gpio_pin_configure(dev, 1, GPIO_INPUT);
	if(ret) {
		printk("config PIN_IN failed.\n");
	}

	gpio_init_callback(&drv_data->gpio_cb, callback, BIT(1));
	ret = gpio_add_callback(dev, &drv_data->gpio_cb);
	if(ret) {
		printk("add callback failed.\n");
	}

	/* 2. Enable PIN callback as both edges */
	ret = gpio_pin_interrupt_configure(dev, 1, GPIO_INT_EDGE_BOTH);
	if (ret == -ENOTSUP) {
		printk("Both edge GPIO interrupt not supported.\n");
		//gpio_remove_callback(dev, &drv_data->gpio_cb);
	}
	if(ret) {
		printk("enable callback failed\n");
	}

	/* 3. Configure PIN_OUT as open drain, internal pull-up (may trigger
	 * callback)
	 */
	k_sleep(K_MSEC(100));
	//ret = gpio_pin_configure(dev, 0, GPIO_OUTPUT_HIGH);
	gpio_pin_set(dev, 0, 1);
	k_sleep(K_MSEC(100));

	//(void)gpio_pin_interrupt_configure(dev, 1, GPIO_INT_DISABLE);

	if (ret == -ENOTSUP) {
		printk("Open drain not supported.\n");
		//gpio_remove_callback(dev, &drv_data->gpio_cb);

		return;
	}
	if(ret) {
		printk("config PIN_OUT failed\n");
	}

	/* 4. Configure PIN_OUT again (should not trigger callback)  */
	//ret = gpio_pin_configure(dev, 0,
	//			 GPIO_OUTPUT | GPIO_OPEN_DRAIN | GPIO_PULL_UP);
	//gpio_remove_callback(dev, &drv_data->gpio_cb);
#if 1
	/* 4. Wait a bit and ensure that interrupt happened at most once */
	k_sleep(K_MSEC(10));

	const struct device *const dev2 =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port1));
	int ret2 = gpio_pin_interrupt_configure(dev2, 0, GPIO_INT_EDGE_BOTH);
	if (ret2 == -ENOTSUP) {
		printk("aaa.\n");
	}
	gpio_init_callback(&drv_data->gpio_cb2, callback2, BIT(0));
	ret = gpio_add_callback(dev2, &drv_data->gpio_cb2);
	if(ret) {
		printk("add callback2 failed.\n");
	}

	const struct device *const dev3 =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port0));
	int ret3 = gpio_pin_interrupt_configure(dev3, 3, GPIO_INT_EDGE_BOTH);
	if (ret3 == -ENOTSUP) {
		printk("bbb.\n");
	}
	gpio_init_callback(&drv_data->gpio_cb3, callback3, BIT(3));
	ret3 = gpio_add_callback(dev3, &drv_data->gpio_cb3);
	if(ret3) {
		printk("add callback2 failed.\n");
	}

	const struct device *const dev4 =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port2));
	int ret4 = gpio_pin_interrupt_configure(dev4, 0, GPIO_INT_EDGE_BOTH);
	if (ret4 == -ENOTSUP) {
		printk("ccc.\n");
	}
	gpio_init_callback(&drv_data->gpio_cb4, callback4, BIT(0));
	ret4 = gpio_add_callback(dev4, &drv_data->gpio_cb4);
	if(ret4) {
		printk("add callback2 failed.\n");
	}
#endif

#endif
#if 0
	/* GPIO test */
	const struct device *ioex_it8801_port0 =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port0));
	const struct device *ioex_it8801_port1 =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port1));
	const struct device *ioex_it8801_port2 =  DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_port2));
	//printk(" ioex_it8801_port0=%p\n",ioex_it8801_port0);


	gpio_pin_configure(ioex_it8801_port0, 0, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port0, 1, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port0, 3, GPIO_OUTPUT_LOW);

	gpio_pin_configure(ioex_it8801_port0, 4, GPIO_OUTPUT_LOW);

	gpio_pin_configure(ioex_it8801_port0, 6, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port0, 7, GPIO_OUTPUT_LOW);

	//gpio_pin_configure(ioex_it8801_port1, 0, GPIO_OUTPUT_LOW);
	//gpio_pin_configure(ioex_it8801_port1, 1, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port1, 2, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port1, 3, GPIO_OUTPUT_LOW);

	gpio_pin_configure(ioex_it8801_port1, 4, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port1, 5, GPIO_OUTPUT_LOW);

	gpio_pin_configure(ioex_it8801_port2, 0, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port2, 1, GPIO_OUTPUT_LOW);
	gpio_pin_configure(ioex_it8801_port2, 2, GPIO_OUTPUT_HIGH);
	gpio_pin_configure(ioex_it8801_port2, 3, GPIO_OUTPUT_HIGH);

#endif
#if 0
	/* PWM test */
	const struct device *const pwm_dev = DEVICE_DT_GET(DT_NODELABEL(ioex_it8801_pwm7));

	printk(" board_hook=%p\n",pwm_dev);
	pwm_set_cycles(pwm_dev, 7, 100, 50, BIT(8));
#endif
}
DECLARE_HOOK(HOOK_INIT, board_hook, HOOK_PRIO_LAST);
