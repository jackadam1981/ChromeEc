#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/peci.h>

struct device *pdev;

int usb_trans_init(void)
{
	int ret = 0;

	ret = peci_enable(pdev);
	//ret = usb_enable(NULL);
	if (ret != 0) {
		printk("Failed to enable USB\n");
		return ret;
	}

	//usb_dc_attach();
	//ret = udc_enable(pdev);
	//ret = usb_deconfig();
	ret = peci_disable(pdev);
	if (ret != 0) {
		printk("Failed to deconfig USB\n");
		return ret;
	}

	ret = device_is_ready(pdev);
	printk("USB deconfigured successfully\n");

	return ret;
}

SYS_INIT(usb_trans_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
