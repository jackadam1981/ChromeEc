#include "driver/charger/sm5803.h"
#include "driver/tcpm/tcpci.h"

__override bool pd_check_vbus_level(int port, enum vbus_level level)
{
	return sm5803_check_vbus_level(port, level);
}
