
#ifndef __MOCK_USB_TYPEC_DRP_ACC_TRYSRC_MOCK_H
#define __MOCK_USB_TYPEC_DRP_ACC_TRYSRC_MOCK_H

#include "usb_pd.h"

#ifdef __cplusplus
extern "C" {
#endif


void pd_dpm_request(int port, enum pd_dpm_request req);


#ifdef __cplusplus
}
#endif



#endif /* _MOCK_USB_TYPEC_DRP_ACC_TRYSRC_MOCK_H */