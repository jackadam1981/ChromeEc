#include "battery.h"
#include "battery_smart.h"
#include "button.h"
#include "board.h"
#include "charge_ramp.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "driver/charger/isl9241.h"
#include "power.h"
#include "registers.h"
#include "switch.h"


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
	int voltage=0;
	int current=0;
	int cycle_count=0;


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

	i2c_read16(I2C_PORT_CHARGER,
			  ISL9241_ADDR_FLAGS,
			  ISL9241_REG_CHG_CURRENT_LIMIT, &current);
	r1->ChargingCurrent = current;

	i2c_read16(I2C_PORT_CHARGER,
			  ISL9241_ADDR_FLAGS,
			  ISL9241_REG_MAX_SYSTEM_VOLTAGE, &voltage);
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
