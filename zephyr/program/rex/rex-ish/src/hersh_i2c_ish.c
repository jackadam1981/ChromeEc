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
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
// #define CONFIG_I2C_TARGET 1
#include <zephyr/drivers/i2c.h>
// #include <i2c_npcx_controller.h>
// #include <zephyr/
// #include <i2c/i2c.h> // shimmed code at ec/zephyr/shim/include/i2c/i2c.h
// #include <zephyr/logging/log.h>


#define CPRINTF(format, args...) cprintf(CC_HERSH_I2C_ISH, format, ##args)

#define REG_LEN_BYTES 2
#define EC_CONTROL_REG  0x52
uint8_t ec_control_reg_val[REG_LEN_BYTES]; // register is 2 bytes long
uint8_t i2c_read_val[REG_LEN_BYTES];

#define I2C_BUS_NODE	DT_ALIAS(i2c_0)
// #define I2C_EC_NODE		DT_ALIAS(nuvoton_ec)
static uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
// static const struct device *const i2c_ec;
const struct device *const i2c_0_dev = DEVICE_DT_GET(I2C_BUS_NODE);
#define EC_DEV DT_NODELABEL(nuvoton_npcx99_ec)
#if DT_NODE_HAS_STATUS(EC_DEV, okay)
	const struct device *const ec_dev = DEVICE_DT_GET(DT_NODELABEL(nuvoton_npcx99_ec));
#else
	#error "Node is disabled"
#endif
struct i2c_target_config i2c_ec_cfg;
#define EC_I2C_ADDR 0x56

bool i2c_init_success = false;


int i2c_target_error_wrapper(int (*f)(const struct device*, struct i2c_target_config*), const struct device *dev, struct i2c_target_config *cfg, const char str[]) {
	int ret;
	switch(ret = (*f)(dev, cfg)) {
		case -EINVAL:
			CPRINTF("%s parameters are invalid\n", str);
			return 1;
		case -EIO:
			CPRINTF("%s general input/output error\n", str);
			return 1;
		case -ENOSYS :
			CPRINTF("%s target mode is not implemented\n", str);
			return 1;
		case -EBUSY : 
			CPRINTF("%s atomic_test_and_set_bit returned true, or transaction is ongoing\n", str);
			return 1;
		case 0 : // target registered sucessfully 
			break;
		default:
			CPRINTF("%s Unspecified case %d, -EINVAL %d, -EIO %d, -ENOSYS %d\n", str, ret, -EINVAL, -EIO, -ENOSYS);
			return 1;
	}
	return 0;
}


void ish_i2c_init(void)
{
	uint32_t i2c_cfg_tmp;
	
	if (!device_is_ready(i2c_0_dev)) {
		CPRINTF("I2C device is not ready\n");
		return;
	}

	/* 1. Verify i2c_configure() */
	if (i2c_configure(i2c_0_dev, i2c_cfg)) {
		CPRINTF("I2C config failed\n");
		return;
	}

	/* 2. Verify i2c_get_config() */
	if (i2c_get_config(i2c_0_dev, &i2c_cfg_tmp)) {
		CPRINTF("I2C get_config failed\n");
		return;
	}
	if (i2c_cfg != i2c_cfg_tmp) {
		CPRINTF("I2C get_config returned invalid config\n");
		return;
	}

	/* 3. Register EC as Target */
	if(ec_dev == NULL) {
		CPRINTF("Failed to get EC Device Binding\n");
		return;
	}
	if(!device_is_ready(ec_dev)) {
		CPRINTF("EC I2C Device is not ready\n");
		return;
	}
	i2c_ec_cfg.address = EC_I2C_ADDR;
	if(i2c_target_error_wrapper(i2c_target_register, ec_dev, &i2c_ec_cfg, "I2C Target driver register - "))
		return;

	/* 4. initialized ec control register to 0 */
	memset(ec_control_reg_val, 0, REG_LEN_BYTES);
	i2c_init_success = true;
}
DECLARE_HOOK(HOOK_INIT, ish_i2c_init, HOOK_PRIO_DEFAULT);


bool i2c_unregister_success = false;
void i2c_unregister(void)
{
	/* Unregister EC as Target */
	if(i2c_target_error_wrapper(i2c_target_unregister, ec_dev, &i2c_ec_cfg, "I2C Target driver unregister - "))
		return;
	i2c_unregister_success = true;
}
// DECLARE_HOOK(HOOK_CHIPSET_RESET, i2c_unregister, HOOK_PRIO_DEFAULT);


static volatile int count = 0;
void i2c_test(void)
{
	if(count++%15 == 0) {
		// i2c_unregister_success = false;
		// i2c_init_success = false;
		// ish_i2c_init();
		// if(!i2c_init_success) {
		// 	CPRINTF("i2c init failure\n");
		// 	return;
		// }
		// read value of SMBus register to ensure it is set

		// if(i2c_write(ec_dev, ec_control_reg_val, REG_LEN_BYTES, EC_CONTROL_REG)) {
		// if(i2c_write(i2c_0_dev, ec_control_reg_val, REG_LEN_BYTES, EC_CONTROL_REG)) {
		if(i2c_reg_write_byte(ec_dev, EC_I2C_ADDR, EC_CONTROL_REG, 0x28 /* 0x00 */)) {
			CPRINTF("Failed to set control register to 0 via I2C\n");
			// return;
		} 
		else
			CPRINTF("Write works\n");
		
		// i2c_unregister();
		// if(!i2c_unregister_success) {
		// 	CPRINTF("i2c unregister failure\n");
		// }
	}
}
DECLARE_HOOK(HOOK_SECOND, i2c_test, HOOK_PRIO_DEFAULT);


void ec_enable_lid_interrupt(void)
{
    ec_control_reg_val[0] |= 0b01;
    i2c_write(i2c_0_dev, ec_control_reg_val, REG_LEN_BYTES, EC_CONTROL_REG);
}


void ec_disable_lid_interrupt(void)
{
    ec_control_reg_val[0] &= 0b10;
    i2c_write(i2c_0_dev, ec_control_reg_val, REG_LEN_BYTES, EC_CONTROL_REG);
}

// void test_ish_i2c(void)
// {
// 	int readret = i2c_read(i2c_ec, i2c_read_val, REG_LEN_BYTES, EC_CONTROL_REG);

//     // ec_control_reg_val[0] |= 0b01;
//     int writeret = i2c_write(i2c_ec, ec_control_reg_val, REG_LEN_BYTES, EC_CONTROL_REG);
// 	CPRINTF(
// 		"ec_control_reg: %d || I2C (Write, Read) Works: %s %s\n", 
// 		i2c_read_val[0] + i2c_read_val[1], 
// 		writeret ? "false" : "true ",
// 		readret ? "false " : "true "
// 	);
// 	ec_control_reg_val[0]++;
// }
// DECLARE_HOOK(HOOK_SECOND, test_ish_i2c, HOOK_PRIO_DEFAULT);


