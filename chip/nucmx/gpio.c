/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for Chrome EC */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "keyboard_config.h"
#include "hooks.h"
#include "registers.h"
#include "switch.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Marco functions for GPIO WUI/ALT table */
#define NUCMX_GPIO(grp, pin)	\
	GPIO_PORT_##grp, MASK_PIN##pin
#define NUCMX_GPIO_NONE \
	GPIO_PORT_COUNT, 0xFF
#define NUCMX_WUI(tbl, grp, pin)	\
	MIWU_TABLE_##tbl, MIWU_GROUP_##grp, MASK_PIN##pin

#define ALT_MASK(pin) \
	CONCAT2(MASK_PIN, pin)
#define ALT_PIN(grp,pin) \
	ALT_MASK(NUCMX_DEVALT##grp##_##pin)
#define NUCMX_ALT(grp, pin)	\
	ALT_GROUP_##grp,ALT_PIN(grp, pin)

struct gpio_wui_map {
	uint8_t gpio_port;
	uint8_t gpio_mask;
	uint8_t wui_table;
	uint8_t wui_group;
	uint8_t wui_mask;
} ;

struct gpio_wui_item{
	struct  gpio_wui_map wui_map[8];
	uint8_t irq;
};

const struct gpio_wui_item gpio_wui_table[] = {
	/* MIWU0 Group A */
	{ {	{ NUCMX_GPIO(8,0), NUCMX_WUI(0,1,0) },
		{ NUCMX_GPIO(8,1), NUCMX_WUI(0,1,1) },
		{ NUCMX_GPIO(8,2), NUCMX_WUI(0,1,2) },
		{ NUCMX_GPIO(8,3), NUCMX_WUI(0,1,3) },
		{ NUCMX_GPIO(8,4), NUCMX_WUI(0,1,4) },
		{ NUCMX_GPIO(8,5), NUCMX_WUI(0,1,5) },
		{ NUCMX_GPIO(8,6), NUCMX_WUI(0,1,6) },
		{ NUCMX_GPIO(8,7), NUCMX_WUI(0,1,7) },
	  },NUCMX_IRQ_MTC_WKINTAD_0 },
	/* MIWU0 Group B */
	{ {	{ NUCMX_GPIO(9,0), NUCMX_WUI(0,2,0) },
		{ NUCMX_GPIO(9,1), NUCMX_WUI(0,2,1) },
		{ NUCMX_GPIO(9,2), NUCMX_WUI(0,2,2) },
		{ NUCMX_GPIO(9,3), NUCMX_WUI(0,2,3) },
		{ NUCMX_GPIO(9,4), NUCMX_WUI(0,2,4) },
		{ NUCMX_GPIO(9,5), NUCMX_WUI(0,2,5) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,2,6) },	/* MSWC Wake-Up	*/
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,2,7) },	/* T0OUT Wake-Up */
	  },  NUCMX_IRQ_TWD_WKINTB_0 },
	/* MIWU0 Group C */
	{ {	{ NUCMX_GPIO(9,6), NUCMX_WUI(0,3,0) },
		{ NUCMX_GPIO(9,7), NUCMX_WUI(0,3,1) },
		{ NUCMX_GPIO(A,0), NUCMX_WUI(0,3,2) },
		{ NUCMX_GPIO(A,1), NUCMX_WUI(0,3,3) },
		{ NUCMX_GPIO(A,2), NUCMX_WUI(0,3,4) },
		{ NUCMX_GPIO(A,3), NUCMX_WUI(0,3,5) },
		{ NUCMX_GPIO(A,4), NUCMX_WUI(0,3,6) },
		{ NUCMX_GPIO(A,5), NUCMX_WUI(0,3,7) },
	  },  NUCMX_IRQ_WKINTC_0 },
	/* MIWU0 Group D */
	{ {	{ NUCMX_GPIO(A,6), NUCMX_WUI(0,4,0) },
		{ NUCMX_GPIO(A,7), NUCMX_WUI(0,4,1) },
		{ NUCMX_GPIO(B,0), NUCMX_WUI(0,4,2) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,4,3) },	/* SMB0 Wake-Up	*/
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,4,4) },	/* SMB1 Wake-Up	*/
		{ NUCMX_GPIO(B,1), NUCMX_WUI(0,4,5) },
		{ NUCMX_GPIO(B,2), NUCMX_WUI(0,4,6) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,4,7) },	/* MTC Wake-Up	*/
	  },  NUCMX_IRQ_MTC_WKINTAD_0 },

	/* MIWU0 Group E */
	{ {	{ NUCMX_GPIO(B,3), NUCMX_WUI(0,5,0) },
	    { NUCMX_GPIO(B,4), NUCMX_WUI(0,5,1) },
	    { NUCMX_GPIO(B,5), NUCMX_WUI(0,5,2) },
	    { NUCMX_GPIO_NONE, NUCMX_WUI(0,5,3) },
	    { NUCMX_GPIO(B,7), NUCMX_WUI(0,5,4) },
	    { NUCMX_GPIO_NONE, NUCMX_WUI(0,5,5) },
	    { NUCMX_GPIO_NONE, NUCMX_WUI(0,5,6) },	/* Host Access Wake-Up	*/
	    { NUCMX_GPIO_NONE, NUCMX_WUI(0,5,7) },	/* LRESET Wake-Up	*/
	  },  NUCMX_IRQ_WKINTEFGH_0 },

	/* MIWU0 Group F */
	{ {	{ NUCMX_GPIO(C,0), NUCMX_WUI(0,6,0) },
		{ NUCMX_GPIO(C,1), NUCMX_WUI(0,6,1) },
		{ NUCMX_GPIO(C,2), NUCMX_WUI(0,6,2) },
		{ NUCMX_GPIO(C,3), NUCMX_WUI(0,6,3) },
		{ NUCMX_GPIO(C,4), NUCMX_WUI(0,6,4) },
		{ NUCMX_GPIO(C,5), NUCMX_WUI(0,6,5) },
		{ NUCMX_GPIO(C,6), NUCMX_WUI(0,6,6) },
		{ NUCMX_GPIO(C,7), NUCMX_WUI(0,6,7) },
	  },  NUCMX_IRQ_WKINTEFGH_0 },
	/* MIWU0 Group G */
	{ {	{ NUCMX_GPIO(D,0), NUCMX_WUI(0,7,0) },
		{ NUCMX_GPIO(D,1), NUCMX_WUI(0,7,1) },
		{ NUCMX_GPIO(D,2), NUCMX_WUI(0,7,2) },
		{ NUCMX_GPIO(D,3), NUCMX_WUI(0,7,3) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,7,4) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,7,5) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,7,6) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,7,7) },
	  },  NUCMX_IRQ_WKINTEFGH_0 },
	/* MIWU0 Group H */
	{ {	{ NUCMX_GPIO_NONE, NUCMX_WUI(0,8,0) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,8,1) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,8,2) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,8,3) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,8,4) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,8,5) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(0,8,6) },
		{ NUCMX_GPIO(E,7), NUCMX_WUI(0,8,7) },
	  },  NUCMX_IRQ_WKINTEFGH_0 },

	/* MIWU1 Group A */
	{ {	{ NUCMX_GPIO(0,0), NUCMX_WUI(1,1,0) },
		{ NUCMX_GPIO(0,1), NUCMX_WUI(1,1,1) },
		{ NUCMX_GPIO(0,2), NUCMX_WUI(1,1,2) },
		{ NUCMX_GPIO(0,3), NUCMX_WUI(1,1,3) },
		{ NUCMX_GPIO(0,4), NUCMX_WUI(1,1,4) },
		{ NUCMX_GPIO(0,5), NUCMX_WUI(1,1,5) },
		{ NUCMX_GPIO(0,6), NUCMX_WUI(1,1,6) },
		{ NUCMX_GPIO(0,7), NUCMX_WUI(1,1,7) },
	  },NUCMX_IRQ_WKINTA_1 },
	/* MIWU1 Group B */
	{ {	{ NUCMX_GPIO(1,0), NUCMX_WUI(1,2,0) },
		{ NUCMX_GPIO(1,1), NUCMX_WUI(1,2,1) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(1,2,2) },
		{ NUCMX_GPIO(1,3), NUCMX_WUI(1,2,3) },
		{ NUCMX_GPIO(1,4), NUCMX_WUI(1,2,4) },
		{ NUCMX_GPIO(1,5), NUCMX_WUI(1,2,5) },
		{ NUCMX_GPIO(1,6), NUCMX_WUI(1,2,6) },
		{ NUCMX_GPIO(1,7), NUCMX_WUI(1,2,7) },
	  },  NUCMX_IRQ_WKINTB_1 },
	/* MIWU1 Group C -- Skipping */
	/* MIWU1 Group D */
	{ {	{ NUCMX_GPIO(2,0), NUCMX_WUI(1,4,0) },
		{ NUCMX_GPIO(2,1), NUCMX_WUI(1,4,1) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(1,4,2) },
		{ NUCMX_GPIO(3,3), NUCMX_WUI(1,4,3) },
		{ NUCMX_GPIO(3,4), NUCMX_WUI(1,4,4) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(1,4,5) },
		{ NUCMX_GPIO(3,6), NUCMX_WUI(1,4,6) },
		{ NUCMX_GPIO(3,7), NUCMX_WUI(1,4,7) },
	  },  NUCMX_IRQ_WKINTD_1 },

	/* MIWU1 Group E */
	{ {	{ NUCMX_GPIO(4,0), NUCMX_WUI(1,5,0) },
	    { NUCMX_GPIO(4,1), NUCMX_WUI(1,5,1) },
	    { NUCMX_GPIO(4,2), NUCMX_WUI(1,5,2) },
	    { NUCMX_GPIO(4,3), NUCMX_WUI(1,5,3) },
	    { NUCMX_GPIO(4,4), NUCMX_WUI(1,5,4) },
	    { NUCMX_GPIO(4,5), NUCMX_WUI(1,5,5) },
	    { NUCMX_GPIO(4,6), NUCMX_WUI(1,5,6) },
	    { NUCMX_GPIO(4,7), NUCMX_WUI(1,5,7) },
	  },  NUCMX_IRQ_WKINTE_1 },

	/* MIWU1 Group F */
	{ {	{ NUCMX_GPIO(5,0), NUCMX_WUI(1,6,0) },
		{ NUCMX_GPIO(5,1), NUCMX_WUI(1,6,1) },
		{ NUCMX_GPIO(5,2), NUCMX_WUI(1,6,2) },
		{ NUCMX_GPIO(5,3), NUCMX_WUI(1,6,3) },
		{ NUCMX_GPIO(5,4), NUCMX_WUI(1,6,4) },
		{ NUCMX_GPIO(5,5), NUCMX_WUI(1,6,5) },
		{ NUCMX_GPIO(5,6), NUCMX_WUI(1,6,6) },
		{ NUCMX_GPIO(5,7), NUCMX_WUI(1,6,7) },
	  },  NUCMX_IRQ_WKINTF_1 },
	/* MIWU1 Group G */
	{ {	{ NUCMX_GPIO(6,0), NUCMX_WUI(1,7,0) },
		{ NUCMX_GPIO(6,1), NUCMX_WUI(1,7,1) },
		{ NUCMX_GPIO(6,2), NUCMX_WUI(1,7,2) },
		{ NUCMX_GPIO(6,3), NUCMX_WUI(1,7,3) },
		{ NUCMX_GPIO(6,4), NUCMX_WUI(1,7,4) },
		{ NUCMX_GPIO(6,5), NUCMX_WUI(1,7,5) },
		{ NUCMX_GPIO(6,6), NUCMX_WUI(1,7,6) },
		{ NUCMX_GPIO(7,1), NUCMX_WUI(1,7,7) },
	  },  NUCMX_IRQ_WKINTG_1 },
	/* MIWU1 Group H */
	{ {	{ NUCMX_GPIO(7,0), NUCMX_WUI(1,8,0) },
		{ NUCMX_GPIO(6,7), NUCMX_WUI(1,8,1) },
		{ NUCMX_GPIO(7,2), NUCMX_WUI(1,8,2) },
		{ NUCMX_GPIO(7,3), NUCMX_WUI(1,8,3) },
		{ NUCMX_GPIO(7,4), NUCMX_WUI(1,8,4) },
		{ NUCMX_GPIO(7,5), NUCMX_WUI(1,8,5) },
		{ NUCMX_GPIO(7,6), NUCMX_WUI(1,8,6) },
		{ NUCMX_GPIO_NONE, NUCMX_WUI(1,8,7) },
	  },  NUCMX_IRQ_WKINTH_1 },
};

struct gpio_alt_map {
	uint8_t gpio_port;
	uint8_t gpio_mask;
	uint8_t alt_group;
	uint8_t alt_mask;
};

const struct gpio_alt_map gpio_alt_table[] = {
	/* I2C Module */
#if I2C0_BUS0
	{ NUCMX_GPIO(B,4),  NUCMX_ALT(2,I2C0_0_SL)	},	/* SMB0SDA  */
	{ NUCMX_GPIO(B,5),  NUCMX_ALT(2,I2C0_0_SL)	},	/* SMB0SCL  */
#else
	{ NUCMX_GPIO(B,2),  NUCMX_ALT(2,I2C0_1_SL)	},	/* SMB0SDA  */
	{ NUCMX_GPIO(B,3),  NUCMX_ALT(2,I2C0_1_SL)	},	/* SMB0SCL  */
#endif
	{ NUCMX_GPIO(8,7),  NUCMX_ALT(2,I2C1_0_SL)	},	/* SMB1SDA  */
	{ NUCMX_GPIO(9,0),  NUCMX_ALT(2,I2C1_0_SL)	},	/* SMB1SCL  */
	{ NUCMX_GPIO(9,1),  NUCMX_ALT(2,I2C2_0_SL)	},	/* SMB2SDA  */
	{ NUCMX_GPIO(9,2),  NUCMX_ALT(2,I2C2_0_SL)	},	/* SMB2SCL  */
	{ NUCMX_GPIO(D,0),  NUCMX_ALT(2,I2C3_0_SL)	},	/* SMB3SDA  */
	{ NUCMX_GPIO(D,1),  NUCMX_ALT(2,I2C3_0_SL)	},	/* SMB3SCL  */
	/* ADC Module */
	{ NUCMX_GPIO(4,5),  NUCMX_ALT(6,ADC0_SL)	},	/* ADC0 	*/
	{ NUCMX_GPIO(4,4),  NUCMX_ALT(6,ADC1_SL)	},	/* ADC1 	*/
	{ NUCMX_GPIO(4,3),  NUCMX_ALT(6,ADC2_SL)	},	/* ADC2  	*/
	{ NUCMX_GPIO(4,2),  NUCMX_ALT(6,ADC3_SL)	},	/* ADC3 	*/
	{ NUCMX_GPIO(4,1),  NUCMX_ALT(6,ADC4_SL)	},	/* ADC4 	*/
	/* UART Module */
	{ NUCMX_GPIO(1,0),  NUCMX_ALT(9,NO_KSO08_SL)},	/* CR_SIN	*/ /*KSO09/GPIO10*/
	{ NUCMX_GPIO(1,1),  NUCMX_ALT(9,NO_KSO09_SL)},	/* CR_SOUT	*/ /*KSO10/GPIO11*/
	/* SPI Module */
	{ NUCMX_GPIO(9,5),  NUCMX_ALT(0,SPIP_SL	)	},	/* SPIP_MISO  */
	{ NUCMX_GPIO(A,5),  NUCMX_ALT(0,SPIP_SL	)	},	/* SPIP_CS1   */
	{ NUCMX_GPIO(A,3),  NUCMX_ALT(0,SPIP_SL	)	},	/* SPIP_MOSI  */
	{ NUCMX_GPIO(A,1),  NUCMX_ALT(0,SPIP_SL	)	},	/* SPIP_SCLK  */
	/* PWM Module */
	{ NUCMX_GPIO(C,3),  NUCMX_ALT(4,PWM0_SL	)	},	/* PWM0  */
	{ NUCMX_GPIO(C,2),  NUCMX_ALT(4,PWM1_SL	)	},	/* PWM1  */
	{ NUCMX_GPIO(C,4),  NUCMX_ALT(4,PWM2_SL	)	},	/* PWM2  */
	{ NUCMX_GPIO(8,0),  NUCMX_ALT(4,PWM3_SL	)	},	/* PWM3  */
	{ NUCMX_GPIO(B,6),  NUCMX_ALT(4,PWM4_SL	)	},	/* PWM4  */
	{ NUCMX_GPIO(B,7),  NUCMX_ALT(4,PWM5_SL	)	},	/* PWM5  */
	{ NUCMX_GPIO(C,0),  NUCMX_ALT(4,PWM6_SL	)	},	/* PWM6  */
	{ NUCMX_GPIO(6,0),  NUCMX_ALT(4,PWM7_SL	)	},	/* PWM7  */
	/* MFT Module */
#if TACH_SEL1
	{ NUCMX_GPIO(4,0),  NUCMX_ALT(3,TA1_TACH1_SL1)},/* TA1_TACH1 */
	{ NUCMX_GPIO(A,4),  NUCMX_ALT(3,TB1_TACH2_SL1)},/* TB1_TACH2 */
#else
	{ NUCMX_GPIO(9,3),  NUCMX_ALT(C,TA1_TACH1_SL2)},/* TA1_TACH1 */
	{ NUCMX_GPIO(D,3),  NUCMX_ALT(C,TB1_TACH2_SL2)},/* TB1_TACH2 */
#endif
	/* JTAG Module */
#if !(JTAG1)
	{ NUCMX_GPIO(2,1),  NUCMX_ALT(5,NJEN0_EN)	},	/* TCLK */
	{ NUCMX_GPIO(1,7),  NUCMX_ALT(5,NJEN0_EN)	},	/* TDI  */
	{ NUCMX_GPIO(1,6),  NUCMX_ALT(5,NJEN0_EN)	},	/* TDO  */
	{ NUCMX_GPIO(2,0),  NUCMX_ALT(5,NJEN0_EN)	},	/* TMS  */
#else
	{ NUCMX_GPIO(D,5),  NUCMX_ALT(5,NJEN1_EN)	},	/* TCLK */
	{ NUCMX_GPIO(E,2),  NUCMX_ALT(5,NJEN1_EN)	},	/* TDI  */
	{ NUCMX_GPIO(D,4),  NUCMX_ALT(5,NJEN1_EN)	},	/* TDO  */
	{ NUCMX_GPIO(E,5),  NUCMX_ALT(5,NJEN1_EN)	},	/* TMS  */
#endif
	/* 01 for PWRGD_OUT*/
};

/*****************************************************************************/
/* Internal functions */
const struct gpio_wui_map* gpio_find_wui_from_io(uint8_t port, uint8_t mask)
{
	int i,j;
	for (i = 0; i < ARRAY_SIZE(gpio_wui_table); i++) {
		const struct gpio_wui_map *map = gpio_wui_table[i].wui_map;
		for(j=0; j<8; j++, map++){
			if (map->gpio_port == port && map->gpio_mask == mask){
				return map;
			}
		}
	}
	return NULL;
}

int gpio_find_irq_from_io(uint8_t port, uint8_t mask)
{
	int i,j;
	for (i = 0; i < ARRAY_SIZE(gpio_wui_table); i++) {
		const struct gpio_wui_map *map = gpio_wui_table[i].wui_map;
		for(j=0; j<8; j++, map++){
			if (map->gpio_port == port && map->gpio_mask == mask){
				return gpio_wui_table[i].irq;
			}
		}
	}
	return -1;
}

int gpio_alt_sel(uint8_t port, uint8_t mask, uint8_t func)
{
	int i;
	const struct gpio_alt_map *map = gpio_alt_table;
	for (i = 0; i < ARRAY_SIZE(gpio_alt_table); i++, map++) {
		if (map->gpio_port == port &&
				(map->gpio_mask == mask)){
			/* Enable alternative function if func >=0 */
			if(func <= 0) /* GPIO functionality */
				NUCMX_DEVALT(map->alt_group) &= ~(map->alt_mask);
			else
				NUCMX_DEVALT(map->alt_group) |= (map->alt_mask);
			return 1;
		}
	}
	return -1;
}

void gpio_execute_isr(uint8_t port, uint8_t mask)
{
	int i;
	const struct gpio_info *g = gpio_list;
	/* Find GPIOs and execute interrupt service routine */
	for (i = 0; i < GPIO_COUNT; i++, g++) {
		if (port == g->port && mask == g->mask && g->irq_handler) {
			g->irq_handler(i);
			return;
		}
	}
}

/* Set interrupt type for GPIO input */
void gpio_interrupt_type_sel(uint8_t port, uint8_t mask, uint32_t flags)
{
	const struct gpio_wui_map *map = gpio_find_wui_from_io(port, mask);
	uint8_t table, group, pmask;

	if(map == NULL)
		return;

	table = map->wui_table;
	group = map->wui_group;
	pmask = map->wui_mask;

	/* Handle interrupt for level trigger */
	if ((flags & GPIO_INT_F_HIGH) ||
			(flags & GPIO_INT_F_LOW)){
		/* Set detection mode to level */
		NUCMX_WKMOD(table, group) |= pmask;
		/* Handle interrupting on level high */
		if(flags & GPIO_INT_F_HIGH){
			NUCMX_WKEDG(table,group) &= ~pmask;
		}
		/* Handle interrupting on level low */
		else if(flags & GPIO_INT_F_LOW){
			NUCMX_WKEDG(table,group) |= pmask;
		}
		/* Enable wake-up input sources */
		NUCMX_WKEN(table,group) |= pmask;
	}
	/* Handle interrupt for edge trigger */
	else if((flags & GPIO_INT_F_RISING) ||
			(flags & GPIO_INT_F_FALLING)){
		/* Set detection mode to edge */
		NUCMX_WKMOD(table,group) &= ~pmask;
		/* Handle interrupting on both edges */
		if ((flags & GPIO_INT_F_RISING) &&
				(flags & GPIO_INT_F_FALLING)){
			/* Enable any edge */
			NUCMX_WKAEDG(table,group) |= pmask;
		}
		/* Handle interrupting on rising edge */
		else if(flags & GPIO_INT_F_RISING){
			/* Disable any edge */
			NUCMX_WKAEDG(table,group) &= ~pmask;
			NUCMX_WKEDG(table,group) &= ~pmask;
		}
		/* Handle interrupting on falling edge */
		else if(flags & GPIO_INT_F_FALLING){
			/* Disable any edge */
			NUCMX_WKAEDG(table,group) &= ~pmask;
			NUCMX_WKEDG(table,group) |= pmask;
		}
		/* Enable wake-up input sources */
		NUCMX_WKEN(table,group) |= pmask;
	}
	else{
		/* Disable wake-up input sources */
		NUCMX_WKEN(table,group) &= ~pmask;
	}

	/* No support analog mode */
}

/*****************************************************************************/
/* IC specific low-level driver */

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	/* Enable alternative pins by func*/
	int pin;
	uint8_t pmask;
	/* check each bit from mask  */
	for (pin = 0; pin < 8; pin++) {
		pmask = (mask & (1 << pin));
		if (pmask){
			gpio_alt_sel(port, pmask, func);
		}
	}
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	return (NUCMX_PDIN(gpio_list[signal].port) &
			gpio_list[signal].mask) ? 1: 0;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	if (value)
		NUCMX_PDOUT(gpio_list[signal].port) |=
				 gpio_list[signal].mask;
	else
		NUCMX_PDOUT(gpio_list[signal].port) &=
				~gpio_list[signal].mask;
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	/*
	 * Select open drain first, so that we don't glitch the signal
	 * when changing the line to an output. 0:push-pull 1:open-drain
	 */
	if (flags & GPIO_OPEN_DRAIN)
		NUCMX_PTYPE(port) |= mask;
	else
		NUCMX_PTYPE(port) &= ~mask;

	/* Select direction of GPIO 0:input 1:output */
	if (flags & GPIO_OUTPUT)
		NUCMX_PDIR(port) |= mask;
	else
		NUCMX_PDIR(port) &= ~mask;

	/* Select pull-up/down of GPIO 0:pull-up 1:pull-down */
	if (flags & GPIO_PULL_UP) {
		NUCMX_PPUD(port)  &= ~mask;
		NUCMX_PPULL(port) |= mask;		/* enable pull down/up */
	} else if (flags & GPIO_PULL_DOWN) {
		NUCMX_PPUD(port)  |= mask;
		NUCMX_PPULL(port) |= mask;		/* enable pull down/up */
	} else {
		/* No pull up/down */
		NUCMX_PPULL(port) &= ~mask;		/* disable pull down/up */
	}

	/* Set up interrupt type */
	if(flags & GPIO_INPUT)
		gpio_interrupt_type_sel(port, mask, flags);

	/* Set level 0:low 1:high*/
	if (flags & GPIO_HIGH)
		NUCMX_PDOUT(port) |= mask;
	else if (flags & GPIO_LOW)
		NUCMX_PDOUT(port) &= ~mask;

}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;
	int irq = gpio_find_irq_from_io(g->port, g->mask);

	/* Fail if no interrupt handler */
	if (irq <0 )
		return EC_ERROR_UNKNOWN;

	task_enable_irq(irq);
	return EC_SUCCESS;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;
	int irq = gpio_find_irq_from_io(g->port, g->mask);

	/* Fail if no interrupt handler */
	if (irq <0 )
		return EC_ERROR_UNKNOWN;

	task_disable_irq(irq);
	return EC_SUCCESS;
}

int gpio_is_reboot_warm(void)
{
	/*  Check for debugger or watch-dog Warm reset */
	if (IS_BIT_SET(NUCMX_RSTCTL,NUCMX_RSTCTL_DBGRST_STS)  /* debugger warm reset */
#if 0
		|| (IS_BIT_SET(NUCMX_T0CSR,NUCMX_T0CSR_WDRST_STS)&&(IS_BIT_SET(NUCMX_TWCFG,NUCMX_TWCFG_WDRST_MODE))))      		/* watch-dog warm reset */
#else
		)
#endif
		return 1;
	else
		return 0;
}

void gpio_pre_init(void)
{
	const struct gpio_info *g = gpio_list;
	int flags;
	int i,j;
	int is_warm = gpio_is_reboot_warm();

	uint32_t 	ksi_mask = (~((1<<KEYBOARD_ROWS)-1)) & KB_ROW_MASK;
	uint32_t 	ks0_mask = (~((1<<KEYBOARD_COLS)-1)) & KB_COL_MASK;

	/* Set necessary pin mux first */
	/* Pin_Mux for KSO0-17 & KSI0-7 */
	NUCMX_DEVALT(ALT_GROUP_7)  = (uint8_t)(ksi_mask);
	NUCMX_DEVALT(ALT_GROUP_8)  = (uint8_t)(ks0_mask);
	NUCMX_DEVALT(ALT_GROUP_9)  = (uint8_t)(ks0_mask >> 8);
	NUCMX_DEVALT(ALT_GROUP_A) |= (uint8_t)(ks0_mask >> 16);

	/* Pin_Mux for FIU/SPI (set to GPIO) */
	SET_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_GPIO_NO_SPIP);
	SET_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_NO_F_SPI);

	/* Clear all pending bits of GPIOS*/
	for (i = 0; i < 2; i++)
		for (j = 0; j < 8; j++)
				NUCMX_WKPCL(i,j) = 0xFF;


	/* No support enable clock for the GPIO port in run and sleep. */
	/* Set flag for each GPIO pin in gpio_list */
	for (i = 0; i < GPIO_COUNT; i++, g++) {
		flags = g->flags;

		if (flags & GPIO_DEFAULT)
			continue;

		/*
		 * If this is a warm reboot, don't set the output levels or
		 * we'll shut off the AP.
		 */
		if (is_warm)
			flags &= ~(GPIO_LOW | GPIO_HIGH);

		/* Set up GPIO based on flags */
		gpio_set_flags_by_mask(g->port, g->mask, flags);
	}
}

/* List of GPIO IRQs to enable. Don't automatically enable interrupts for
 * the keyboard input GPIO bank - that's handled separately. Of course the
 * bank is different for different systems. */
static void gpio_init(void)
{
	int i;
	/* Enable IRQs now that pins are set up */
	for (i = 0; i < ARRAY_SIZE(gpio_wui_table); i++)
		task_enable_irq(gpio_wui_table[i].irq);

}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Interrupt handlers */

/**
 * Handle a GPIO interrupt.
 *
 * @param int_no	Interrupt number for GPIO
 */

static void gpio_interrupt(int int_no)
{
#if DEBUG_GPIO
	static uint8_t i, pin, wui_mask;
#else
	uint8_t i, pin, wui_mask;
#endif
	for (i = 0; i < ARRAY_SIZE(gpio_wui_table); i++) {
		/* If interrupt number is the same */
		if(gpio_wui_table[i].irq == int_no){

			/* Mapping relationship between WUI and GPIO */
			const struct gpio_wui_map *map = gpio_wui_table[i].wui_map;
			/* Get pending mask */
			wui_mask = NUCMX_WKPND(map->wui_table , map->wui_group);

			/* If pending bits is not zero */
			if(wui_mask){
				/* Clear pending bits of WUI */
				NUCMX_WKPCL(map->wui_table , map->wui_group) = wui_mask;

				for(pin = 0; pin< 8; pin++, map++){
					/* If pending bit is high, execute ISR */
					if(wui_mask & (1<<pin))
						gpio_execute_isr(map->gpio_port,map->gpio_mask);
				}
			}
		}
	}
}

/**
 * Handlers for each GPIO port.  These read and clear the interrupt bits for
 * the port, then call the master handler above.
 */

#define GPIO_IRQ_FUNC(_irq_func, int_no)	\
void _irq_func(void)						\
{											\
	gpio_interrupt(int_no);					\
}

GPIO_IRQ_FUNC(__gpio_wk0ad_interrupt	,NUCMX_IRQ_MTC_WKINTAD_0);
GPIO_IRQ_FUNC(__gpio_wk0b_interrupt		,NUCMX_IRQ_TWD_WKINTB_0	);
GPIO_IRQ_FUNC(__gpio_wk0c_interrupt		,NUCMX_IRQ_WKINTC_0		);
GPIO_IRQ_FUNC(__gpio_wk0efgh_interrupt	,NUCMX_IRQ_WKINTEFGH_0	);
GPIO_IRQ_FUNC(__gpio_wk1a_interrupt		,NUCMX_IRQ_WKINTA_1		);
GPIO_IRQ_FUNC(__gpio_wk1b_interrupt		,NUCMX_IRQ_WKINTB_1		);
GPIO_IRQ_FUNC(__gpio_wk1d_interrupt		,NUCMX_IRQ_WKINTD_1		);
GPIO_IRQ_FUNC(__gpio_wk1e_interrupt		,NUCMX_IRQ_WKINTE_1		);
GPIO_IRQ_FUNC(__gpio_wk1f_interrupt		,NUCMX_IRQ_WKINTF_1		);
GPIO_IRQ_FUNC(__gpio_wk1g_interrupt		,NUCMX_IRQ_WKINTG_1		);
GPIO_IRQ_FUNC(__gpio_wk1h_interrupt		,NUCMX_IRQ_WKINTH_1		);

DECLARE_IRQ(NUCMX_IRQ_MTC_WKINTAD_0, 	__gpio_wk0ad_interrupt, 	1);
DECLARE_IRQ(NUCMX_IRQ_TWD_WKINTB_0, 	__gpio_wk0b_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTC_0, 		__gpio_wk0c_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTEFGH_0, 		__gpio_wk0efgh_interrupt, 	1);
DECLARE_IRQ(NUCMX_IRQ_WKINTA_1, 		__gpio_wk1a_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTB_1, 		__gpio_wk1b_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTD_1, 		__gpio_wk1d_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTE_1, 		__gpio_wk1e_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTF_1, 		__gpio_wk1f_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTG_1, 		__gpio_wk1g_interrupt, 		1);
DECLARE_IRQ(NUCMX_IRQ_WKINTH_1, 		__gpio_wk1h_interrupt, 		1);


#undef GPIO_IRQ_FUNC

