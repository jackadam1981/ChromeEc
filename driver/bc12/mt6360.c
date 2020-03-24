#include "charge_manager.h"
#include "usb_charge.h"

static int mt6360_get_bc12_ilim(int charge_supplier)
{
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
}
