/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef FFT_H
#define FFT_H

enum fft_size {
    FFT_256=256,
    FFT_512=512,
    FFT_1024=1024,
    FFT_2048=2048,
};
int ams_rfft(int16_t *data, enum fft_size size);
void ams_get_magnitude(int16_t *data, uint16_t *buffer, uint16_t size);

#endif /* FFT_H */
