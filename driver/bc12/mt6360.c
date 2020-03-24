#include "charger.h"
#include "charge_manager.h"
#include "console.h"
#include "driver/bc12/mt6360.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "usb_charge.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) \
	cprints(CC_CHARGER, "%s " format, "MT6360", ## args)

static enum ec_error_list mt6360_read8(int reg, int *val)
{
	return i2c_read8(mt6360_config.i2c_port, mt6360_config.i2c_addr_flags,
			reg, val);
}

static enum ec_error_list mt6360_write8(int reg, int val)
{
	return i2c_write8(mt6360_config.i2c_port, mt6360_config.i2c_addr_flags,
			reg, val);
}

static int mt6360_update_bits(int reg, int mask, int val)
{
	int rv;
	int reg_val = 0;

	rv = mt6360_read8(reg, &reg_val);
	if (rv)
		return rv;
	reg_val &= ~mask;
	reg_val |= (mask & val);
	rv = mt6360_write8(reg, reg_val);
	return rv;
}

static inline int mt6360_set_bit(int reg, int mask)
{
	return mt6360_update_bits(reg, mask, mask);
}

static inline int mt6360_clr_bit(int reg, int mask)
{
	return mt6360_update_bits(reg, mask, 0x00);
}


static int mt6360_get_bc12_device_type(void)
{
	int reg;

	if (mt6360_read8(MT6360_REG_USB_STATUS_1, &reg))
		return CHARGE_SUPPLIER_NONE;

	CPRINTS("%s: %x", __func__, reg);
	switch (reg & MT6360_MASK_USB_STATUS) {
	case MT6360_MASK_SDP:
		return CHARGE_SUPPLIER_BC12_SDP;
	case MT6360_MASK_CDP:
		return CHARGE_SUPPLIER_BC12_CDP;
	case MT6360_MASK_DCP:
		return CHARGE_SUPPLIER_BC12_DCP;
	default:
		return CHARGE_SUPPLIER_NONE;
	}
}

static int mt6360_get_bc12_ilim(int charge_supplier)
{
	/* TODO */
	switch (charge_supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
		if (IS_ENABLED(CONFIG_CHARGE_RAMP_SW) ||
		    IS_ENABLED(CONFIG_CHARGE_RAMP_HW))
			/* A conservative value to prevent a bad charger. */
			return 2165;
		/* fallback */
	case CHARGE_SUPPLIER_BC12_CDP:
		return 1500;
	case CHARGE_SUPPLIER_BC12_SDP:
	default:
		return USB_CHARGER_MIN_CURR_MA;
	}
}

static int rt946x_enable_bc12_detection(int en)
{
	int rv;

	if (en) {
#ifdef CONFIG_MT6360_BC12_DETECT_GPIO
		gpio_set_level(CONFIG_MT6360_BC12_DETECT_GPIO, 1);
#endif
		return mt6360_set_bit(MT6360_REG_DEVICE_TYPE,
				      MT6360_MASK_USBCHGEN);
	}

	rv = mt6360_clr_bit(MT6360_REG_DEVICE_TYPE, MT6360_MASK_USBCHGEN);
#ifdef CONFIG_MT6360_BC12_DETECT_GPIO
	gpio_set_level(CONFIG_MT6360_BC12_DETECT_GPIO, 0);
#endif
	return rv;
}

int rt946x_toggle_bc12_detection(void)
{
	int rv;
	rv = rt946x_enable_bc12_detection(0);
	if (rv)
		return rv;
	/* mt6370 requires 40us delay to toggle RT946X_MASK_USBCHGEN */
	udelay(40);
	return rt946x_enable_bc12_detection(1);
}

static void rt946x_bc12_workaround(void)
{
	/*
	 * There is a parasitic capacitance on D+,
	 * which results in pulling D+ up too slow while detecting BC1.2.
	 * So we try to fix this in two steps:
	 * 1. Pull D+ up to a voltage under 0.6V
	 * 2. re-toggling and pull D+ up to 0.6V (again)
	 * and then detect the voltage of D-.
	 */
	rt946x_toggle_bc12_detection();
	msleep(10);
	rt946x_toggle_bc12_detection();
}
DECLARE_DEFERRED(rt946x_bc12_workaround);

int usb_charger_ramp_allowed(int supplier)
{
	return supplier == CHARGE_SUPPLIER_BC12_DCP;
}

int usb_charger_ramp_max(int supplier, int sup_curr)
{
	return mt6360_get_bc12_ilim(supplier);
}

void mt6360_usb_charger_task(void *u)
{
	struct charge_port_info chg;
	int bc12_type = CHARGE_SUPPLIER_NONE;
	int bc12_cnt = 0;
	const int max_bc12_cnt = 3;

	chg.voltage = USB_CHARGER_VOLTAGE_MV;
	while (1) {
		rt946x_enable_bc12_detection(1);

		/* TODO: change this to interrupt */
		usleep(300 * MSEC);

		/* TODO: apple charger detection 
		chg_type = mt6360_get_charger_type();
		*/
		bc12_type = mt6360_get_bc12_device_type();
		chg.current = mt6360_get_bc12_ilim(bc12_type);
		CPRINTS("BC12 type", bc12_type);

		if (bc12_type == CHARGE_SUPPLIER_NONE) {
			CPRINTS("VBUS detached");
			bc12_cnt = 0;
			charge_manager_update_charge(bc12_type, 0, NULL);
		} else if (bc12_type == CHARGE_SUPPLIER_BC12_SDP &&
		    ++bc12_cnt < max_bc12_cnt) {
			/*
			 * defer the workaround and awaiting for
			 * waken up by the interrupt.
			 */
			hook_call_deferred(
				&rt946x_bc12_workaround_data, 5);
		} else {
			charge_manager_update_charge(bc12_type, 0, &chg);
		}

		rt946x_enable_bc12_detection(0);
		task_wait_event(-1);
	}
}
