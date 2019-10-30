#include "common.h"
#include "demo.h"
#include "gpio.h"
#include "hooks.h"
#include "lcd.h"
#include "registers.h"
#include "spi.h"
#include "i2c.h"
#include "system.h"

#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_COMM_CAP)

/* TODO: fill in correct source and sink capabilities */
const uint32_t pd_src_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);
const uint32_t pd_src_pdo_max[] = {
	PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_max_cnt = ARRAY_SIZE(pd_src_pdo_max);

const uint32_t pd_snk_pdo[] = {
	PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
	PDO_BATT(4750, 21000, 15000),
	PDO_VAR(4750, 21000, 3000),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

int pd_check_data_swap(int port, int data_role)
{
	/*
	 * Allow data swap if we are a UFP, otherwise don't allow.
	 *
	 * When we are still in the Read-Only firmware, avoid swapping roles
	 * so we don't jump in RW as a SNK/DFP and potentially confuse the
	 * power supply by sending a soft-reset with wrong data role.
	 */
	return (data_role == PD_ROLE_UFP) &&
	       (system_get_image_copy() != SYSTEM_IMAGE_RO) ? 1 : 0;
}
