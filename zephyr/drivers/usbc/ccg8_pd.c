#include "usb_pd.h"
#include "console.h"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/ccg8_pd.h>
#include <usbc/utils.h>

#define CCG8_COMPAT_PD intel_pd_powermode

#define PD_CHIP_ENTRY(usbc_id, pd_id, config_fn) \
	[USBC_PORT_NEW(usbc_id)] = config_fn(pd_id),

#define CHECK_COMPAT(compat, usbc_id, pd_id, config_fn) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(pd_id, compat),  \
		    (PD_CHIP_ENTRY(usbc_id, pd_id, config_fn)), ())

#define PD_POW_CHIP_FIND(usbc_id, pd_id) \
	CHECK_COMPAT(CCG8_COMPAT_PD, usbc_id, pd_id, DEVICE_DT_GET)

#define PD_POW_CHIP(usbc_id)                                                      \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, pd_powmode),                \
		    (PD_POW_CHIP_FIND(usbc_id, DT_PHANDLE(usbc_id, pd_powmode))), \
		    ())

#define PD_MAX_WRITE_SIZE 4
/* Generate device tree for available PDs */
const struct device *pd_pow_config_array[] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, PD_POW_CHIP) };

#define DT_DRV_COMPAT intel_pd_powermode 
LOG_MODULE_REGISTER(INTEL_POWMODE, LOG_LEVEL_ERR);

struct ccg8_powmode_config {
        /* I2C config */
        struct i2c_dt_spec i2c;
        /*
         * PD interrupt to wake the task to configure alternate modes. There
         * can be individual Interrupt pin for each PD port or all the PD
         * interrupts can be muxed to single GPIO. This helps to keep common
         * code for single port / dual port PD solutions offered by different
         * PD vendors.
         */
        struct gpio_dt_spec int_gpio;
        /* Shared interrupt pin in dual port solution */
        bool shared_irq;
};

static int intel_write_powmode(const struct device *dev,
                               uint16_t reg, uint8_t len, void *data)
{
	const struct ccg8_powmode_config *cfg = dev->config;
	uint8_t i2c_buf[PD_MAX_WRITE_SIZE];
	struct i2c_msg msg;
	cprints(CC_USB, "In intel write power mode \n");
	/*
	 * Write sequence
	 * DEV_ADDR - REG_ID_0 - REG_ID_1 - DATA_LEN - DATA0 .. DATAn
	 */
	i2c_buf[0] = reg & 0x00ff;
	i2c_buf[1] = (reg & 0xff00) >> 8;
	memcpy(&i2c_buf[2], data, len);

	msg.buf = (uint8_t *)&i2c_buf;
	msg.len =  len + 2;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;
	return i2c_transfer_dt(&cfg->i2c, &msg, 1);

}

static const struct ccg8_hostcap_driver_api intel_ccg8_hostcap_driver_api = {
	.pd_write_powmode = intel_write_powmode,
};

#define CCG8_ALTMODE_DEFINE(inst)                                        \
	static const struct ccg8_powmode_config ccg8_powmode_config##inst = { \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),       \
		.shared_irq = DT_INST_PROP(inst, irq_shared),             \
	};                                                                \
                                                                          \
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL,             \
			      NULL,                    \
			      &ccg8_powmode_config##inst, POST_KERNEL,      \
			      CONFIG_APPLICATION_INIT_PRIORITY,           \
			      &intel_ccg8_hostcap_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CCG8_ALTMODE_DEFINE)
