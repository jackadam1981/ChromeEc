#ifndef __HECI_DEFS_H
#define __HECI_DEFS_H

#include "heci.h"

void heci_init(void);
uint32_t heci_send_flow_control (uint32_t conn_id);

#endif /* __HECI_DEFS_H */
