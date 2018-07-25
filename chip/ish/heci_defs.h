#ifndef HECI_DEFS_H_
#define HECI_DEFS_H_

#include "heci.h"

void heci_init(void);
uint32_t heci_send_flow_control (uint32_t conn_id);

#endif /* HECI_DEFS_H_ */
