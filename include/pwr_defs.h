#ifndef __CROS_EC_PWR_DEFS_H
#define __CROS_EC_PWR_DEFS_H

#include <system.h>

struct pwr_con_t {
	uint16_t volts;
	uint16_t milli_amps;
};

inline int pwr_con_to_milliwats(struct pwr_con_t *pwr)
{
	return (pwr->volts * pwr->milli_amps);
}

#endif
