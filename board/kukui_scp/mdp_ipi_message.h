/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _MDP_IPI_MESSAGE_H
#define _MDP_IPI_MESSAGE_H

struct mdp_msg_service {
    int id;
    unsigned char msg[20];
};

void mdp_common_init(void);
void mdp_ipi_task_handler(void *pvParameters);

#endif  // _MDP_IPI_MESSAGE_H