/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_TCPM_SOFT_H
#define __CROS_EC_DRIVER_TCPM_SOFT_H

/*
 * Generic functions to enqueue and dequeue PD messages for
 * software implementation of TCPC. They can be overriden and if so, they must
 * call soft_* functions for ports controlled by this driver.
 */
__overridable int tcpm_has_pending_message(int port);
__overridable int tcpm_dequeue_message(int port, uint32_t *payload, int *head);
__overridable int tcpm_enqueue_message(int port);
__overridable void tcpm_clear_pending_messages(int port);

/*
 * Function to enqueue and dequeue PD messages for software implementation of 
 * TCPC. They must be called manually if tcpm_* functions are overriden.
 */
int soft_tcpm_has_pending_message(int port);
int soft_tcpm_dequeue_message(int port, uint32_t *payload, int *head);
int soft_tcpm_enqueue_message(int port);
void soft_tcpm_clear_pending_messages(int port);

extern const struct tcpm_drv soft_tcpm_drv;

#endif /* __CROS_EC_DRIVER_TCPM_SOFT_H */
