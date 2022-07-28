#include <stdint.h>

#include "adc.h"
#include "dps.h"
#include "atomic.h"
#include "battery.h"
#include "console.h"
#include "charger.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "ec_commands.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_pd.h"
#include "util.h"
#include "usb_pe_sm.h"
#include "driver/charger/sm5803.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

static int fake_state_of_charge = -1;

static enum ec_status
host_command_get_chg_info(struct host_cmd_handler_args *args)
//static void host_command_get_chg_info(void)
{
	const struct ec_params_get_chg_info *p = args->params;
	struct ec_response_get_chg_info *r1 = args->response;
	struct batt_params batt_new = {0};
	int current=0;
	int cycle_count=0;
    int voltage=0;


	if(p->index !=0)
		return EC_RES_SUCCESS;

	/* RSOC */
	if (sb_read(SB_RELATIVE_STATE_OF_CHARGE, &batt_new.state_of_charge)
	    && fake_state_of_charge < 0)
		batt_new.flags |= BATT_FLAG_BAD_STATE_OF_CHARGE;
	r1->RSOC = batt_new.state_of_charge;

	/*Get battery current and voltage*/
	if (sb_read(SB_VOLTAGE, &batt_new.voltage))
		batt_new.flags |= BATT_FLAG_BAD_VOLTAGE;
	r1->charge_voltage = batt_new.voltage;

	if (sb_read(SB_CURRENT, &batt_new.current))
		batt_new.flags |= BATT_FLAG_BAD_CURRENT;
	else
		batt_new.current = (int16_t)batt_new.current;
	r1->charge_current = batt_new.current;

    /*i2c_read8(charge_manager_get_active_charge_port(), SM5803_ADDR_CHARGER_FLAGS,
            SM5803_REG_CHG_ILIM, &current);*/
    charger_get_input_current(charge_get_active_chg_chip(), &current);
	r1->ChargingCurrent = current;

	/*rv = i2c_read8(charge_manager_get_active_charge_port(), SM5803_ADDR_CHARGER_FLAGS, SM5803_REG_VBAT_FAST_MSB, &regval);
	v = regval << 3;
	rv |= i2c_read8(charge_manager_get_active_charge_port(), SM5803_REG_VBAT_FAST_LSB, &regval);
	v |= (regval & 0x3);*/
    charger_get_voltage(charge_get_active_chg_chip(), &voltage);
	r1->ChargingVoltage = voltage;

	sb_read(SB_REMAINING_CAPACITY, &batt_new.remaining_capacity);
	r1->remaining_capacity = batt_new.remaining_capacity;

	sb_read(SB_FULL_CHARGE_CAPACITY, &batt_new.full_capacity);
	r1->full_capacity = batt_new.full_capacity;

	sb_read(SB_CYCLE_COUNT, &cycle_count);
	r1->cycle_count = cycle_count;

	if (sb_read(SB_TEMPERATURE, &batt_new.temperature))
		batt_new.flags |= BATT_FLAG_BAD_TEMPERATURE;
	r1->temp = batt_new.temperature;

	args->response_size = sizeof(*r1);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_CHARGER_INFO, host_command_get_chg_info, EC_VER_MASK(0));
