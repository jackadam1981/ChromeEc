/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI bq257x0 battery charger driver.
 */

#ifndef __CROS_EC_BQ257X0_REGS_H
#define __CROS_EC_BQ257X0_REGS_H

#include "bq25710.h"




#define BQ257X0_MASK(_chip, _reg, _field)				\
	GENMASK(							\
		(_chip##_##_reg##_##_field##_SHIFT +			\
		 _chip##_##_reg##_##_field##_BITS - 1),			\
		_chip##_##_reg##_##_field##_SHIFT)

#define BQ25720_MASK(_reg, _field)	BQ257X0_MASK(BQ25720, _reg, _field)

#define GET_BQ25720(_reg, _field, _x)				\
	(((_x) >> BQ25720_##_reg##_##_field##_SHIFT) &		\
	 GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0))

#define SET_BQ25720(_reg, _field, _v, _x)				\
	(((_x) & ~BQ25720_MASK(_reg, _field)) |				\
	 (((_v) &							\
	   GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25720_##_reg##_##_field##_SHIFT))

#define SET_BQ25720_BY_NAME(_reg, _field, _e, _x)			\
	(((_x) & ~BQ25720_MASK(_reg, _field)) |				\
	 ((BQ25720_##_reg##_##_field##__##_e &				\
	   GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25720_##_reg##_##_field##_SHIFT))


#define SET_CO4(_f, _v, _x)		SET_BQ25720(CHARGE_OPTION_4, _f, _v, (_x))
#define SET_CO4_BY_NAME(_f, _e, _x)	SET_BQ25720_BY_NAME(CHARGE_OPTION_4, _f, _e, (_x))




#endif /* __CROS_EC_BQ257X0_REGS_H */
