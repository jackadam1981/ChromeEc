/*
Relevant Files
--------------
* Tablet Mode
	/home/hershjos/Repos/chromiumos/src/platform/ec/include/tablet_mode.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/common/tablet_mode.c
* Lid Switch
	/home/hershjos/Repos/chromiumos/src/platform/ec/include/lid_switch.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/common/lid_switch.c
* I2C Docs
	/home/hershjos/Repos/chromiumos/src/platform/ec/docs/zephyr/zephyr_i2c.md
* Sample I2C Code
	This is especially useful: /home/hershjos/Repos/chromiumos/src/third_party/zephyr/main/tests/drivers/i2c/i2c_api/src/test_i2c.c
	/home/hershjos/Repos/chromiumos/src/third_party/zephyr/main/tests/drivers/i2c/i2c_target_api/src/main.c
	Example I2C binding/yaml file by Vijay: https://chromium-review.googlesource.com/c/chromiumos/platform/ec/+/3600628/7/zephyr/dts/bindings/kb_discrete/ite%2Cit8801.yaml
	/home/hershjos/Repos/chromiumos/src/platform/ec/driver/accel_bma4xx.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/driver/accel_bma4xx.c
* I2C
	/home/hershjos/Repos/chromiumos/src/third_party/zephyr/main/include/zephyr/drivers/i2c.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/include/i2c.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/common/i2c_controller.c
	Example files:
		/home/hershjos/Repo/chromiumos/src/platform/ec/driver/accel_bma4xx.c
		/home/hershjos/Repos/chromiumos/src/platform/ec/driver/accel_bma4xx.h
	refer to this touse the i2c0 port 0 in DT source file https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/arm/npcx9m6f_evb/npcx9m6f_evb.dts#L87-L96
	another example: https://github.com/zephyrproject-rtos/zephyr/pull/58217
	/home/hershjos/Repos/chromiumos/src/third_party/zephyr/main/drivers/i2c/target/eeprom_target.c
* Hooks
	/home/hershjos/Repos/chromiumos/src/platform/ec/include/hooks.h
* Schematic - rex_proto_schematic
	ec schematic p30
	I2C map p5
	MTL (meteorlake SoC) i2c schematic p9
		maybe its on the right, the 
			SOC_ISH_I2C_SENSOR_SCL and EC_I2C_SENSOR_SCL
			SOC_ISH_I2C_SENSOR_SDA and EC_I2C_SENSOR_SDA
			SOC_ACCEL_INT_L and SOC_ISH_ACCEL_INT_L
* Register access
	REG16: /home/hershjos/Repos/chromiumos/src/platform/ec/include/common.h
	GET_FIELD, SET_FIELD: /home/hershjos/Repos/chromiumos/src/platform/ec/chip/npcx/registers.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/chip/npcx/registers-npcx9.h
	example of reading registers
	/home/hershjos/Repos/chromiumos/src/platform/ec/chip/npcx/adc.c

* Other
	/home/hershjos/Repos/chromiumos/src/platform/ec/zephyr/program/rex/rex-ish/i2c.dtsi
* Files I've added:
	/home/hershjos/Repos/chromiumos/src/platform/ec/zephyr/dts/bindings/i2c_device/intel,mtl-ish.yaml
	

Interrupt Logic
---------------
________                 _______________
INT     \_______________/
		^				^
		Interrupt		Status REG
		Fired			read
		by EC			by ISH (in AP)

1. Control register set by AP/OS
	a. As soon as the ISH boots, it should configure the control register 
	(see section 1.5 table 1.5.3)
2. If control register is enabled, triggers an interrupt
3. If interrupt is triggered pull INT line low
4. INT line goes back to default (high) when status register is read by ISH in 
   AP

Logic
-----

Resources
---------
* 

TODO
----
* add a callback function to a function in chromiumos/src/third_party/zephyr/drivers/i2c_npcx_controller.h
	* one of the structs passed to some function can have a function tied to it that is called when it detects an i2c_read or i2c_write being made to the EC
	* definition of struct i2c_target_config chromiumos/src/third_party/zephyr/main/include/zephyr/drivers/i2c.h: 443
		struct i2c_target_config { 
 			sys_snode_t node; 
 			uint8_t flags; 
 			uint16_t address; 
 			// Callback functions 
			const struct i2c_target_callbacks *callbacks; 
		};
	* callback definition from line 420
		struct i2c_target_callbacks {
			i2c_target_write_requested_cb_t write_requested;
			i2c_target_read_requested_cb_t read_requested;
			i2c_target_write_received_cb_t write_received;
			i2c_target_read_processed_cb_t read_processed;
		#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
			i2c_target_buf_write_received_cb_t buf_write_received;
			i2c_target_buf_read_requested_cb_t buf_read_requested;
		#endif
			i2c_target_stop_cb_t stop;
		};
i	* chromiumos/src/third_party/zephyr/main/drivers/i2c/target/eeprom_target.c has examples of using this
*/

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "printf.h"
#include "host_command.h"
#include "util.h"
#include "lid_switch.h"
#include "tablet_mode.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>


#define CPRINTF(format, args...) cprintf(CC_HERSH_I2C_EC, format, ##args)

#define EC_CONTROL_REG  0x52

// void ec_i2c_init(void)
// {
// 	i2c_target_register();
// }
// DECLARE_HOOK(HOOK_INIT, ec_i2c_init, HOOK_PRIO_DEFAULT);
// DECLARE_HOOK(HOOK_SECOND, ec_i2c_init, HOOK_PRIO_DEFAULT);

// static const struct device *const gpio_interrupt_pin = DEVICE_DT_GET(SOC_ISH_ACCEL_INT_L or SOC_ISH_ACCEL_INT_L);


// /**
//  * See if I can use this https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html#details
//  */
// void trigger_interrupt(void)
// {
//  	// bottom 2 bits correspond to if tablet or lid state is enabled
// 	if(REG16(EC_CONTROL_REG) & 0b11) {

// 		// trigger interrupt
// // // 		// gpio_set_level();
// 	}
// }
// DECLARE_HOOK(HOOK_LID_CHANGE, trigger_interrupt, HOOK_PRIO_DEFAULT);
// DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, trigger_interrupt, HOOK_PRIO_DEFAULT);


/*
Test/debugging/learning coe
*/
void read_tablet_lid_state(void)
{
	int lid_state = lid_is_open();
	// int tablet_state = tablet_get_mode();
	int tablet_state = 0;
	CPRINTF("\nL%d T%d\n", lid_state, tablet_state);
}
DECLARE_HOOK(HOOK_LID_CHANGE, read_tablet_lid_state, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, read_tablet_lid_state, HOOK_PRIO_DEFAULT);
// DECLARE_HOOK(HOOK_SECOND, read_tablet_lid_state, HOOK_PRIO_DEFAULT); // for debugging only


#define SMB0ADDR1 0x40009008
static volatile int counter = 0;
void read_reg(void)
{
	if(counter++%15 == 0) {
		CPRINTF(
			"REG 0x52: %d. REG SMBUSn: ADDR (SMBUSn & 0x7F): %d, SLAVE: ((SMBUSn & 0x80) >> 7) %d\n", 
			REG32(EC_CONTROL_REG), 
			REG32(SMB0ADDR1) & 0x7F, 
			(REG32(SMB0ADDR1) & 0x80) >> 7
		);
	}
}
DECLARE_HOOK(HOOK_SECOND, read_reg, HOOK_PRIO_DEFAULT);


// #define I2C_BUS_NODE	DT_ALIAS(i2c_0)
// #define I2C_BUS_NODE	DT_ALIAS(MTL_ISH)
// static uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
// uint8_t buffer[] = "hello world\n";
// #define ISH_REG 0x00
/**
 * Writes to ISH over I2C
 * 
 * Resources:/home/hershjos/Repos/chromiumos/src/third_party/zephyr/main/tests/drivers/i2c/i2c_api/src/test_i2c.c
 */
// void ish_i2c_write(void)
// {
// 	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_BUS_NODE);
// 	uint32_t i2c_cfg_temp;

// 	CPRINTF("I2C Device is ready: 			%s\n", !device_is_ready(i2c_dev) ? "false" : "true");
// 	CPRINTF("I2C configure Worked:			%s\n", i2c_configure(i2c_dev, i2c_cfg) ? "false" : "true");
// 	CPRINTF("I2C get_config Worked: 			%s\n", i2c_get_config(i2c_dev, &i2c_cfg_temp) ? "false" : "true");
// 	CPRINTF("I2C get_config returned valid config: 	%s\n", i2c_cfg == i2c_cfg_temp ? "true" : "false");
// 	// CPRINTF("I2C Write Worked: 				%s\n", i2c_write(i2c_dev, buffer, 1, ISH_REG) ? "false" : "true");

// 	return;
// }
// DECLARE_HOOK(HOOK_SECOND, ish_i2c_write, HOOK_PRIO_DEFAULT);
