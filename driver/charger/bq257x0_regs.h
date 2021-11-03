/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI bq257x0 battery charger driver.
 */

#ifndef __CROS_EC_BQ257X0_REGS_H
#define __CROS_EC_BQ257X0_REGS_H

#include "bq25710.h"

/* ChargeOption1 Register (0x30) */
#define BQ257X0_CHARGE_OPTION_1_CMP_REF_SHIFT	7
#define BQ257X0_CHARGE_OPTION_1_CMP_REF_BITS	1
#define BQ257X0_CHARGE_OPTION_1_CMP_REF__2P3	0
#define BQ257X0_CHARGE_OPTION_1_CMP_REF__1P2	1

/* ChargeOption2 Register (0x31) */
#define BQ257X0_CHARGE_OPTION_2_EN_ACOC_SHIFT		3
#define BQ257X0_CHARGE_OPTION_2_EN_ACOC_BITS		1
#define BQ257X0_CHARGE_OPTION_2_EN_ACOC__DISABLE	0
#define BQ257X0_CHARGE_OPTION_2_EN_ACOC__ENABLE		1

/* ChargeCurrent Register */
#define BQ257X0_CHARGE_CURRENT_CHARGE_CURRENT_SHIFT	6
#define BQ257X0_CHARGE_CURRENT_CHARGE_CURRENT_BITS	7

#define BQ257X0_CHARGE_CURRENT_MASK	BQ257X0_MASK_(BQ257X0, \
						CHARGE_CURRENT, \
						CHARGE_CURRENT)

#define BQ257X0_MASK_(_chip, _reg, _field)				\
	GENMASK(							\
		(_chip##_##_reg##_##_field##_SHIFT +			\
		 _chip##_##_reg##_##_field##_BITS - 1),			\
		_chip##_##_reg##_##_field##_SHIFT)

#define BQ257X0_MASK(_reg, _field)	BQ257X0_MASK_(BQ257X0, _reg, _field)
#define BQ25720_MASK(_reg, _field)	BQ257X0_MASK_(BQ25720, _reg, _field)

#define GET_BQ25720(_reg, _field, _x)				\
	(((_x) >> BQ25720_##_reg##_##_field##_SHIFT) &		\
	 GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0))

#define SET_BQ25720(_reg, _field, _v, _x)				\
	(((_x) & ~BQ25720_MASK(_reg, _field)) |				\
	 (((_v) &							\
	   GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25720_##_reg##_##_field##_SHIFT))

#define SET_BQ257X0_BY_NAME(_reg, _field, _e, _x)			\
	(((_x) & ~BQ257X0_MASK(_reg, _field)) |				\
	 ((BQ257X0_##_reg##_##_field##__##_e &				\
	   GENMASK(BQ257X0_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ257X0_##_reg##_##_field##_SHIFT))

#define SET_BQ25720_BY_NAME(_reg, _field, _e, _x)			\
	(((_x) & ~BQ25720_MASK(_reg, _field)) |				\
	 ((BQ25720_##_reg##_##_field##__##_e &				\
	   GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25720_##_reg##_##_field##_SHIFT))

#define SET_CO1_BY_NAME(_f, _e, _x)	SET_BQ257X0_BY_NAME(CHARGE_OPTION_1, \
							    _f, _e, (_x))

#define SET_CO2(_field, _v, _x)		SET_BQ257X0(CHARGE_OPTION_2, \
							    _field, _v, (_x))
#define SET_CO2_BY_NAME(_field, _e, _x)	SET_BQ257X0_BY_NAME(CHARGE_OPTION_2, \
							    _field, _e, (_x))

#define SET_CO4(_f, _v, _x)		SET_BQ25720(CHARGE_OPTION_4, _f, _v, (_x))
#define SET_CO4_BY_NAME(_f, _e, _x)	SET_BQ25720_BY_NAME(CHARGE_OPTION_4, _f, _e, (_x))




#endif /* __CROS_EC_BQ257X0_REGS_H */
