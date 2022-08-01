/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __TCS3410_FD_H__
#define __TCS3410_FD_H__


#define START_OF_FD_DATA          (31)
#define FFT_MAX_SAMPLE_SIZE      (2048) /* limitation - arbitrary */
#define FD_PACKET_COMPRESSED_SZ    (7) /* MOD_FD_FIFO_DATA7_WIDTH + 1(base 0) + 1(HW bit flag) */

ams_errno_t process_fd_data(volatile ams_current_state_t *pcurr_state, uint8_t *pbuffer, uint32_t len);


#endif /* __TCS3410_FD_H__ */

