/*
 * Code assumes this is the only file writing to the control register (0x52). If this isnt the case
 * i2c_reads need to be added before every i2c_write to obtain the value
 */
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "printf.h"
#include "host_command.h"
#include "util.h"
#include <zephyr/drivers/i2c.h>


#define CPRINTF(format, args...) cprintf(CC_HERSH_I2C_ISH, format, ##args)

#define EC_CONTROL_REG  0x52
uint16_t ec_control_reg_val;


#define I2C_DEV_NODE	DT_ALIAS(i2c_0)
static uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
// static const struct device *const i2c_ec;
struct device *i2c_ec;


/**
 * @return 0 - success, 1 - failure
 */
int ec_init(void)
{
	i2c_ec = DEVICE_DT_GET(I2C_DEV_NODE);
	uint32_t i2c_cfg_tmp;
	
    if (!device_is_ready(i2c_ec)) {
		CPRINTF("I2C device is not ready\n");
		return 1;
	}

	/* 1. Verify i2c_configure() */
	if (i2c_configure(i2c_ec, i2c_cfg)) {
		CPRINTF("I2C config failed\n");
		return 1;
	}

	/* 2. Verify i2c_get_config() */
	if (i2c_get_config(i2c_ec, &i2c_cfg_tmp)) {
		CPRINTF("I2C get_config failed\n");
		return 1;
	}
	if (i2c_cfg != i2c_cfg_tmp) {
		CPRINTF("I2C get_config returned invalid config\n");
		return 1;
	}

    // initialized ec control register to 0
    ec_control_reg_val = 0x0000;    
	if(i2c_write(i2c_ec, &ec_control_reg_val, sizeof(ec_control_reg_val), EC_CONTROL_REG)) {
        CPRINTF("Failed to set control register to 0 via I2C");
        return 1;
    }
    return 0;
}
DECLARE_HOOK(HOOK_INIT, ec_init, HOOK_PRIO_DEFAULT);


void ec_enable_lid_interrupt(void)
{
    ec_control_reg_val |= 0b01;
    i2c_write(i2c_ec, &ec_control_reg_val, sizeof(ec_control_reg_val), EC_CONTROL_REG);
}