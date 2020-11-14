/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Lightbar LED control for Lindar */

#include "ec_commands.h"
#include "led_common.h"
#include "i2c.h"
#include "ktd2064.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* Here is define for global variables */
static uint8_t LIGHTBAR_LIGHT_NUM;
static uint8_t LIGHTBAR_BRIGHTNESS;
static uint8_t LIGHTBAR_SET_COLOR;

/* write value to i2c register */
static void controller_write(uint8_t reg, uint8_t val)
{
	uint8_t buf[2];

	buf[0] = reg;
	buf[1] = val;
	i2c_xfer_unlocked(I2C_PORT_LIGHTBAR, KTD2064_I2C_ADDR,
				buf, 2, 0, 0,
				I2C_XFER_SINGLE);
}


void lightbar_help(void)
{
	CPRINTS("ectool lightbar on");
	CPRINTS("ectool lightbar off");
	CPRINTS("ectool lightbar LED [brightness][color][number]");
	CPRINTS("	brightness range from 0x00 ~ 0xff");
	CPRINTS("	color of light, the range is 0x00 ~ 0x04");
	CPRINTS("		0x00 - red");
	CPRINTS("		0x01 - green");
	CPRINTS("		0x02 - bule");
	CPRINTS("		0x03 - yellow");
	CPRINTS("		0x04 - white");
	CPRINTS("	number of LEDs, the range is 0x00 ~ 0x04");
	CPRINTS("		0x00 - turn on two LEDs");
	CPRINTS("		0x01 - turn on four LEDs");
	CPRINTS("		0x02 - turn on six LEDs");
	CPRINTS("		0x03 - turn on eight LEDs");
	CPRINTS("		0x04 - turn on ten LEDs");
}
/* Lightbar LED color setting */
void lightbar_set_color(enum lightbar_color color)
{
	switch (color) {
	case LIGHTBAR_COLOR_RED:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_COLOR_RED, LIGHTBAR_BRIGHTNESS);
		controller_write(KTD2064_COLOR_GREEN, 0x00);
		controller_write(KTD2064_COLOR_BLUE, 0x00);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case LIGHTBAR_COLOR_GREEN:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_COLOR_RED, 0x00);
		controller_write(KTD2064_COLOR_GREEN, LIGHTBAR_BRIGHTNESS);
		controller_write(KTD2064_COLOR_BLUE, 0x00);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case LIGHTBAR_COLOR_BLUE:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_COLOR_RED, 0x00);
		controller_write(KTD2064_COLOR_GREEN, 0x00);
		controller_write(KTD2064_COLOR_BLUE, LIGHTBAR_BRIGHTNESS);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case LIGHTBAR_COLOR_CYAN:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_COLOR_RED, LIGHTBAR_BRIGHTNESS);
		controller_write(KTD2064_COLOR_GREEN, LIGHTBAR_BRIGHTNESS);
		controller_write(KTD2064_COLOR_BLUE, 0x00);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case LIGHTBAR_COLOR_WHITE:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_COLOR_RED, LIGHTBAR_BRIGHTNESS);
		controller_write(KTD2064_COLOR_GREEN, LIGHTBAR_BRIGHTNESS);
		controller_write(KTD2064_COLOR_BLUE, LIGHTBAR_BRIGHTNESS);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	default:
		CPRINTS("the color of light from 0 ~ 4");
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(LED_INIT, 0x00);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	}
}

/* Lightbar LED number setting */
void lightbar_set_number(uint8_t set_num)
{
	switch (set_num) {
	case 0:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_NUM_1, LED_TURN_OFF);
		controller_write(KTD2064_NUM_2, LED_TURN_OFF);
		controller_write(KTD2064_NUM_3, LED_TURN_OFF);
		controller_write(KTD2064_NUM_4, LED_TURN_OFF);
		controller_write(KTD2064_NUM_5, LED_TURN_OFF);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case 1:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_NUM_1, LED_TURN_OFF);
		controller_write(KTD2064_NUM_2, LED_TURN_ON);
		controller_write(KTD2064_NUM_3, LED_TURN_OFF);
		controller_write(KTD2064_NUM_4, LED_TURN_OFF);
		controller_write(KTD2064_NUM_5, LED_TURN_OFF);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case 2:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_NUM_1, LED_TURN_ON);
		controller_write(KTD2064_NUM_2, LED_TURN_ON);
		controller_write(KTD2064_NUM_3, LED_TURN_OFF);
		controller_write(KTD2064_NUM_4, LED_TURN_OFF);
		controller_write(KTD2064_NUM_5, LED_TURN_OFF);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case 3:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_NUM_1, LED_TURN_ON);
		controller_write(KTD2064_NUM_2, LED_TURN_ON);
		controller_write(KTD2064_NUM_3, LED_TURN_OFF);
		controller_write(KTD2064_NUM_4, LED_TURN_ON);
		controller_write(KTD2064_NUM_5, LED_TURN_OFF);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case 4:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_NUM_1, LED_TURN_ON);
		controller_write(KTD2064_NUM_2, LED_TURN_ON);
		controller_write(KTD2064_NUM_3, LED_TURN_ON);
		controller_write(KTD2064_NUM_4, LED_TURN_ON);
		controller_write(KTD2064_NUM_5, LED_TURN_OFF);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	case 5:
		i2c_lock(I2C_PORT_LIGHTBAR, 1);
		controller_write(KTD2064_NUM_1, LED_TURN_ON);
		controller_write(KTD2064_NUM_2, LED_TURN_ON);
		controller_write(KTD2064_NUM_3, LED_TURN_ON);
		controller_write(KTD2064_NUM_4, LED_TURN_ON);
		controller_write(KTD2064_NUM_5, LED_TURN_ON);
		i2c_lock(I2C_PORT_LIGHTBAR, 0);
		break;
	default:
		CPRINTS("the light setting number from 0 ~ 5");
	}
}

/* Off - turn off lightbar */
void lightbar_turn_off(void)
{
	i2c_lock(I2C_PORT_LIGHTBAR, 1);
	controller_write(LED_INIT, 0x00);
	i2c_lock(I2C_PORT_LIGHTBAR, 0);
}

/* On - turn on lightbar */
void lightbar_turn_on(void)
{
	i2c_lock(I2C_PORT_LIGHTBAR, 1);
	controller_write(LED_INIT, 0x80);
	i2c_lock(I2C_PORT_LIGHTBAR, 0);
}

/* Initial - Lightbar initial  */
void lightbar_init(void)
{
	i2c_lock(I2C_PORT_LIGHTBAR, 1);
	controller_write(KTD2064_NUM_1, LED_TURN_ON);
	controller_write(KTD2064_NUM_2, LED_TURN_ON);
	controller_write(KTD2064_NUM_3, LED_TURN_ON);
	controller_write(KTD2064_NUM_4, LED_TURN_ON);
	controller_write(KTD2064_NUM_5, LED_TURN_ON);
	i2c_lock(I2C_PORT_LIGHTBAR, 0);
}

/* ectool lightbar on
 * ectool lightbar off
 * ectool lightbar LED [brightness][color][number]
 *	brightness range from 0x00 ~ 0xff
 *	color of light, the range is 0x00 ~ 0x04
 *		0x00 - red
 *		0x01 - green
 *		0x02 - bule
 *		0x03 - yellow
 *		0x04 - white
 *	number of LEDs, the range is 0x00 ~ 0x04
 *		0x00 - turn on two LEDs
 *		0x01 - turn on four LEDs
 *		0x02 - turn on six LEDs
 *		0x03 - turn on eight LEDs
 *		0x04 - turn on ten LEDs
 */
static enum ec_status host_command_lightbar(struct host_cmd_handler_args *args)
{
	const struct ec_params_lightbar *in = args->params;

	switch (in->cmd) {
	case LIGHTBAR_CMD_OFF:
		lightbar_turn_off();
		break;
	case LIGHTBAR_CMD_ON:
		lightbar_init();
		lightbar_turn_on();
		break;
	case LIGHTBAR_CMD_SET_RGB:
		LIGHTBAR_BRIGHTNESS = in->reg.reg;
		LIGHTBAR_SET_COLOR = in->reg.value;
		LIGHTBAR_LIGHT_NUM = in->set_rgb.blue;
		if (LIGHTBAR_SET_COLOR > 5 || LIGHTBAR_LIGHT_NUM > 5)
			return EC_RES_INVALID_PARAM;
		lightbar_init();
		lightbar_turn_on();
		lightbar_set_number(LIGHTBAR_LIGHT_NUM);
		lightbar_set_color(LIGHTBAR_SET_COLOR);
		break;
	default:
		lightbar_help();
		return EC_RES_INVALID_PARAM;
	}
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_LIGHTBAR_CMD,
		host_command_lightbar, EC_VER_MASK(0));

