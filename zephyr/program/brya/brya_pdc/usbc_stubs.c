#include "common.h"
#include "usbc/pdc_power_mgnt.h"
#include "usb_mux.h"

uint8_t board_get_usb_pd_port_count(void)
{
        return CONFIG_USB_PD_PORT_MAX_COUNT;
}

void board_charging_enable(int port, int en)
{
}

void pd_request_vconn_swap(int port)
{
}

void pd_request_power_swap(int port)
{
}

void pd_set_new_power_request(int port)
{
}

int board_get_vbus_voltage(int port)
{
        return 0; //pdc_get_vbus_voltage(port);
}

int board_vbus_source_enabled(int port)
{
        return 0;
}

int board_set_active_charge_port(int port)
{
	return 0;
}
