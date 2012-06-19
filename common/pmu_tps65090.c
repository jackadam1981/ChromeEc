/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TPS65090 PMU driver.
 */

#include "smart_battery.h"
#include "board.h"
#include "chipset.h"
#include "console.h"
#include "common.h"
#include "gpio.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "smart_battery.h"
#include "system.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_CHARGER, outstr)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

#define TPS65090_I2C_ADDR 0x90

#define IRQ1_REG 0x00
#define IRQ2_REG 0x01
#define IRQ1MASK 0x02
#define IRQ2MASK 0x03
#define CG_CTRL0 0x04
#define CG_CTRL1 0x05
#define CG_CTRL2 0x06
#define CG_CTRL3 0x07
#define CG_CTRL4 0x08
#define CG_CTRL5 0x09
#define CG_STATUS1 0x0a
#define CG_STATUS2 0x0b

/* IRQ events */
#define EVENT_VACG    (1 <<  1)
#define EVENT_VBATG   (1 <<  3)

/* Charging and discharging alarms */
#define ALARM_DISCHARGING (ALARM_TERMINATE_DISCHARGE | ALARM_OVER_TEMP)
#define ALARM_CHARGING (ALARM_TERMINATE_CHARGE | \
		ALARM_OVER_CHARGED | \
		ALARM_OVER_TEMP)


#ifdef CONFIG_TASK_PMU_TPS65090_CHARGER
/* Time delay in usec for idle, charging and discharging.
 * Defined in battery charging flow.
 */
#define T1_USEC 5000000
#define T2_USEC 10000000
#define T3_USEC 10000000

/* Non-SBS charging states */
enum charging_state {
	ST_NONE = 0,
	ST_IDLE,
	ST_PRE_CHARGING,
	ST_CHARGING,
	ST_DISCHARGING,
};

static const char * const state_list[] = {
	"none",
	"idle",
	"pre-charging",
	"charging",
	"discharging"
};
#endif /* CONFIG_TASK_PMU_TPS65090_CHARGER */

/* Read/write tps65090 register */
static inline int pmu_read(int reg, int *value)
{
	return i2c_read8(I2C_PORT_CHARGER, TPS65090_I2C_ADDR, reg, value);
}

static inline int pmu_write(int reg, int value)
{
	return i2c_write8(I2C_PORT_CHARGER, TPS65090_I2C_ADDR, reg, value);
}

/* Clear tps65090 irq */
static inline int pmu_clear_irq(void)
{
	return pmu_write(IRQ1_REG, 0);
}

/* Read all tps65090 interrupt events */
static int pmu_get_event(int *event)
{
	int rv;
	int irq1, irq2;

	pmu_clear_irq();

	rv = pmu_read(IRQ1_REG, &irq1);
	if (rv)
		return rv;
	rv = pmu_read(IRQ2_REG, &irq2);
	if (rv)
		return rv;

	*event = irq1 | (irq2 << 8);

	return EC_SUCCESS;
}

void pmu_init(void)
{
	/* Init configuration
	 *   Fast charge timer    : 2 hours
	 *   Charger              : disable
	 *   External pin control : enable
	 *
	 * TODO: move settings to battery pack specific init
	 */
	pmu_write(CG_CTRL0, 2);

	/* Enable interrupt mask */
	pmu_write(IRQ1MASK, 0xff);
	pmu_write(IRQ2MASK, 0xff);

	/* Limit full charge current to 50%
	 * TODO: remove this temporary hack.
	 */
	pmu_write(CG_CTRL3, 0xbb);
}

#ifdef CONFIG_TASK_PMU_TPS65090_CHARGER

/* An ES1 workaround to get AC state
 * The dev boards without rework can not get AC state directly from gpio.
 * And the tps65090 VACG/VBATG does not reflect AC and battery state correctly.
 * This workaround uses tps65090 irq event and battery i2c to get correct AC
 * state.
 */
static int get_ac(void)
{
	static int prev_event;
	int event;
	int alarm;
	int chg_ac_good = 0, chg_bat_good = 0;

	if (pmu_get_event(&event))
		return 0;

	if (prev_event != event) {
		CPRINTF("pmu event: %016b\n", event);
		prev_event = event;
	}

	if (event & EVENT_VACG)
		chg_ac_good = 1;
	if (event & EVENT_VBATG)
		chg_bat_good = 1;

	/* TODO: Need to clarify tps65090 charger behavior
	 *   VACG always on with either AC or BAT
	 *   VBAT is on only when both AC and BAT are on
	 */

	if (chg_ac_good && chg_bat_good)
		return 1;

	if (battery_status(&alarm) == 0)
		return 0;

	return 1;
}

/* Convert SBS battery temperature (deci K) to Celcius */
static int battery_temperature_celcius(int t)
{
	return (t - 2731) / 10;
}

static int battery_start_charging_range(int t)
{
	t = battery_temperature_celcius(t);
	return (t > 5 && t < 40);
}

static int battery_charging_range(int t)
{
	t = battery_temperature_celcius(t);
	return (t > 5 && t < 60);
}

static int battery_discharging_range(int t)
{
	t = battery_temperature_celcius(t);
	return (t < 70);
}

static int wait_t1_idle(void)
{
	usleep(T1_USEC);
	return ST_IDLE;
}

static int wait_t2_charging(void)
{
	usleep(T2_USEC);
	return ST_CHARGING;
}

static int wait_t3_discharging(void)
{
	usleep(T3_USEC);
	return ST_DISCHARGING;
}

static int system_off(void)
{
	CPUTS("[pmu] turn system off\n");
	chipset_exit_hard_off();

	/* TODO: After have impl in chipset_exit_hard_off(),
	 * remove these gpio hack
	 */
	gpio_set_level(GPIO_EN_PP3300, 0);
	gpio_set_level(GPIO_EN_PP1350, 0);
	gpio_set_level(GPIO_PMIC_PWRON_L, 1);
	gpio_set_level(GPIO_EN_PP5000, 0);

	return ST_IDLE;
}

static int notify_battery_low(void)
{
	static timestamp_t last_notify_time;
	timestamp_t now = get_time();
	if (now.val - last_notify_time.val > 60000000) {
		CPUTS("[pmu] notify battery low (< 10%)\n");
		last_notify_time = now;
	}
	return ST_DISCHARGING;
}

static int calc_next_state(int state)
{
	int d;

	switch (state) {
	case ST_IDLE:
		/* Turn off charger */
		if (gpio_get_level(GPIO_CHARGER_EN))
			gpio_set_level(GPIO_CHARGER_EN, 0);

		/* Check AC */
		if (!get_ac())
			return ST_DISCHARGING;

		/* Enable charging if battery doesn't respond */
		if (battery_temperature(&d))
			return ST_PRE_CHARGING;

		if (!battery_start_charging_range(d))
			return wait_t1_idle();

		if (battery_status(&d) || (d & ALARM_CHARGING)) {
			if (!(d & ALARM_TERMINATE_CHARGE))
				CPRINTF("[pmu] idle %016b\n", d);
			return wait_t1_idle();
		}

		gpio_set_level(GPIO_CHARGER_EN, 1);
		return ST_CHARGING;

	case ST_PRE_CHARGING:
		if (!get_ac())
			return wait_t1_idle();

		if (!gpio_get_level(GPIO_CHARGER_EN)) {
			CPUTS("[pmu] try charging\n");
			gpio_set_level(GPIO_CHARGER_EN, 1);
		}

		/* If the battery goes online after enable the charger,
		 * go into charging state.
		 */
		if (battery_temperature(&d) == EC_SUCCESS)
			return ST_CHARGING;

		wait_t1_idle();
		return ST_PRE_CHARGING;

	case ST_CHARGING:
		if (!get_ac())
			break;
		if (battery_temperature(&d) || !battery_charging_range(d)) {
			CPUTS("[pmu] charging: battery hot\n");
			break;
		}
		if (battery_status(&d) || (d & ALARM_CHARGING)) {
			CPUTS("[pmu] charging: battery alarm\n");
			break;
		}
		if (pmu_read(CG_STATUS1, &d) || (d & 3)) {
			CPUTS("[pmu] charging: charger alarm\n");
			break;
		}
		return wait_t2_charging();

	case ST_DISCHARGING:

		if (get_ac())
			return ST_IDLE;

		/* Check battery discharging temperature range */
		if (battery_temperature(&d) == 0) {
			if (!battery_discharging_range(d)) {
				CPUTS("[pmu] discharging: battery hot\n");
				return system_off();
			}
		}
		/* Check discharging alarm */
		if (battery_status(&d) || (d & ALARM_DISCHARGING)) {
			CPRINTF("[pmu] discharging: battery alarm %016b\n", d);
			return system_off();
		}
		/* Check remaining charge % */
		if (battery_state_of_charge(&d) == 0 && d < 10)
			return notify_battery_low();

		return wait_t3_discharging();
	}

	return ST_IDLE;
}

void pmu_charger_task(void)
{
	int state = ST_IDLE;
	int next_state;

	pmu_init();

	/* TODO: Clarify unexpected tps65090 behavior
	 *
	 * Start internal charger:
	 *   trigger physical AC change, or toggle CHARGER_EN pin
	 */
	if (gpio_get_level(GPIO_CHARGER_EN) == 0) {
		gpio_set_level(GPIO_CHARGER_EN, 1);
		usleep(1000);
	}
	gpio_set_level(GPIO_CHARGER_EN, 0);

	while (1) {
		next_state = calc_next_state(state);
		if (next_state != state) {
			CPRINTF("[batt] state %s -> %s\n",
				state_list[state],
				state_list[next_state]);
			state = next_state;
		}
	}
}

#endif /* CONFIG_TASK_PMU_TPS65090_CHARGER */

