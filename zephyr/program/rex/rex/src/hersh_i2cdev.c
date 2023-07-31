/*
Steps
1. Create the file hersh_i2cdev.c in chromiumos/src/platform/ec/zephyr/program/
   rex/rex/src/hersh_i2cdev.c (this file)
2. Edit chromiumos/src/platform/ec/zephyr/program/rex/CMakeList.txt
	a. add zephyr_library_sources("src/rex/hersh_i2cdev.c")
3. edit /home/hershjos/Repos/chromiumos/src/platform/ec/include/console_channel.inc
	add this at the end CONSOLE_CHANNEL(CC_HERSH_I2CDEV, "hersh_i2cdev")

Relevant Files
--------------
* Tablet Mode
	/home/hershjos/Repos/chromiumos/src/platform/ec/include/tablet_mode.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/common/tablet_mode.c
* Lid Switch
	/home/hershjos/Repos/chromiumos/src/platform/ec/include/lid_switch.h
* Docs
	/home/hershjos/Repos/chromiumos/src/platform/ec/docs/zephyr/zephyr_i2c.md
* Sample I2C Code
	/home/hershjos/Repos/chromiumos/src/platform/ec/driver/accel_bma4xx.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/driver/accel_bma4xx.c
* I2C
	/home/hershjos/Repos/chromiumos/src/platform/ec/include/i2c.h
	/home/hershjos/Repos/chromiumos/src/platform/ec/common/i2c_controller.c
	/home/hershjos/Repos/chromiumos/src/platform/ec/docs/zephyr/zephyr_i2c.md
	Example files:
		/home/hershjos/Repos/chromiumos/src/platform/ec/driver/accel_bma4xx.c
		/home/hershjos/Repos/chromiumos/src/platform/ec/driver/accel_bma4xx.h
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

Interrupt Logic
---------------
________                 _______________
INT     \_______________/
		^		^
		Interrupt	Status REG
		Fired		read
		by EC		by ISH (in AP)

1. Control register set by AP/OS
	a. As soon as the ISH boots, it should configure the control register 
	(see section 1.5 table 1.5.3)
2. If control register is enabled, triggers an interrupt
3. If interrupt is triggered pull INT line low
4. INT line goes back to default (high) when status register is read by ISH in 
   AP

Logic
-----

TODO
----
* focus on writing basic I2C code
*/

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "printf.h"
#include "host_command.h"
#include "i2c.h"
#include "util.h"
#include "lid_switch.h"
// #include "task.h"
// #include "uart.h"
// #include "i2cdev.h"

#define CPRINTF(format, args...) cprintf(CC_HERSH_I2CDEV, format, ##args)

#define NPCX_EC_STATUS_REG 0x51

// TODO: figure out the actual port and address flags
#define EC_I2C_ADDR_FLAGS 0x86


// TODO: figure out the actual port and address flags
/** 
 * Reads lid sensor value
 *
 * @return		0: lid open, 1: lid closed
 */
bool read_lid_sensor_value(void)
{
	return REG16(NPCX_EC_STATUS_REG) & 0b01;
}


/** 
 * Reads tablet mode sensor value
 *
 * @return		0: laptop mode, 1: tablet mode
 */
bool read_tablet_sensor_value(void)
{
	return REG16(NPCX_EC_STATUS_REG) & 0b10;
}


unsigned long long counter = 0;
void ec_write8(void)
{
	volatile bool lid_state = read_tablet_sensor_value();
	volatile bool tablet_state = read_tablet_sensor_value();

	// const int port = I2C_PORT_SENSOR; // I2C0
	// const uint16_t addr_flags = TODO;
	// const int offset = TODO; // register
	// int data = TODO;	
	// i2c_write8(port, addr_flags, offset, data);
	if(!(counter++ % 10))
		CPRINTF("\nL%d T%d\n", lid_state, tablet_state);
		// CPRINTF("\nREG%d\n", REG16(NPCX_EC_STATUS_REG));
	// TODO: check if a delay is needed
}
DECLARE_HOOK(HOOK_SECOND, ec_write8, HOOK_PRIO_DEFAULT);
// DECLARE_HOOK(HOOK_LID_CHANGE, ec_write8, HOOK_PRIO_DEFAULT);
// DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, ec_write8, HOOK_PRIO_DEFAULT);

// TODO: ec registers are actually 16 bytes, so make a ec_write16 version